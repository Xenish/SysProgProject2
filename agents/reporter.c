#include "agent_common.h"
int main(int argc, char **argv) {
    if (agent_args(argc, argv)) return 2;
    return agent_failure("reporter", argv[1], argv[2], "no unprocessed reporter message", 1);
}
