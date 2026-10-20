#include "orchestrator.h"
#include "process.h"
#include "protocol.h"
#include "workspace.h"
#include "mailbox.h"
#include <stdio.h>
#include <string.h>

Task *find_task(Agency *a, const char *id) {
    for (size_t i = 0; i < a->count; ++i) if (!strcmp(a->tasks[i].id, id)) return &a->tasks[i];
    return NULL;
}
int save_status(const Task *t) {
    char path[FIELD];
    if (task_path(path, sizeof(path), "status", t->id, "status")) return -1;
    FILE *fp = atomic_begin(path);
    if (!fp) return -1;
    fprintf(fp, "id: %s\nagent: %s\npid: %ld\nstate: %s\ncollected: %s\n", t->id, t->agent, (long)t->pid, t->state, t->collected ? "yes" : "no");
    if (!strcmp(t->state, "done")) fprintf(fp, "exit_code: %d\nreport: reports/%s.report\n", t->exit_code, t->id);
    if (!strcmp(t->state, "error")) fprintf(fp, "message: %s\n", t->message);
    return atomic_finish(fp, path);
}
void show_report(Task *t) {
    char path[FIELD];
    if (task_path(path, sizeof(path), "reports", t->id, "report")) return;
    if (print_file(path)) { printf("%s: no report produced\n", t->id); return; }
    t->collected = true;
    if (save_status(t)) perror("status publish");
}
int dispatch(Agency *a, int argc, char **argv) {
    if (!argc) return 0;
    reap(a);
    monitor_recovered(a);
    const char *cmd = argv[0];
    if (!strcmp(cmd, "exit") && argc == 1) return 1;
    if (!strcmp(cmd, "help") && argc == 1) {
        puts("submit <task_file> | tasks | collect <task_id> | run <program> [args...] | list | status/wait/report/log <task_id> | exit | help");
    } else if (!strcmp(cmd, "run") && argc >= 2) {
        if (a->count == MAX_TASKS) { fputs("Task table full\n", stderr); return 0; }
        Task *t = &a->tasks[a->count];
        memset(t, 0, sizeof(*t));
        if (next_task_id(t->id, sizeof(t->id)) || !single_line(argv[1]) || copy_field(t->agent, sizeof(t->agent), argv[1])) { perror("run validation"); return 0; }
        ++a->count;
        launch(a, t, argv + 1);
    } else if (!strcmp(cmd, "submit") && argc == 2) {
        submit_task(a, argv[1]);
    } else if (!strcmp(cmd, "tasks") && argc == 1) {
        show_tasks(a);
    } else if (!strcmp(cmd, "list") && argc == 1) {
        puts("ID AGENT PID STATE");
        for (size_t i = 0; i < a->count; ++i) printf("%s %s %ld %s\n", a->tasks[i].id, a->tasks[i].agent, (long)a->tasks[i].pid, a->tasks[i].state);
    } else if (argc == 2 && (!strcmp(cmd, "status") || !strcmp(cmd, "wait") || !strcmp(cmd, "report") || !strcmp(cmd, "log") || !strcmp(cmd, "collect"))) {
        Task *t = find_task(a, argv[1]);
        if (!t) { fprintf(stderr, "Unknown task: %s\n", argv[1]); return 0; }
        if (!strcmp(cmd, "collect")) { wait_task(a, t); show_report(t); }
        else if (!strcmp(cmd, "wait")) wait_task(a, t);
        else if (!strcmp(cmd, "report")) show_report(t);
        else {
            char path[FIELD];
            int log = !strcmp(cmd, "log");
            if (task_path(path, sizeof(path), log ? "logs" : "status", t->id, log ? "log" : "status") || print_file(path)) perror("read task file");
        }
    } else fprintf(stderr, "Unknown command or invalid arguments; use help\n");
    return 0;
}
