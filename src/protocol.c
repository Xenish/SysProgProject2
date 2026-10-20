#include "protocol.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int copy_field(char *dst, size_t size, const char *src) {
    if (strlen(src) >= size) { errno = EOVERFLOW; return -1; }
    memcpy(dst, src, strlen(src) + 1);
    return 0;
}
int single_line(const char *s) {
    for (; *s; ++s) if ((unsigned char)*s < 32 || (unsigned char)*s == 127) return 0;
    return 1;
}
int valid_name(const char *s) {
    if (!*s) return 0;
    for (; *s; ++s) if (!isalnum((unsigned char)*s) && *s != '_' && *s != '-') return 0;
    return 1;
}
int valid_id(const char *s) {
    if (strncmp(s, "task_", 5) || strlen(s) < 8 || strlen(s) >= IDLEN) return 0;
    for (s += 5; *s; ++s) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}
int task_path(char *dst, size_t size, const char *dir, const char *id, const char *ext) {
    int n = snprintf(dst, size, "%s/%s.%s", dir, id, ext);
    if (n < 0 || (size_t)n >= size) { errno = ENAMETOOLONG; return -1; }
    return 0;
}
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) ++s;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) --end;
    *end = '\0';
    return s;
}
int read_document(const char *path, Document *doc) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    memset(doc, 0, sizeof(*doc));
    char line[LINE];
    int result = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (!strchr(line, '\n') && !feof(fp)) { result = -1; break; }
        char *s = trim(line);
        if (!*s) continue;
        char *sep = strchr(s, ':');
        if (!sep) { result = -1; break; }
        *sep++ = '\0';
        char *key = trim(s), *val = trim(sep);
        if (!*key || !single_line(val) || doc->count == MAX_KEYS) { result = -1; break; }
        for (size_t i = 0; i < doc->count; ++i)
            if (!strcmp(doc->key[i], key)) result = -1;
        if (result || copy_field(doc->key[doc->count], 64, key) ||
            copy_field(doc->value[doc->count], FIELD, val)) { result = -1; break; }
        ++doc->count;
    }
    if (ferror(fp)) result = -1;
    if (fclose(fp)) result = -1;
    if (result) errno = EINVAL;
    return result;
}
const char *value(const Document *doc, const char *key) {
    for (size_t i = 0; i < doc->count; ++i) if (!strcmp(key, doc->key[i])) return doc->value[i];
    return "";
}
static int temp_path(char *tmp, size_t size, const char *path) {
    int n = snprintf(tmp, size, "%s.tmp", path);
    if (n < 0 || (size_t)n >= size) { errno = ENAMETOOLONG; return -1; }
    return 0;
}
FILE *atomic_begin(const char *path) {
    char tmp[FIELD + 4];
    if (temp_path(tmp, sizeof(tmp), path)) return NULL;
    return fopen(tmp, "w");
}
int atomic_finish(FILE *fp, const char *path) {
    char tmp[FIELD + 4];
    if (temp_path(tmp, sizeof(tmp), path)) { fclose(fp); return -1; }
    int bad = ferror(fp);
    if (fflush(fp)) bad = 1;
    if (fclose(fp)) bad = 1;
    if (!bad && rename(tmp, path) == 0) return 0;
    int saved = errno;
    unlink(tmp);
    errno = saved ? saved : EIO;
    return -1;
}
void atomic_abort(FILE *fp, const char *path) {
    char tmp[FIELD + 4];
    if (fp) fclose(fp);
    if (!temp_path(tmp, sizeof(tmp), path)) unlink(tmp);
}
int atomic_text(const char *path, const char *text) {
    FILE *fp = atomic_begin(path);
    if (!fp) return -1;
    if (fputs(text, fp) == EOF) { atomic_abort(fp, path); return -1; }
    return atomic_finish(fp, path);
}
int print_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    char buf[4096];
    size_t n;
    int bad = 0;
    while ((n = fread(buf, 1, sizeof(buf), fp))) if (fwrite(buf, 1, n, stdout) != n) { bad = 1; break; }
    if (ferror(fp)) bad = 1;
    if (fclose(fp)) bad = 1;
    return bad ? -1 : 0;
}

int message_number(const char *name, unsigned long *number) {
    size_t len = strlen(name);
    if (strncmp(name, "msg_", 4) || len < 12 || strcmp(name + len - 4, ".txt")) return -1;
    const char *start = name + 4;
    const char *dot = strchr(start, '.');
    if (!dot || dot - start < 3) return -1;
    for (const char *p = start; p < dot; ++p) if (!isdigit((unsigned char)*p)) return -1;
    errno = 0;
    char *end;
    unsigned long n = strtoul(start, &end, 10);
    if (errno == ERANGE || n == ULONG_MAX) return -2;
    if (end != dot) return -1;
    *number = n;
    return 0;
}
