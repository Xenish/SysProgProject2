/* Linked only into build/fault-agency: never present in the shipped core. */
#undef fork
#undef execvp
#undef waitpid
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int fault(const char *name) {
    const char *mode = getenv("AGENCY_TEST_FAULT");
    return mode && !strcmp(mode, name);
}
pid_t agency_test_fork(void) {
    if (fault("fork")) { errno = EAGAIN; return -1; }
    return fork();
}
int agency_test_execvp(const char *file, char *const argv[]) {
    if (fault("exec")) { errno = ENOENT; return -1; }
    return execvp(file, argv);
}
pid_t agency_test_waitpid(pid_t pid, int *status, int options) {
    static int failed;
    if (fault("waitpid") && !failed && options == 0) { failed = 1; errno = EIO; return -1; }
    return waitpid(pid, status, options);
}
