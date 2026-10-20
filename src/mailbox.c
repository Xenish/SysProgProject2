#include "mailbox.h"
#include "orchestrator.h"
#include "process.h"
#include "protocol.h"
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int read_task(const char *path, Task *t) {
    Document doc;
    memset(t, 0, sizeof(*t));
    if (read_document(path, &doc)) return -1;
    if (copy_field(t->id, sizeof(t->id), value(&doc, "id")) ||
        copy_field(t->agent, sizeof(t->agent), value(&doc, "agent")) ||
        copy_field(t->input, sizeof(t->input), value(&doc, "input")) ||
        copy_field(t->output, sizeof(t->output), value(&doc, "output"))) return -1;
    if (!valid_id(t->id) || !valid_name(t->agent) || !*t->input) { errno = EINVAL; return -1; }
    if (!*t->output && task_path(t->output, sizeof(t->output), "data", t->id, "out")) return -1;
    const char *priority = value(&doc, "priority");
    if (*priority) {
        char *end;
        errno = 0;
        (void)strtol(priority, &end, 10);
        if (errno || *end) { errno = EINVAL; return -1; }
    }
    return 0;
}
/* Resolve the existing parent, including symlinks and '..', before comparing paths. */
static int output_key(const char *path, char *key, size_t size) {
    char parent[FIELD], resolved[4096];
    if (!single_line(path) || copy_field(parent, sizeof(parent), path)) return -1;
    char *slash = strrchr(parent, '/');
    const char *base = slash ? slash + 1 : path;
    if (!*base || !strcmp(base, ".") || !strcmp(base, "..")) { errno = EINVAL; return -1; }
    char name[FIELD];
    if (copy_field(name, sizeof(name), base)) return -1;
    if (slash) { if (slash == parent) slash[1] = '\0'; else *slash = '\0'; }
    else strcpy(parent, ".");
    if (!realpath(parent, resolved) || access(resolved, W_OK)) return -1;
    int n = snprintf(key, size, "%s/%s", resolved, name);
    if (n < 0 || (size_t)n >= size) { errno = ENAMETOOLONG; return -1; }
    return 0;
}
int validate_task(const Task *t) {
    char program[FIELD], key[4096];
    struct stat st;
    int n = snprintf(program, sizeof(program), "./agents/%s", t->agent);
    if (n < 0 || (size_t)n >= sizeof(program) || stat(program, &st) || !S_ISREG(st.st_mode) || access(program, X_OK)) {
        fprintf(stderr, "Agent executable unavailable: %s\n", t->agent); return -1;
    }
    if (stat(t->input, &st) || !S_ISREG(st.st_mode) || access(t->input, R_OK)) {
        fprintf(stderr, "Input unreadable: %s\n", t->input); return -1;
    }
    if (output_key(t->output, key, sizeof(key))) { fprintf(stderr, "Invalid output path: %s\n", t->output); return -1; }
    if (lstat(t->output, &st) == 0 || errno != ENOENT) {
        fprintf(stderr, "Output already exists or inaccessible: %s\n", t->output); return -1;
    }
    char root[4096];
    if (!realpath(".", root)) return -1;
    const char *dirs[] = {"tasks/", "status/", "messages/", "logs/", "agents/", "src/", "include/"};
    const char *relative = key;
    if (!strncmp(key, root, strlen(root)) && key[strlen(root)] == '/') {
        relative = key + strlen(root) + 1;
        for (size_t i = 0; i < sizeof(dirs)/sizeof(*dirs); ++i)
            if (!strncmp(relative, dirs[i], strlen(dirs[i]))) { fprintf(stderr, "Output uses reserved directory\n"); return -1; }
        if (!strncmp(relative, "reports/", 8) && strstr(relative + 8, ".report")) {
            fprintf(stderr, "Output cannot overwrite an agent report\n"); return -1;
        }
    }
    size_t len = strlen(key);
    if (len >= 4 && !strcmp(key + len - 4, ".tmp")) { fprintf(stderr, "Output cannot use .tmp suffix\n"); return -1; }
    return 0;
}
static int same_task(const Task *x, const Task *y) {
    return !strcmp(x->id, y->id) && !strcmp(x->agent, y->agent) &&
        !strcmp(x->input, y->input) && !strcmp(x->output, y->output);
}
static int output_conflict(const Task *t) {
    char key[4096];
    if (output_key(t->output, key, sizeof(key))) return -1;
    DIR *dir = opendir("tasks");
    if (!dir) return -1;
    struct dirent *e;
    int conflict = 0;
    while ((e = readdir(dir))) {
        if (!strstr(e->d_name, ".task") || strstr(e->d_name, ".tmp")) continue;
        char path[FIELD], other_key[4096];
        Task other;
        int n = snprintf(path, sizeof(path), "tasks/%s", e->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) { conflict = 1; break; }
        if (!read_task(path, &other) && strcmp(t->id, other.id) &&
            !output_key(other.output, other_key, sizeof(other_key)) && !strcmp(key, other_key)) { conflict = 1; break; }
    }
    closedir(dir);
    if (conflict) fprintf(stderr, "Output owned by another task\n");
    return conflict ? -1 : 0;
}
void submit_task(Agency *a, const char *path) {
    Task t;
    if (read_task(path, &t)) { fprintf(stderr, "Invalid task file (id/agent/input required): %s\n", path); return; }
    char canonical[FIELD], status[FIELD];
    if (task_path(canonical, sizeof(canonical), "tasks", t.id, "task") || task_path(status, sizeof(status), "status", t.id, "status")) return;
    if (find_task(a, t.id) || access(status, F_OK) == 0) { fprintf(stderr, "Duplicate task id: %s\n", t.id); return; }
    if (a->count == MAX_TASKS) { fprintf(stderr, "Task table full\n"); return; }
    if (!strcmp(t.agent, "reporter")) {
        for (size_t i = 0; i < a->count; ++i) if (!strcmp(a->tasks[i].agent, "reporter") &&
            (a->tasks[i].child || !strcmp(a->tasks[i].state, "running"))) {
            fprintf(stderr, "A reporter is already running\n"); return;
        }
    }
    if (validate_task(&t) || output_conflict(&t)) return;
    struct stat st;
    if (lstat(canonical, &st) == 0) {
        Task existing;
        if (read_task(canonical, &existing) || !same_task(&t, &existing)) {
            fprintf(stderr, "Canonical task conflict: %s\n", canonical); return;
        }
    } else {
        if (errno != ENOENT) { perror("canonical task"); return; }
        Document doc;
        if (read_document(path, &doc)) { perror("task read"); return; }
        FILE *fp = atomic_begin(canonical);
        if (!fp) { perror("task publish"); return; }
        for (size_t i = 0; i < doc.count; ++i) fprintf(fp, "%s: %s\n", doc.key[i], doc.value[i]);
        if (atomic_finish(fp, canonical)) { perror("task publish"); return; }
    }
    Task *stored = &a->tasks[a->count++];
    *stored = t;
    char program[FIELD];
    snprintf(program, sizeof(program), "./agents/%s", t.agent);
    char *args[] = {program, stored->id, stored->input, stored->output, NULL};
    launch(a, stored, args);
}
void show_tasks(Agency *a) {
    puts("ID AGENT STATE");
    for (size_t i = 0; i < a->count; ++i) printf("%s %s %s\n", a->tasks[i].id, a->tasks[i].agent, a->tasks[i].state);
    DIR *dir = opendir("tasks");
    if (!dir) { perror("tasks"); return; }
    struct dirent *e;
    while ((e = readdir(dir))) {
        size_t len = strlen(e->d_name);
        if (len < 5 || strcmp(e->d_name + len - 5, ".task")) continue;
        char path[FIELD];
        int n = snprintf(path, sizeof(path), "tasks/%s", e->d_name);
        Task t;
        if (n > 0 && (size_t)n < sizeof(path) && !read_task(path, &t) && !find_task(a, t.id)) printf("%s %s PENDING\n", t.id, t.agent);
    }
    closedir(dir);
}
