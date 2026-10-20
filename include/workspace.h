#ifndef WORKSPACE_H
#define WORKSPACE_H
#include "agency.h"
int workspace_open(Agency *a);
int recover_workspace(Agency *a);
void monitor_recovered(Agency *a);
int next_task_id(char *id, size_t size);
#endif
