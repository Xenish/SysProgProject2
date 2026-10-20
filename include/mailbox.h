#ifndef MAILBOX_H
#define MAILBOX_H
#include "agency.h"
int read_task(const char *path, Task *t);
int validate_task(const Task *t);
void submit_task(Agency *a, const char *path);
void send_message(Agency *a, const char *from, const char *to, const char *id, const char *message);
void show_tasks(Agency *a);
#endif
