CC = cc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -g
.PHONY: all test clean demo linux-test
.SECONDARY:
SOURCES = src/main.c src/cli.c src/orchestrator.c src/process.c src/workspace.c src/protocol.c src/mailbox.c src/advisor.c
OBJECTS = $(SOURCES:%.c=build/%.o)
AGENTS = analyst reporter scout
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
build/protocol-test: tests/protocol_test.c src/protocol.c src/cli.c include/protocol.h include/cli.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/protocol_test.c src/protocol.c src/cli.c -o $@
build/fault-agency: $(SOURCES) tests/faults.c $(wildcard include/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Dfork=agency_test_fork -Dexecvp=agency_test_execvp -Dwaitpid=agency_test_waitpid $(SOURCES) tests/faults.c -o $@
test: all build/fixture build/clean build/protocol-test build/fault-agency
	sh tests/run_tests.sh
build/clean: tools/clean.c src/protocol.c include/protocol.h include/agency.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/clean.c src/protocol.c -o $@
clean: build/clean
	./build/clean
	rm -rf build agency
	rm -f agents/analyst agents/reporter agents/scout
demo: all
	sh tests/demo.sh
linux-test:
	sh tests/linux.sh
