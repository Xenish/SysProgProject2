CC = cc
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2 -g
.PHONY: all test clean
all:
	@echo 'P0: workspace skeleton ready (application starts in P1)'
test:
	sh tests/run_tests.sh
clean:
	rm -rf build agency
	rm -f agents/analyst agents/reporter agents/scout
	find status reports messages logs -type f ! -name .gitkeep -exec rm -f {} +
	rm -f data/task_*.out .agency.lock
