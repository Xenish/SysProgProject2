#include "process.h"
#include "orchestrator.h"
#include "protocol.h"
#include "workspace.h"
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

volatile sig_atomic_t child_changed;
static void on_child(int sig) { (void)sig; child_changed = 1; }
int process_init(sigset_t *original) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_child;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGCHLD, &action, NULL)) return -1;
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGCHLD);
    return sigprocmask(SIG_BLOCK, &blocked, original);
}
static void error_task(Task *t, const char *message) {
    copy_field(t->state, sizeof(t->state), "error");
    copy_field(t->message, sizeof(t->message), message);
    if (save_status(t)) perror("status publish");
}
int launch(Agency *a, Task *t, char **argv) {
    (void)a;
    char path[FIELD];
    if (task_path(path, sizeof(path), "logs", t->id, "log")) return -1;
    int logfd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (logfd < 0) { error_task(t, "cannot create log"); return -1; }
    int error_pipe[2];
    if (pipe(error_pipe)) { close(logfd); error_task(t, "pipe failed"); return -1; }
    if (fcntl(error_pipe[1], F_SETFD, FD_CLOEXEC)) {
        close(logfd); close(error_pipe[0]); close(error_pipe[1]); error_task(t, "fcntl failed"); return -1;
    }
    pid_t pid = fork();
    if (pid == 0) {
        close(error_pipe[0]);
        int child_errno = 0;
        if (dup2(logfd, STDOUT_FILENO) < 0 || dup2(logfd, STDERR_FILENO) < 0) child_errno = errno;
        close(logfd);
        sigset_t empty;
        sigemptyset(&empty);
        if (sigprocmask(SIG_SETMASK, &empty, NULL)) child_errno = errno;
        if (!child_errno) execvp(argv[0], argv);
        if (!child_errno) child_errno = errno;
        ssize_t sent;
        do { sent = write(error_pipe[1], &child_errno, sizeof(child_errno)); } while (sent < 0 && errno == EINTR);
        _exit(127);
    }
    close(logfd); close(error_pipe[1]);
    if (pid < 0) { close(error_pipe[0]); error_task(t, "fork failed"); return -1; }
    t->pid = pid;
    t->child = true;
    int child_errno = 0;
    ssize_t n;
    do { n = read(error_pipe[0], &child_errno, sizeof(child_errno)); } while (n < 0 && errno == EINTR);
    close(error_pipe[0]);
    if (n != 0) {
        char msg[FIELD];
        snprintf(msg, sizeof(msg), "exec/child setup failed: %s", n < 0 ? "error pipe read" : strerror(child_errno));
        error_task(t, msg);
    } else {
        copy_field(t->state, sizeof(t->state), "running");
        if (save_status(t)) { perror("status publish"); return -1; }
    }
    printf("Task submitted: %s (agent: %s, pid %ld)\n", t->id, t->agent, (long)pid);
    return n == 0 ? 0 : -1;
}
static void completed(Agency *a, pid_t pid, int status) {
    for (size_t i = 0; i < a->count; ++i) {
        Task *t = &a->tasks[i];
        if (!t->child || t->pid != pid) continue;
        t->child = false;
        if (!strcmp(t->state, "error")) return;
        if (WIFEXITED(status)) {
            copy_field(t->state, sizeof(t->state), "done");
            t->exit_code = WEXITSTATUS(status);
            if (save_status(t)) perror("status publish");
        } else {
            char msg[FIELD];
            snprintf(msg, sizeof(msg), "child terminated by signal %d", WIFSIGNALED(status) ? WTERMSIG(status) : 0);
            error_task(t, msg);
        }
        return;
    }
}
static void wait_failure(Agency *a) {
    for (size_t i = 0; i < a->count; ++i) if (a->tasks[i].child) {
        a->tasks[i].child = false;
        error_task(&a->tasks[i], "waitpid failed: exit status unavailable");
    }
}
void reap(Agency *a) {
    child_changed = 0;
    for (;;) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) completed(a, pid, status);
        else if (pid < 0 && errno == EINTR) continue;
        else { if (pid < 0 && errno != ECHILD) wait_failure(a); break; }
    }
}
void wait_task(Agency *a, Task *t) {
    while (!t->child && !strcmp(t->state, "running")) {
        monitor_recovered(a);
        reap(a);
        if (!strcmp(t->state, "running")) {
            struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000};
            nanosleep(&delay, NULL);
        }
    }
    while (t->child) {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid > 0) completed(a, pid, status);
        else if (errno != EINTR) { wait_failure(a); break; }
    }
}
void wait_children(Agency *a) {
    for (size_t i = 0; i < a->count; ++i) if (a->tasks[i].child) wait_task(a, &a->tasks[i]);
}
