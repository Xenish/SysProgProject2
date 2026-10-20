#include "advisor.h"
#include "mailbox.h"
#include "orchestrator.h"
#include "protocol.h"
#include <dirent.h>
#include <string.h>
#include <unistd.h>

static int source_has_message(const char *id) {
    DIR *dir = opendir("messages");
    if (!dir) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(dir))) {
        unsigned long seq;
        if (message_number(e->d_name, &seq)) continue;
        char path[FIELD];
        Document doc;
        int n = snprintf(path, sizeof(path), "messages/%s", e->d_name);
        if (n > 0 && (size_t)n < sizeof(path) && !read_document(path, &doc) &&
            !strcmp(value(&doc, "task"), id) && !strcmp(value(&doc, "to"), "reporter")) { found = 1; break; }
    }
    closedir(dir);
    return found;
}
static int source_has_briefing(const char *id) {
    DIR *dir = opendir("reports");
    if (!dir) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(dir))) {
        size_t len = strlen(e->d_name);
        if (len < 7 || strcmp(e->d_name + len - 7, ".report")) continue;
        char path[FIELD];
        Document doc;
        int n = snprintf(path, sizeof(path), "reports/%s", e->d_name);
        if (n > 0 && (size_t)n < sizeof(path) && !read_document(path, &doc) &&
            !strcmp(value(&doc, "agent"), "reporter") && !strcmp(value(&doc, "status"), "success") &&
            !strcmp(value(&doc, "source_task"), id) && access(value(&doc, "briefing"), R_OK) == 0) { found = 1; break; }
    }
    closedir(dir);
    return found;
}
static int pending_reporter(Agency *a) {
    DIR *dir = opendir("tasks");
    if (!dir) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(dir))) {
        size_t len = strlen(e->d_name);
        if (len < 5 || strcmp(e->d_name + len - 5, ".task")) continue;
        char path[FIELD];
        Task t;
        int n = snprintf(path, sizeof(path), "tasks/%s", e->d_name);
        if (n > 0 && (size_t)n < sizeof(path) && !read_task(path, &t) && !strcmp(t.agent, "reporter") &&
            !find_task(a, t.id) && !validate_task(&t)) { printf("submit %s\n", path); found = 1; break; }
    }
    closedir(dir);
    return found;
}
void suggest(Agency *a) {
    for (size_t i = 0; i < a->count; ++i) {
        Task *t = &a->tasks[i];
        if (!strcmp(t->state, "error") || (!strcmp(t->state, "done") && t->exit_code)) {
            printf("log %s; fix the cause, then create a task with a new ID and submit it.\n", t->id); return;
        }
    }
    for (size_t i = 0; i < a->count; ++i) if (!strcmp(a->tasks[i].state, "running")) {
        printf("tasks; status %s\n", a->tasks[i].id); return;
    }
    for (size_t i = 0; i < a->count; ++i) {
        Task *t = &a->tasks[i];
        char path[FIELD];
        if (!strcmp(t->state, "done") && !t->collected && !task_path(path, sizeof(path), "reports", t->id, "report") && access(path, R_OK) == 0) {
            printf("collect %s\n", t->id); return;
        }
    }
    int awaiting_reporter = 0;
    for (size_t i = 0; i < a->count; ++i) {
        Task *t = &a->tasks[i];
        char path[FIELD];
        Document report;
        if (strcmp(t->agent, "analyst") || strcmp(t->state, "done") || t->exit_code ||
            task_path(path, sizeof(path), "reports", t->id, "report") || read_document(path, &report) ||
            strcmp(value(&report, "status"), "success") || source_has_briefing(t->id)) continue;
        if (!source_has_message(t->id)) { printf("send analyst reporter %s \"please brief\"\n", t->id); return; }
        awaiting_reporter = 1;
    }
    if (awaiting_reporter && pending_reporter(a)) return;
    if (!a->count) puts("submit tasks/task_001.task");
    else puts("No pending suggestions.");
}
