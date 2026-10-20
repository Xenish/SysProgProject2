#ifndef MAILBOX_H
#define MAILBOX_H
#include "agency.h"
int read_task(const char *path, Task *t);
int validate_task(const Task *t);
void submit_task(Agency *a, const char *path);
void show_tasks(Agency *a);
#endif
