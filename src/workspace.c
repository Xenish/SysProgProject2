#include "workspace.h"
#include "protocol.h"
#include "mailbox.h"
#include "orchestrator.h"
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int workspace_open(Agency *a) {
    const char *dirs[] = {"tasks", "status", "reports", "messages", "logs", "data"};
    for (size_t i = 0; i < sizeof(dirs)/sizeof(*dirs); ++i) {
        struct stat st;
        if (stat(dirs[i], &st) || !S_ISDIR(st.st_mode)) { fprintf(stderr, "Missing workspace directory: %s\n", dirs[i]); return -1; }
    }
    a->lock_fd = open(".agency.lock", O_CREAT | O_RDWR, 0600);
    if (a->lock_fd < 0) return -1;
    struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET};
    if (fcntl(a->lock_fd, F_SETFD, FD_CLOEXEC) || fcntl(a->lock_fd, F_SETLK, &lock)) {
        fprintf(stderr, "Workspace locked by another orchestrator\n"); close(a->lock_fd); return -1;
    }
    return 0;
}
int next_task_id(char *id, size_t size) {
    unsigned long max = 0;
    const char *dirs[] = {"tasks", "status"};
    const char *exts[] = {".task", ".status"};
    for (size_t d = 0; d < 2; ++d) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) return -1;
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (strncmp(entry->d_name, "task_", 5)) continue;
            errno = 0;
            char *end;
            unsigned long n = strtoul(entry->d_name + 5, &end, 10);
            if (!strcmp(end, exts[d])) {
                if (errno == ERANGE || n == ULONG_MAX) { closedir(dir); errno = EOVERFLOW; return -1; }
                if (n > max) max = n;
            }
        }
        if (closedir(dir)) return -1;
    }
    int n = snprintf(id, size, "task_%03lu", max + 1);
    if (n < 0 || (size_t)n >= size) { errno = EOVERFLOW; return -1; }
    return 0;
}

void monitor_recovered(Agency *a) {
    for (size_t i = 0; i < a->count; ++i) {
        Task *t = &a->tasks[i];
        if (!t->child && !strcmp(t->state, "running") && kill(t->pid, 0) < 0 && errno == ESRCH) {
            strcpy(t->state, "error");
            strcpy(t->message, "exit status unavailable after restart");
            if (save_status(t)) perror("recovery status");
        }
    }
}
int recover_workspace(Agency *a) {
    struct dirent **entries;
    int count = scandir("status", &entries, NULL, alphasort);
    if (count < 0) return -1;
    int bad = 0;
    for (int i = 0; i < count; ++i) {
        const char *name = entries[i]->d_name;
        size_t len = strlen(name);
        if (!bad && len > 7 && !strcmp(name + len - 7, ".status")) {
            char path[FIELD], taskfile[FIELD];
            Document doc;
            Task t;
            memset(&t, 0, sizeof(t));
            int n = snprintf(path, sizeof(path), "status/%s", name);
            if (n < 0 || (size_t)n >= sizeof(path) || read_document(path, &doc) ||
                !valid_id(value(&doc, "id")) || !*value(&doc, "agent") ||
                copy_field(t.id, sizeof(t.id), value(&doc, "id")) ||
                copy_field(t.agent, sizeof(t.agent), value(&doc, "agent")) ||
                copy_field(t.state, sizeof(t.state), value(&doc, "state")) ||
                copy_field(t.message, sizeof(t.message), value(&doc, "message"))) bad = 1;
            else {
                char expected[FIELD];
                snprintf(expected, sizeof(expected), "%s.status", t.id);
                char *end;
                errno = 0;
                long pid = strtol(value(&doc, "pid"), &end, 10);
                if (strcmp(expected, name) || errno || !*value(&doc, "pid") || *end || pid < 0 || pid > INT_MAX) bad = 1;
                t.pid = (pid_t)pid;
                if (!strcmp(t.state, "done")) {
                    errno = 0;
                    long code = strtol(value(&doc, "exit_code"), &end, 10);
                    if (errno || !*value(&doc, "exit_code") || *end || code < 0 || code > 255) bad = 1;
                    t.exit_code = (int)code;
                } else if (strcmp(t.state, "running") && strcmp(t.state, "error")) bad = 1;
                if (!strcmp(t.state, "running") && t.pid <= 0) bad = 1;
                t.collected = !strcmp(value(&doc, "collected"), "yes");
                t.canonical_created = !strcmp(value(&doc, "canonical_created"), "yes");
                if (!task_path(taskfile, sizeof(taskfile), "tasks", t.id, "task")) {
                    Task spec;
                    if (!read_task(taskfile, &spec)) {
                        strcpy(t.input, spec.input); strcpy(t.output, spec.output);
                    }
                }
                if (a->count == MAX_TASKS) bad = 1;
                if (!bad) a->tasks[a->count++] = t;
            }
            if (bad) fprintf(stderr, "Invalid persisted status: %s\n", name);
        }
        free(entries[i]);
    }
    free(entries);
    if (bad) { errno = EINVAL; return -1; }
    monitor_recovered(a);
    return 0;
}
