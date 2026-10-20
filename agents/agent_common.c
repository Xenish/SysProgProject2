#include "agent_common.h"
#include <string.h>
int agent_args(int argc, char **argv) {
    if (argc != 4 || !valid_id(argv[1]) || !*argv[2] || !*argv[3] ||
        strlen(argv[2]) >= FIELD || strlen(argv[3]) >= FIELD || !single_line(argv[2]) || !single_line(argv[3])) {
        fputs("Usage: agent <task_id> <input> <output>\n", stderr); return -1;
    }
    char report[FIELD];
    if (task_path(report, sizeof(report), "reports", argv[1], "report")) return -1;
    if (!strcmp(report, argv[3]) || !strcmp(argv[2], argv[3])) {
        fputs("Output must be separate from input and report\n", stderr); return -1;
    }
    return 0;
}
int agent_report(const char *id, const char *text) {
    char path[FIELD];
    if (task_path(path, sizeof(path), "reports", id, "report") || atomic_text(path, text)) {
        perror("agent report"); return -1;
    }
    return 0;
}
int agent_failure(const char *agent, const char *id, const char *input, const char *message, int code) {
    char text[LINE];
    int n = snprintf(text, sizeof(text), "agent: %s\ninput: %s\nstatus: failure\nmessage: %s\n", agent, input, message);
    fprintf(stderr, "%s: %s\n", agent, message);
    if (n < 0 || (size_t)n >= sizeof(text) || agent_report(id, text)) return 3;
    return code;
}
