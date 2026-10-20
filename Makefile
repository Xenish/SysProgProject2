CC = cc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -g
.PHONY: all test clean
SOURCES = src/main.c src/cli.c src/orchestrator.c src/process.c src/workspace.c src/protocol.c
OBJECTS = $(SOURCES:%.c=build/%.o)
all: agency
agency: $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@
build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
-include $(OBJECTS:.o=.d)
test:
	sh tests/run_tests.sh
clean:
	rm -rf build agency
	rm -f agents/analyst agents/reporter agents/scout
	find status reports messages logs -type f ! -name .gitkeep -exec rm -f {} +
	rm -f data/task_*.out .agency.lock
