#ifndef PROCESS_H
#define PROCESS_H
#include "agency.h"
#include <signal.h>
extern volatile sig_atomic_t child_changed;
int process_init(sigset_t *original);
int launch(Agency *a, Task *t, char **argv);
void reap(Agency *a);
void wait_task(Agency *a, Task *t);
void wait_children(Agency *a);
#endif
