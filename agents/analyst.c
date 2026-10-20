#include "agent_common.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (agent_args(argc, argv)) return 2;
    const char *id = argv[1], *input = argv[2], *output = argv[3];
    FILE *fp = fopen(input, "r");
    if (!fp) return agent_failure("analyst", id, input, "cannot read input", 3);
    char *line = NULL;
    size_t capacity = 0, rows = 0;
    double sum = 0;
    int code = 0;
    const char *message = "invalid CSV (expected id,score and positive integer IDs with finite scores)";
    ssize_t n = getline(&line, &capacity, fp);
    if (n <= 0 || n >= LINE || memchr(line, '\0', n > 0 ? (size_t)n : 0)) code = 2;
    if (!code) {
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (strcmp(line, "id,score")) code = 2;
    }
    while (!code && (n = getline(&line, &capacity, fp)) >= 0) {
        if (n >= LINE || memchr(line, '\0', (size_t)n)) { code = 2; break; }
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        char *end;
        errno = 0;
        long row_id = strtol(line, &end, 10);
        if (errno || row_id <= 0 || end == line || *end != ',') { code = 2; break; }
        char *score_start = end + 1;
        errno = 0;
        double score = strtod(score_start, &end);
        if (errno || end == score_start || *end || !isfinite(score) || !isfinite(sum + score)) { code = 2; break; }
        sum += score;
        ++rows;
    }
    if (ferror(fp)) { code = 3; message = "input I/O failed"; }
    if (fclose(fp)) { code = 3; message = "input close failed"; }
    free(line);
    if (code) return agent_failure("analyst", id, input, message, code);
    if (!rows) return agent_failure("analyst", id, input, "no data rows", 1);
    char report[LINE], summary[FIELD];
    snprintf(summary, sizeof(summary), "Rows: %zu\nSum: %.10g\nAverage: %.2f\n", rows, sum, sum / (double)rows);
    if (atomic_text(output, summary)) return agent_failure("analyst", id, input, "output publish failed", 3);
    snprintf(report, sizeof(report), "agent: analyst\ninput: %s\nrows_processed: %zu\nsum: %.10g\naverage: %.2f\nstatus: success\n", input, rows, sum, sum / (double)rows);
    if (agent_report(id, report)) return 3;
    printf("Analyst processed %zu rows\n", rows);
    return 0;
}
