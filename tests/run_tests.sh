#!/bin/sh
set -eu
for dir in tasks status reports messages logs data; do
    test -f "$dir/.gitkeep"
done
test -f data/survey.csv
test -f tasks/task_001.task
echo 'PASS P0: workspace and authored fixtures'
root=$(pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/agency-test.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/tasks" "$tmp/status" "$tmp/reports" "$tmp/messages" "$tmp/logs" "$tmp/data" "$tmp/agents"
cp agency "$tmp/agency"
cd "$tmp"
cat > commands <<'CMDS'
run /bin/echo "hello world"
wait task_001
list
status task_001
log task_001
report task_001
run /bin/sh -c 'exit 2'
wait task_002
run /bin/sh -c 'exit 127'
wait task_003
run /no/such/agency-program
wait task_004
run /bin/sh -c 'kill -TERM $$'
wait task_005
exit
CMDS
./agency < commands > output 2>&1
grep -q 'hello world' logs/task_001.log
grep -q 'state: done' status/task_001.status
grep -q 'exit_code: 2' status/task_002.status
grep -q 'state: done' status/task_003.status
grep -q 'exit_code: 127' status/task_003.status
grep -q 'state: error' status/task_004.status
grep -q 'exec/child setup failed' status/task_004.status
grep -q 'state: error' status/task_005.status
grep -q 'signal 15' status/task_005.status
grep -q 'no report produced' output
echo 'PASS P1: run/list/status/wait/log/report; nonzero/127/exec/signal semantics'
# Keep input open without sending wait/list/status: reaping must happen at idle.
mkfifo input
./agency < input > idle-output 2>&1 &
agency_pid=$!
exec 3>input
printf 'run /bin/sleep 0.1\n' >&3
for n in 1 2 3 4 5 6 7 8 9 10; do
    if test -f status/task_006.status && grep -q 'state: done' status/task_006.status; then break; fi
    sleep 0.1
done
grep -q 'state: done' status/task_006.status
child_pid=$(sed -n 's/^pid: //p' status/task_006.status)
if ps -p "$child_pid" -o stat= > child-state 2>/dev/null; then
    echo 'FAIL: idle child still exists'; exit 1
fi
printf 'exit\n' >&3
exec 3>&-
wait "$agency_pid"
echo 'PASS P1: SIGCHLD idle reaping; child PID absent from ps'
cd "$root"
cd "$tmp"
cp "$root/build/fixture" agents/fixture
printf 'test input\n' > data/input.txt
cat > external.task <<'TASK'
id: task_020
agent: fixture
input: data/input.txt
TASK
printf 'submit external.task\ncollect task_020\nsubmit external.task\ntasks\nexit\n' | ./agency > mailbox-output 2>&1
test -f tasks/task_020.task
grep -q 'state: done' status/task_020.status
grep -q 'data/task_020.out' logs/task_020.log
grep -q 'Duplicate task id: task_020' mailbox-output
grep -q 'no report produced' mailbox-output
# Invalid tasks must not create any status/log or fork.
for field in id agent input; do
    sed "/^$field:/d" external.task > invalid.task
    printf 'submit invalid.task\nexit\n' | ./agency > invalid-output 2>&1
    grep -q 'Invalid task file' invalid-output
done
sed 's/task_020/task_021/; s/fixture/missing/' external.task > invalid.task
printf 'submit invalid.task\nexit\n' | ./agency > invalid-output 2>&1
grep -q 'Agent executable unavailable' invalid-output
test ! -f status/task_021.status
sed 's/task_020/task_022/; s@data/input.txt@data/missing@' external.task > invalid.task
printf 'submit invalid.task\nexit\n' | ./agency > invalid-output 2>&1
grep -q 'Input unreadable' invalid-output
test ! -f status/task_022.status
sed 's/task_020/task_023/' external.task > invalid.task
printf 'output: status/task_023.status\n' >> invalid.task
printf 'submit invalid.task\nexit\n' | ./agency > invalid-output 2>&1
grep -q 'reserved directory' invalid-output
test ! -f status/task_023.status
printf 'status task_020\nrun /bin/true\nexit\n' | ./agency > restart-output 2>&1
grep -q 'state: done' restart-output
test -f status/task_021.status
# Dead persisted PID can never be assigned a fabricated normal exit.
cat > status/task_030.status <<'STATUS'
id: task_030
agent: fixture
pid: 2147483647
state: running
collected: no
STATUS
printf 'status task_030\nexit\n' | ./agency > recovery-output 2>&1
grep -q 'state: error' status/task_030.status
grep -q 'exit status unavailable after restart' recovery-output
echo 'PASS P2: submit argv/default output, duplicate/preflight, canonical task, persistent IDs/recovery'
cd "$root"
