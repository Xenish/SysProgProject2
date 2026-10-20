#include "workspace.h"
#include "protocol.h"
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
