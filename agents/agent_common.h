#ifndef AGENT_COMMON_H
#define AGENT_COMMON_H
#include "protocol.h"
int agent_args(int argc, char **argv);
int agent_report(const char *id, const char *text);
int agent_failure(const char *agent, const char *id, const char *input, const char *message, int code);
#endif
