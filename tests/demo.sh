#!/bin/sh
set -eu
root=$(pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/agency-demo.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/tasks" "$tmp/status" "$tmp/reports" "$tmp/messages" "$tmp/logs" "$tmp/data"
cp -R src include agents tools "$tmp/"
cp Makefile "$tmp/"
for dir in tasks status reports messages logs data; do cp "$dir/.gitkeep" "$tmp/$dir/"; done
cp tasks/task_001.task tasks/task_002.task tasks/task_003.task "$tmp/tasks/"
cp data/survey.csv data/report_request.txt data/events.txt "$tmp/data/"
cd "$tmp"
printf '$ make clean && make\n'
make clean && make
cat > demo.commands <<'COMMANDS'
help
tasks
suggest
run /bin/sleep 0.3
status task_004
list
wait task_004
submit tasks/task_001.task
status task_001
collect task_001
suggest
send analyst reporter task_001 "please brief"
suggest
submit tasks/task_002.task
collect task_002
report task_002
log task_001
suggest
submit tasks/task_003.task
collect task_003
exit
COMMANDS
printf '\n$ ./agency < demo.commands\nInputs:\n'
cat demo.commands
printf '\nActual output:\n'
./agency < demo.commands
printf '\n$ ls status\n'
ls status
printf '\n$ cat reports/task_001.briefing\n'
cat reports/task_001.briefing
printf '\n$ cat status/task_001.status\n'
cat status/task_001.status
printf '\n$ ./agency (restart: tasks, status task_002, suggest, exit)\n'
printf 'tasks\nstatus task_002\nsuggest\nexit\n' | ./agency
printf '\n$ ps aux | grep defunct\n'
ps aux | grep defunct || :
printf '\n$ ps aux | grep "[d]efunct"\n'
ps aux | grep '[d]efunct' || :
printf '\nOwned demo child PID checks:\n'
for file in status/*.status; do
    pid=$(sed -n 's/^pid: //p' "$file")
    if ps -p "$pid" -o stat= >/dev/null 2>&1; then echo "FAIL: child $pid remains"; exit 1; fi
    printf 'PID %s: absent (reaped)\n' "$pid"
done
printf '\nAtomic publishing implementation (src/protocol.c):\n'
sed -n '/^FILE \*atomic_begin/,/^int print_file/p' "$root/src/protocol.c"
grep -q 'average: 80.00' reports/task_001.briefing
grep -q 'source_message: msg_001.analyst.reporter.txt' reports/task_002.report
grep -q 'matching_lines: 2' reports/task_003.report
printf '\nPASS: canonical analyst -> reporter demo, scout, restart, child PID checks\n'
