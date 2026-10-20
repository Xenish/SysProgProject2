#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H
#include "agency.h"
Task *find_task(Agency *a, const char *id);
int save_status(const Task *t);
void show_report(Task *t);
int dispatch(Agency *a, int argc, char **argv);
#endif
