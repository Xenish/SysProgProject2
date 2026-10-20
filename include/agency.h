#ifndef AGENCY_H
#define AGENCY_H
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#define FIELD 1024
#define IDLEN 64
#define LINE 8192
#define MAX_ARGS 128
#define MAX_TASKS 1024

typedef struct {
    char id[IDLEN], agent[FIELD], input[FIELD], output[FIELD];
    char state[16], message[FIELD];
    pid_t pid;
    int exit_code;
    bool collected, child;
} Task;
typedef struct {
    Task tasks[MAX_TASKS];
    size_t count;
    int lock_fd;
} Agency;
#endif
