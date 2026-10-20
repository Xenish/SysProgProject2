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
cd "$tmp"
cp "$root/agents/analyst" "$root/agents/reporter" agents/
cp "$root/data/survey.csv" data/
cp "$root/data/report_request.txt" data/
sed 's/task_001/task_040/g' "$root/tasks/task_001.task" > tasks/task_040.task
printf 'submit tasks/task_040.task\ncollect task_040\nexit\n' | ./agency > analyst-output 2>&1
grep -q 'state: done' status/task_040.status
grep -q 'exit_code: 0' status/task_040.status
grep -q 'collected: yes' status/task_040.status
grep -q 'rows_processed: 4' reports/task_040.report
grep -q 'sum: 320' reports/task_040.report
grep -q 'average: 80.00' reports/task_040.report
test -s data/task_040.out
printf 'id,score\n1,oops\n' > data/bad.csv
sed 's/task_040/task_041/g; s@data/survey.csv@data/bad.csv@' tasks/task_040.task > tasks/task_041.task
printf 'submit tasks/task_041.task\ncollect task_041\nexit\n' | ./agency > bad-csv-output 2>&1
grep -q 'state: done' status/task_041.status
grep -q 'exit_code: 2' status/task_041.status
grep -q 'status: failure' reports/task_041.report
test ! -f data/task_041.out
set +e
agents/reporter task_042 data/report_request.txt reports/task_040.briefing > reporter-output 2>&1
code=$?
set -e
test "$code" = 1
grep -q 'status: failure' reports/task_042.report
test ! -f reports/task_040.briefing
echo 'PASS P3: analyst CSV/output/report, malformed data done:2, reporter missing-message contract'
cd "$root"
cd "$tmp"
# Replace the P3 reporter copy with the built messaging agent (same build).
printf 'send analyst reporter task_040 "please brief"\nexit\n' | ./agency > send-output 2>&1
test -f messages/msg_001.analyst.reporter.txt
grep -q 'message: please brief' messages/msg_001.analyst.reporter.txt
cat > tasks/task_043.task <<'TASK'
id: task_043
agent: reporter
input: data/report_request.txt
output: reports/task_040.briefing
TASK
printf 'submit tasks/task_043.task\ncollect task_043\nexit\n' | ./agency > brief-output 2>&1
grep -q 'exit_code: 0' status/task_043.status
grep -q 'source_task: task_040' reports/task_043.report
grep -q 'source_message: msg_001.analyst.reporter.txt' reports/task_043.report
grep -q 'average: 80.00' reports/task_040.briefing
set +e
agents/reporter task_044 data/report_request.txt reports/second.briefing > repeat-output 2>&1
code=$?
set -e
test "$code" = 1
test ! -f reports/second.briefing
printf 'send analyst reporter task_040 "second message"\nexit\n' | ./agency > send-restart-output 2>&1
test -f messages/msg_002.analyst.reporter.txt
printf 'send scout reporter task_040 "invalid source"\nsend analyst unknown task_040 hello\nexit\n' | ./agency > invalid-send-output 2>&1
grep -q 'source task/agent mismatch' invalid-send-output
grep -q 'Unknown recipient agent' invalid-send-output
test "$(find messages -name '*.txt' | wc -l | tr -d ' ')" = 2
echo 'PASS P4: atomic numbered messages/restart, reporter briefing and durable consumption, send validation'
cd "$root"
# Isolated advisor scenarios must not inherit earlier deliberately failed tasks.
adv="$tmp/advisor"
mkdir -p "$adv/tasks" "$adv/status" "$adv/reports" "$adv/messages" "$adv/logs" "$adv/data" "$adv/agents"
cp agency "$adv/"
cp agents/analyst agents/reporter "$adv/agents/"
cp tasks/task_001.task tasks/task_002.task "$adv/tasks/"
cp data/survey.csv data/report_request.txt "$adv/data/"
cd "$adv"
printf 'suggest\nexit\n' | ./agency > initial-output
grep -q '^submit tasks/task_001.task$' initial-output
test "$(find status -name '*.status' | wc -l | tr -d ' ')" = 0
printf 'submit tasks/task_001.task\nwait task_001\nsuggest\ncollect task_001\nsuggest\nsend analyst reporter task_001 "please brief"\nsuggest\nsubmit tasks/task_002.task\ncollect task_002\nsuggest\nexit\n' | ./agency > advisor-output 2>&1
grep -q '^collect task_001$' advisor-output
grep -q '^send analyst reporter task_001 "please brief"$' advisor-output
grep -q '^submit tasks/task_002.task$' advisor-output
grep -q '^No pending suggestions.$' advisor-output
printf 'run /bin/sh -c "exit 2"\nwait task_003\nsuggest\nexit\n' | ./agency > failed-suggestion
grep -q 'log task_003;.*new ID' failed-suggestion
echo 'PASS P5: advisor initial/collect/send/submit/complete/failure priorities; no automatic execution'
cd "$root"

cd "$tmp"
cp "$root/agents/scout" agents/
cp "$root/data/events.txt" data/
sed 's/task_003/task_050/g' "$root/tasks/task_003.task" > tasks/task_050.task
printf 'submit tasks/task_050.task\ncollect task_050\nexit\n' | ./agency > scout-output 2>&1
grep -q 'exit_code: 0' status/task_050.status
grep -q 'rows_processed: 5' reports/task_050.report
grep -q 'matching_lines: 2' reports/task_050.report
test "$(wc -l < data/task_050.out | tr -d ' ')" = 2
if grep -q lowercase data/task_050.out; then exit 1; fi
printf 'INFO clean\n' > data/clean.txt
set +e
agents/scout task_051 data/clean.txt data/task_051.out > scout-empty-output
code=$?
set -e
test "$code" = 1
grep -q 'matching_lines: 0' reports/task_051.report
test ! -s data/task_051.out
echo 'PASS P6: scout case-sensitive ERROR search, output/report and no-match exit:1'
cd "$root"
cd "$tmp"
mkdir protocol-unit
(cd protocol-unit && "$root/build/protocol-test")
# Infrastructure fault binary calls the real POSIX functions except one selected failure.
for failure in fork exec waitpid; do
    faultdir="$tmp/fault-$failure"
    mkdir -p "$faultdir/tasks" "$faultdir/status" "$faultdir/reports" "$faultdir/messages" "$faultdir/logs" "$faultdir/data"
    cp "$root/build/fault-agency" "$faultdir/agency"
    (cd "$faultdir"; printf 'run /bin/sleep 0.1\nwait task_001\nexit\n' | AGENCY_TEST_FAULT="$failure" ./agency > output 2>&1)
    grep -q 'state: error' "$faultdir/status/task_001.status"
    case "$failure" in
        fork) grep -q 'fork failed' "$faultdir/status/task_001.status" ;;
        exec) grep -q 'exec/child setup failed' "$faultdir/status/task_001.status" ;;
        waitpid) grep -q 'waitpid failed' "$faultdir/status/task_001.status" ;;
    esac
    pid=$(sed -n 's/^pid: //p' "$faultdir/status/task_001.status")
    if test "$pid" != 0 && ps -p "$pid" -o stat= >/dev/null 2>&1; then echo 'FAIL: fault left child'; exit 1; fi
done
echo 'PASS P7: injected fork/exec/waitpid failures; all started child PIDs reaped'
# Hold a workspace lock; neither a second agency nor clean may mutate files.
mkfifo locked-input
./agency < locked-input > locked-output 2>&1 &
locked_pid=$!
exec 3>locked-input
printf 'help\n' >&3
sleep 0.1
if printf 'exit\n' | ./agency > lock-error 2>&1; then echo 'FAIL: double orchestrator'; exit 1; fi
grep -q 'Workspace locked' lock-error
if "$root/build/clean" > clean-lock-error 2>&1; then echo 'FAIL: clean accepted locked workspace'; exit 1; fi
grep -q 'Cannot clean a locked workspace' clean-lock-error
printf 'exit\n' >&3
exec 3>&-
wait "$locked_pid"
echo 'PASS P7: fcntl single orchestrator and clean lock protection'
# Concurrent reporter jobs must not consume the same message.
cp agents/reporter real-reporter
cp "$root/build/fixture" agents/reporter
printf 'delay input\n' > data/delay.txt
for id in 060 061; do
    printf 'id: task_%s\nagent: reporter\ninput: data/delay.txt\noutput: data/task_%s.out\n' "$id" "$id" > "tasks/task_$id.task"
done
printf 'submit tasks/task_060.task\nsubmit tasks/task_061.task\nexit\n' | ./agency > concurrent-output 2>&1
grep -q 'A reporter is already running' concurrent-output
test ! -f status/task_061.status
mv real-reporter agents/reporter
echo 'PASS P7: concurrent reporter rejection'
# A recovered PID is not our child. Observe it until its real parent reaps it.
sleep 0.4 &
detached_pid=$!
printf 'id: task_070\nagent: fixture\npid: %s\nstate: running\ncollected: no\n' "$detached_pid" > status/task_070.status
mkfifo recovery-input
./agency < recovery-input > live-recovery-output 2>&1 &
recovery_pid=$!
exec 3>recovery-input
printf 'wait task_070\nstatus task_070\nexit\n' >&3
wait "$detached_pid"
exec 3>&-
wait "$recovery_pid"
grep -q 'exit status unavailable after restart' status/task_070.status
grep -q 'state: error' status/task_070.status
# Actual orchestrator crash: persisted terminal states and ID authority survive SIGKILL.
mkfifo crash-input
./agency < crash-input > crash-output 2>&1 &
crash_pid=$!
exec 3>crash-input
printf 'status task_040\n' >&3
sleep 0.1
kill -KILL "$crash_pid"
exec 3>&-
wait "$crash_pid" 2>/dev/null || :
printf 'status task_040\nrun /bin/true\nexit\n' | ./agency > actual-restart-output
grep -q 'exit_code: 0' actual-restart-output
test -f status/task_071.status
echo 'PASS P7: live detached PID monitoring, SIGKILL restart and persisted ID authority'
# make clean in a disposable project copy; preserve authored examples/placeholders.
cleandir="$tmp/cleanup"
mkdir "$cleandir"
(cd "$root"; tar -cf - Makefile tools src include tasks data status reports messages logs) | (cd "$cleandir"; tar -xf -)
cp -R status/. "$cleandir/status/"
cp -R tasks/. "$cleandir/tasks/"
cp -R reports/. "$cleandir/reports/"
cp -R messages/. "$cleandir/messages/"
cp -R logs/. "$cleandir/logs/"
printf 'authored example\n' > "$cleandir/reports/example.report"
printf 'authored input\n' > "$cleandir/data/custom.txt"
(cd "$cleandir"; make clean > clean-output)
for dir in tasks status reports messages logs data; do test -f "$cleandir/$dir/.gitkeep"; done
test -f "$cleandir/tasks/task_001.task"
test -f "$cleandir/data/survey.csv"
test -f "$cleandir/reports/example.report"
test -f "$cleandir/data/custom.txt"
test ! -f "$cleandir/tasks/task_020.task"
test ! -f "$cleandir/status/task_040.status"
test ! -f "$cleandir/reports/task_040.report"
test ! -f "$cleandir/reports/task_040.briefing"
test ! -f "$cleandir/messages/msg_001.analyst.reporter.txt"
echo 'PASS P7: make clean preserves authored inputs/examples/.gitkeep; removes generated canonical/tasks/status/report/message/log'
cd "$root"
cd "$tmp"
# Canonical conflict and same-output ownership must not silently clobber work.
cat > tasks/task_080.task <<'TASK'
id: task_080
agent: fixture
input: data/input.txt
output: data/owned.out
TASK
sed 's/task_080/task_081/' tasks/task_080.task > external-conflict.task
printf 'submit external-conflict.task\nexit\n' | ./agency > output-conflict 2>&1
grep -q 'Output owned by another task' output-conflict
test ! -f status/task_081.status
sed 's@data/owned.out@data/different.out@' tasks/task_080.task > external-conflict.task
printf 'submit external-conflict.task\nexit\n' | ./agency > canonical-conflict 2>&1
grep -q 'Canonical task conflict' canonical-conflict
test ! -f status/task_080.status
cp messages/msg_001.analyst.reporter.txt messages/msg_999.analyst.reporter.txt
printf 'send analyst reporter task_040 after999\nexit\n' | ./agency > large-message-counter
test -f messages/msg_1000.analyst.reporter.txt
# Reporter chooses numeric oldest, not lexical order, and skips successful ACK metadata.
set +e
agents/reporter task_082 data/report_request.txt reports/numeric.briefing > numeric-output
code=$?
set -e
test "$code" = 0
grep -q 'source_message: msg_002.analyst.reporter.txt' reports/task_082.report
agents/reporter task_084 data/report_request.txt reports/numeric999.briefing > numeric999-output
grep -q 'source_message: msg_999.analyst.reporter.txt' reports/task_084.report
agents/reporter task_085 data/report_request.txt reports/numeric1000.briefing > numeric1000-output
grep -q 'source_message: msg_1000.analyst.reporter.txt' reports/task_085.report
# Failed initial status publication must prevent fork/log creation.
sed 's/task_080/task_082/g; s@data/owned.out@data/task_082.out@' tasks/task_080.task > tasks/task_082.task
mkdir status/task_083.status.tmp
printf 'run /bin/true\nexit\n' | ./agency > failed-publish-output 2>&1
test ! -f logs/task_083.log
test ! -f logs/task_083.log.tmp
test ! -f status/task_083.status
rmdir status/task_083.status.tmp
# Shell syntax is deliberately not expanded, and oversized lines recover at newline.
{
    printf 'run /bin/echo "$HOME;literal"\nwait task_083\n'
    awk 'BEGIN { for (i=0;i<9000;i++) printf "x"; printf "\n" }'
    printf 'help\nexit\n'
} | ./agency > parser-output 2>&1
grep -q '\$HOME;literal' logs/task_083.log
grep -q 'Command line too long' parser-output
grep -q 'submit <task_file>' parser-output
echo 'PASS P7: output/canonical ownership, numeric message counter/order, failed status pre-fork, CLI bounds/no expansion'
cd "$root"

cd "$tmp"
cat > messages/msg_1001.analyst.reporter.txt <<'MESSAGE'
from: analyst
to: reporter
task: task_999
message: missing source
MESSAGE
set +e
agents/reporter task_086 data/report_request.txt reports/missing.briefing > missing-source-output 2>&1
code=$?
set -e
test "$code" = 1
grep -q 'source report unavailable' reports/task_086.report
test ! -f reports/missing.briefing
n=90
for row in '0,10' '1,nan' '1,inf' '1,' '1,20,30' 'oops,20'; do
    printf 'id,score\n%s\n' "$row" > data/invalid.csv
    set +e
    agents/analyst "task_0$n" data/invalid.csv data/invalid.out > csv-validation-output 2>&1
    code=$?
    set -e
    test "$code" = 2
    test ! -f data/invalid.out
    n=$((n + 1))
done
printf 'id,score\000ignored\n1,10\n' > data/invalid.csv
set +e
agents/analyst task_096 data/invalid.csv data/invalid.out > csv-validation-output 2>&1
code=$?
set -e
test "$code" = 2
printf 'id,score\r\n1,70\r\n' > data/crlf.csv
agents/analyst task_097 data/crlf.csv data/crlf.out > crlf-output
grep -q 'average: 70.00' reports/task_097.report
echo 'PASS P7: reporter missing source; CSV zero ID/NaN/Inf/missing/extra/NUL rejection and CRLF support'
cd "$root"
