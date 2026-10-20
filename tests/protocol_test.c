#include "protocol.h"
#include "cli.h"
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
    assert(atomic_text("atomic.txt", "original\n") == 0);
    struct rlimit original, restricted;
    assert(getrlimit(RLIMIT_FSIZE, &original) == 0);
    restricted = original;
    restricted.rlim_cur = 16;
    assert(signal(SIGXFSZ, SIG_IGN) != SIG_ERR);
    assert(setrlimit(RLIMIT_FSIZE, &restricted) == 0);
    char big[4096];
    memset(big, 'x', sizeof(big) - 1); big[sizeof(big) - 1] = '\0';
    assert(atomic_text("atomic.txt", big) == -1);
    assert(setrlimit(RLIMIT_FSIZE, &original) == 0);
    FILE *fp = fopen("atomic.txt", "r");
    assert(fp);
    char buf[32];
    assert(fgets(buf, sizeof(buf), fp)); assert(!strcmp(buf, "original\n")); assert(fclose(fp) == 0);
    assert(access("atomic.txt.tmp", F_OK) == -1);
    assert(symlink("atomic.txt", "atomic.txt.tmp") == 0);
    assert(atomic_text("atomic.txt", "must not follow symlink") == -1);
    assert(unlink("atomic.txt.tmp") == 0);
    assert(mkdir("directory", 0700) == 0);
    assert(atomic_text("directory", "rename must fail") == -1);
    assert(access("directory.tmp", F_OK) == -1);
    assert(atomic_text("bad.txt", "id: task_001\nid: task_002\n") == 0);
    Document doc;
    assert(read_document("bad.txt", &doc) == -1);
    fp = fopen("bad.txt", "w"); assert(fp);
    const char nul[] = "id: task_001\0injected: value\n";
    assert(fwrite(nul, 1, sizeof(nul) - 1, fp) == sizeof(nul) - 1); assert(fclose(fp) == 0);
    assert(read_document("bad.txt", &doc) == -1);
    char command[] = "send analyst reporter task_001 \"please \\\"brief\\\"\"";
    char *args[MAX_ARGS];
    assert(tokenize(command, args) == 5);
    assert(!strcmp(args[4], "please \"brief\""));
    char bad_quote[] = "run 'unterminated";
    assert(tokenize(bad_quote, args) == -1);
    unsigned long seq;
    assert(message_number("msg_1000.analyst.reporter.txt", &seq) == 0 && seq == 1000);
    assert(message_number("msg_999.analyst.reporter.txt.tmp", &seq) == -1);
    puts("PASS P7: atomic flush/rename/symlink failures preserve final; protocol NUL/duplicate and CLI quoting");
    return 0;
}
