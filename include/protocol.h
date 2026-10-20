#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "agency.h"
#include <stdio.h>
#define MAX_KEYS 32
typedef struct { char key[MAX_KEYS][64], value[MAX_KEYS][FIELD]; size_t count; } Document;
int read_document(const char *path, Document *doc);
const char *value(const Document *doc, const char *key);
int copy_field(char *dst, size_t size, const char *src);
int valid_id(const char *id);
int valid_name(const char *name);
int single_line(const char *s);
int task_path(char *dst, size_t size, const char *dir, const char *id, const char *ext);
FILE *atomic_begin(const char *path);
int atomic_finish(FILE *fp, const char *path);
void atomic_abort(FILE *fp, const char *path);
int atomic_text(const char *path, const char *text);
int message_number(const char *name, unsigned long *number);
int print_file(const char *path);
#endif
