#include "agency.h"
#include "cli.h"
#include "orchestrator.h"
#include "process.h"
#include "workspace.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

int main(void) {
    Agency *a = calloc(1, sizeof(*a));
    if (!a) { perror("allocate agency"); return 1; }
    if (workspace_open(a)) { perror("workspace"); free(a); return 1; }
    if (recover_workspace(a)) { perror("recover workspace"); close(a->lock_fd); free(a); return 1; }
    sigset_t original;
    if (process_init(&original)) { perror("signals"); close(a->lock_fd); free(a); return 1; }
    char line[LINE];
    size_t used = 0;
    int overflow = 0, exiting = 0;
    int interactive = isatty(STDIN_FILENO);
    if (interactive) { fputs("agency> ", stdout); fflush(stdout); }
    while (!exiting) {
        reap(a);
        monitor_recovered(a);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timespec timeout = {.tv_sec = 0, .tv_nsec = 250000000};
        int ready = pselect(STDIN_FILENO + 1, &fds, NULL, NULL, &timeout, &original);
        if (ready < 0) { if (errno == EINTR) continue; perror("pselect"); break; }
        if (!ready) continue;
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n < 0) { if (errno == EINTR) continue; perror("stdin"); break; }
        if (n == 0 && used == 0 && !overflow) break;
        if (n == 0 || c == '\n') {
            line[used] = '\0';
            if (overflow) fputs("Command line too long or invalid control character\n", stderr);
            else {
                char *argv[MAX_ARGS];
                int argc = tokenize(line, argv);
                if (argc >= 0) exiting = dispatch(a, argc, argv);
            }
            used = 0; overflow = 0;
            fflush(stdout); fflush(stderr);
            if (n == 0) break;
            if (interactive && !exiting) { fputs("agency> ", stdout); fflush(stdout); }
        } else if (c == '\0' || ((unsigned char)c < 32 && c != '\t') || (unsigned char)c == 127) overflow = 1;
        else if (used + 1 < sizeof(line)) line[used++] = c;
        else overflow = 1;
    }
    wait_children(a);
    close(a->lock_fd);
    free(a);
    return 0;
}
