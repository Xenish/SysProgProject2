#include "protocol.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Only remove protocol-generated names. Never recursively erase a workspace. */
static int remove_file(const char *path) {
    if (unlink(path) && errno != ENOENT) { perror(path); return -1; }
    return 0;
}
static int remove_pair(const char *dir, const char *id, const char *ext) {
    char path[FIELD + 4], base[FIELD];
    if (task_path(base, sizeof(base), dir, id, ext)) return -1;
    int result = remove_file(base);
    int n = snprintf(path, sizeof(path), "%s.tmp", base);
    if (n < 0 || (size_t)n >= sizeof(path)) return -1;
    if (remove_file(path)) result = -1;
    return result;
}
static int clean_dir(const char *dir, const char *ext) {
    DIR *dp = opendir(dir);
    if (!dp) return -1;
    struct dirent *e;
    int result = 0;
    while ((e = readdir(dp))) {
        char id[IDLEN];
        const char *dot = strchr(e->d_name, '.');
        if (!dot || (size_t)(dot - e->d_name) >= sizeof(id)) continue;
        memcpy(id, e->d_name, (size_t)(dot - e->d_name));
        id[dot - e->d_name] = '\0';
        if (!valid_id(id) || (strcmp(dot + 1, ext) && !(strlen(dot + 1) == strlen(ext) + 4 && !strncmp(dot + 1, ext, strlen(ext)) && !strcmp(dot + 1 + strlen(ext), ".tmp")))) continue;
        if (!strcmp(dir, "status") && !strcmp(dot + 1, "status")) {
            char path[FIELD];
            Document doc;
            if (!task_path(path, sizeof(path), dir, id, ext) && !read_document(path, &doc)) {
                if (!strcmp(value(&doc, "canonical_created"), "yes") && strcmp(id, "task_001") && strcmp(id, "task_002") && strcmp(id, "task_003"))
                    if (remove_pair("tasks", id, "task")) result = -1;
            }
        }
        if (remove_pair(dir, id, ext)) result = -1;
    }
    if (closedir(dp)) result = -1;
    return result;
}
int main(void) {
    int fd = open(".agency.lock", O_CREAT | O_RDWR, 0600);
    if (fd < 0) { perror("clean lock"); return 1; }
    struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET};
    if (fcntl(fd, F_SETLK, &lock)) { fputs("Cannot clean a locked workspace\n", stderr); close(fd); return 1; }
    /* An orphan from a crashed orchestrator may still own an output file. */
    DIR *statuses = opendir("status");
    if (!statuses) { close(fd); return 1; }
    struct dirent *e;
    int result = 0;
    while ((e = readdir(statuses))) {
        size_t len = strlen(e->d_name);
        if (len < 7 || strcmp(e->d_name + len - 7, ".status")) continue;
        char path[FIELD];
        Document doc;
        int n = snprintf(path, sizeof(path), "status/%s", e->d_name);
        if (n < 0 || (size_t)n >= sizeof(path) || read_document(path, &doc)) { result = 1; break; }
        if (!strcmp(value(&doc, "state"), "running")) {
            char *end;
            long pid;
            errno = 0;
            pid = strtol(value(&doc, "pid"), &end, 10);
            if (errno || *end || pid <= 0 || pid > 2147483647L || kill((pid_t)pid, 0) == 0 || errno == EPERM) {
                fputs("Cannot clean while a recorded process may be alive\n", stderr); result = 1; break;
            }
        }
    }
    if (closedir(statuses)) result = 1;
    if (result) { close(fd); return 1; }
    if (clean_dir("status", "status") || clean_dir("reports", "report") || clean_dir("reports", "briefing") || clean_dir("logs", "log") || clean_dir("data", "out")) result = 1;
    DIR *messages = opendir("messages");
    if (!messages) result = 1;
    else {
        while ((e = readdir(messages))) {
            char name[FIELD], path[FIELD];
            if (copy_field(name, sizeof(name), e->d_name)) { result = 1; continue; }
            size_t len = strlen(name);
            if (len > 4 && !strcmp(name + len - 4, ".tmp")) name[len - 4] = '\0';
            unsigned long seq;
            if (message_number(name, &seq)) continue;
            int n = snprintf(path, sizeof(path), "messages/%s", e->d_name);
            if (n < 0 || (size_t)n >= sizeof(path) || remove_file(path)) result = 1;
        }
        if (closedir(messages)) result = 1;
    }
    close(fd); /* Do not unlink: replacing a lock inode allows competing owners. */
    return result;
}
