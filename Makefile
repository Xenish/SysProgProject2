CC = cc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -g
.PHONY: all test clean
.SECONDARY:
SOURCES = src/main.c src/cli.c src/orchestrator.c src/process.c src/workspace.c src/protocol.c src/mailbox.c
OBJECTS = $(SOURCES:%.c=build/%.o)
AGENTS = analyst reporter
AGENT_COMMON = build/agents/agent_common.o build/src/protocol.o
all: agency $(AGENTS:%=agents/%)
agents/%: build/agents/%.o $(AGENT_COMMON)
	$(CC) $(CFLAGS) $^ -o $@
-include $(AGENT_COMMON:.o=.d) $(AGENTS:%=build/agents/%.d)
agency: $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@
build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
-include $(OBJECTS:.o=.d)
build/fixture: tests/fixture.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
test: all build/fixture
	sh tests/run_tests.sh
clean:
	rm -rf build agency
	rm -f agents/analyst agents/reporter agents/scout
	find status reports messages logs -type f ! -name .gitkeep -exec rm -f {} +
	rm -f data/task_*.out .agency.lock
