#include "agent_common.h"
#include <dirent.h>
#include <limits.h>
#include <string.h>

static int consumed(const char *name) {
    DIR *dir = opendir("reports");
    if (!dir) return -1;
    struct dirent *e;
    int result = 0;
    while ((e = readdir(dir))) {
        size_t len = strlen(e->d_name);
        if (len < 7 || strcmp(e->d_name + len - 7, ".report")) continue;
        char path[FIELD];
        int n = snprintf(path, sizeof(path), "reports/%s", e->d_name);
        Document doc;
        if (n < 0 || (size_t)n >= sizeof(path) || read_document(path, &doc)) { result = -1; break; }
        if (!strcmp(value(&doc, "agent"), "reporter") && !strcmp(value(&doc, "status"), "success") && !strcmp(value(&doc, "source_message"), name)) { result = 1; break; }
    }
    if (closedir(dir)) return -1;
    return result;
}
static int oldest_message(char *name, size_t size, Document *message) {
    DIR *dir = opendir("messages");
    if (!dir) return -1;
    struct dirent *e;
    unsigned long oldest = ULONG_MAX;
    int result = 1;
    while ((e = readdir(dir))) {
        unsigned long seq;
        if (message_number(e->d_name, &seq)) continue;
        char path[FIELD];
        int n = snprintf(path, sizeof(path), "messages/%s", e->d_name);
        Document doc;
        if (n < 0 || (size_t)n >= sizeof(path) || read_document(path, &doc)) { result = -1; break; }
        if (strcmp(value(&doc, "to"), "reporter")) continue;
        int used = consumed(e->d_name);
        if (used < 0) { result = -1; break; }
        if (used || seq >= oldest) continue;
        if (copy_field(name, size, e->d_name)) { result = -1; break; }
        oldest = seq;
        *message = doc;
        result = 0;
    }
    if (closedir(dir)) return -1;
    return result;
}
int main(int argc, char **argv) {
    if (agent_args(argc, argv)) return 2;
    char name[FIELD];
    Document message, source;
    int found = oldest_message(name, sizeof(name), &message);
    if (found) return agent_failure("reporter", argv[1], argv[2], found < 0 ? "mailbox/report scan failed" : "no unprocessed reporter message", found < 0 ? 3 : 1);
    const char *source_id = value(&message, "task");
    char path[FIELD];
    if (!valid_id(source_id) || task_path(path, sizeof(path), "reports", source_id, "report")) return agent_failure("reporter", argv[1], argv[2], "invalid source task", 2);
    if (read_document(path, &source)) return agent_failure("reporter", argv[1], argv[2], "source report unavailable", 1);
    if (strcmp(value(&source, "status"), "success") || strcmp(value(&source, "agent"), value(&message, "from"))) return agent_failure("reporter", argv[1], argv[2], "source report not successful or sender mismatch", 2);
    FILE *fp = atomic_begin(argv[3]);
    if (!fp) return agent_failure("reporter", argv[1], argv[2], "briefing open failed", 3);
    fprintf(fp, "Agency briefing\nSource task: %s\nSource message: %s\n", source_id, name);
    for (size_t i = 0; i < source.count; ++i) fprintf(fp, "%s: %s\n", source.key[i], source.value[i]);
    if (atomic_finish(fp, argv[3])) return agent_failure("reporter", argv[1], argv[2], "briefing publish failed", 3);
    char report[LINE];
    int n = snprintf(report, sizeof(report), "agent: reporter\ninput: %s\nsource_task: %s\nsource_message: %s\nbriefing: %s\nstatus: success\n", argv[2], source_id, name, argv[3]);
    if (n < 0 || (size_t)n >= sizeof(report) || agent_report(argv[1], report)) return 3;
    puts("Reporter published briefing");
    return 0;
}
