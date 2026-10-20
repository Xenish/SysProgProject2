#include "agent_common.h"
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    if (agent_args(argc, argv)) return 2;
    FILE *input = fopen(argv[2], "r");
    if (!input) return agent_failure("scout", argv[1], argv[2], "cannot read input", 3);
    FILE *output = atomic_begin(argv[3]);
    if (!output) { fclose(input); return agent_failure("scout", argv[1], argv[2], "cannot open output", 3); }
    char *line = NULL;
    size_t cap = 0, rows = 0, matches = 0;
    ssize_t len;
    int code = 0;
    while ((len = getline(&line, &cap, input)) >= 0) {
        if (memchr(line, '\0', (size_t)len)) { code = 2; break; }
        ++rows;
        if (strstr(line, "ERROR")) {
            ++matches;
            if (fwrite(line, 1, (size_t)len, output) != (size_t)len) { code = 3; break; }
        }
    }
    if (ferror(input)) code = 3;
    free(line);
    if (fclose(input)) code = 3;
    if (code) { atomic_abort(output, argv[3]); return agent_failure("scout", argv[1], argv[2], "invalid text or I/O failure", code); }
    if (atomic_finish(output, argv[3])) return agent_failure("scout", argv[1], argv[2], "output publish failed", 3);
    char report[LINE];
    snprintf(report, sizeof(report), "agent: scout\ninput: %s\nrows_processed: %zu\nmatching_lines: %zu\nkeyword: ERROR\nstatus: success\n", argv[2], rows, matches);
    if (agent_report(argv[1], report)) return 3;
    printf("Scout found %zu matching lines out of %zu\n", matches, rows);
    return matches ? 0 : 1;
}
