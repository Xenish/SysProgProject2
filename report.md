# BIL 322 Project 2 — The Air-Gapped Agency

## 1. Student information

- Name: **[STUDENT MUST FILL]**
- Student ID: **[STUDENT MUST FILL]**
- Second student, if applicable: **[FILL OR REMOVE]**

No student identity or GitHub URL has been invented. This is a local Git
repository without a configured remote. The original professor assignment,
IMPLEMENTATION_SPEC.md, CODEX_EXECUTION_GUIDE.md and AGENTS.md are unchanged.

## 2. Design and module ownership

The application implements all twelve required CLI commands and `help`, two
required agents and the third-agent scout bonus. The core is C11/POSIX. It uses
no networking, sockets, HTTP, external APIs, GUI, `system()` or LLM. File mailbox,
logs and process exit status carry the work. The only pipe is an exec-error pipe
between the orchestrator and its own child.

| Module | Responsibility |
|---|---|
| `main.c` | Workspace boot, signal-safe input event loop and graceful shutdown. |
| `cli.c` | Bounded tokenizer, single/double quotes, backslash escaping. |
| `orchestrator.c` | Dispatch, task table, status ownership, report/log presentation. |
| `process.c` | fork/execvp/dup2, error pipe, status classification, waitpid/reaping. |
| `workspace.c` | fcntl workspace lock, disk-derived IDs and restart reconstruction. |
| `protocol.c` | Bounded key:value parser and tmp/flush/close/rename file publishing. |
| `mailbox.c` | Task validation/canonical publication, submit/tasks/send. |
| `advisor.c` | Read-only workspace-based suggestions. |
| `agent_common.c` | Agent argument validation and own-ID success/failure reports. |
| `analyst.c`, `reporter.c`, `scout.c` | Separate real agent executables. |
| `tools/clean.c` | Locked cleanup of generated artifacts, preserving authored files. |

The RAM table caches the process relationships needed for `waitpid`. Workspace
files retain the durable task/message/status/report context. One orchestrator
holds an advisory `fcntl` write lock on `.agency.lock`; its FD has `FD_CLOEXEC`.
The lock file is never unlinked by clean, avoiding lock-inode replacement races.
The workspace is trusted: users must not edit protocol files concurrently with
their owners or run another standalone agent against an already-owned ID/output.

## 3. Protocol for another team's agents

All paths below are relative to the workspace root, which must be the working
directory. Protocol documents contain one `key: value` per line. The first colon
separates key from value; values may contain additional colons. Leading/trailing
whitespace is trimmed. Blank lines are allowed. Duplicate keys, embedded NUL,
control characters in values, oversized lines/fields and malformed lines are
rejected. Unknown keys are retained by the parser but ignored by consumers.

Limits: 32 keys/document; key 63 bytes; value/path 1023 bytes; ID 63 bytes;
protocol/CLI line 8191 bytes excluding newline. CLI accepts at most 127 tokens,
including the command. At most 1024 submitted/recovered tasks are cached per
session; clean/archive old work before exceeding this limit.

Task IDs are strings matching `task_` plus at least three decimal digits. Derived
names are always built from that exact ID. After task_999 comes task_1000.
Agent names match `[A-Za-z0-9_-]+`. New `run` IDs use max numeric task filename
from tasks/status plus one; message IDs use max numeric message filename plus
one on every send. Numeric exhaustion produces an error rather than wraparound.

### Task: tasks/task_001.task

```text
id: task_001
agent: analyst
input: data/survey.csv
output: data/task_001.out
priority: 1
```

`id`, `agent`, `input` must be nonempty and valid. `output` defaults to
`data/<id>.out`. `priority` is parsed as an integer and has no scheduling effect.
The input and executable must be regular, accessible files. The output parent
must exist and be writable. Existing outputs, output ownership conflicts and
reserved protocol paths are rejected. Resolved parent paths prevent simple
`..`/symlink aliases from bypassing output comparisons.

A prepared `.task` is a pending work order, not evidence of a previous launch.
Duplicate means the ID is already tracked or has a status file. A finished ID
cannot be resubmitted. External task files are atomically canonicalized to
`tasks/<id>.task`; an existing canonical task with different id/agent/input/output
is rejected. The sample tasks remain authored inputs and survive clean.

### Status: status/task_001.status

Only the orchestrator writes status. A running example:

```text
id: task_001
agent: analyst
pid: 102
state: running
collected: no
canonical_created: no
```

A normal process exit appends the verdict fields:

```text
state: done
exit_code: 2
report: reports/task_001.report
```

Any normal exit is `done`, including 1, 2, 3 or 127. The code is the agent's
answer. A signal, fork/exec/child setup/waitpid error, log publication failure or
unknown recovered exit outcome produces `error` with an orchestrator-written
`message`. `pid: 0` means no child PID was obtained. Error statuses do not invent
an exit code. A done task may have no report; collect then says
`no report produced` rather than crashing.

`collected: yes` is persisted only after report output succeeds. `canonical_created`
records that submit generated a canonical task, so clean can preserve authored
work orders. A conservative `error: launch interrupted before PID publication`
status reserves the ID before any fork; successful startup replaces it with
running. Failed reservation prevents process creation.

### Standard report: reports/<own-id>.report

Each agent writes only its own standard report, even when consuming another
agent's report. The analyst's actual successful fields are:

```text
agent: analyst
input: data/survey.csv
rows_processed: 4
sum: 320
average: 80.00
status: success
```

An agent attempts a `status: failure` report with `agent`, `input` and `message`
on logical/I/O failure. When the report directory itself is unwritable this
cannot be guaranteed; exit3 and the log remain available.

Analyst accepts simple two-column CSV headed `id,score`, positive integer IDs and
finite numeric scores. It rejects malformed rows, missing/extra values, NaN,
Inf, NUL, overflowing sums and data lines of 8192 bytes or longer. LF and CRLF
are supported, quoted CSV is not. Duplicate positive row IDs are not required
to be unique. Zero data rows produce exit1 with a failure report.

Reporter scans `messages/`, checking `to: reporter`. It chooses the smallest
numeric sequence not recorded by a successful reporter report's
`source_message`. It opens `reports/<source-task>.report`, requires successful
status and matching sender, then writes the briefing to its own `<output>`.
The demo uses `reports/task_001.briefing`. Its own report is distinct:

```text
agent: reporter
input: data/report_request.txt
source_task: task_001
source_message: msg_001.analyst.reporter.txt
briefing: reports/task_001.briefing
status: success
```

The reporter's input is a required readable task-context/request file; the
mailbox supplies the source task. No message or absent source report produces
exit1 with a failure report. Malformed/unsuccessful sources produce exit2.
Messages remain on disk. A second concurrent reporter is refused before fork,
preventing two active reporters from selecting the same message.

Scout scans text with case-sensitive `strstr(line, "ERROR")`. Matching lines go
to `<output>`; its own report contains `rows_processed`, `matching_lines`,
`keyword: ERROR`, `status: success`. No matching lines still produce a valid
empty output and success report, with exit1 (grep-like normal result).

All agents use: 0 success; 1 no suitable result; 2 invalid data/arguments;
3 input/output/report I/O failure. Invalid argv without a trustworthy own ID
cannot safely produce an own-ID report and returns2 with a stderr diagnostic.

### Message: messages/msg_001.analyst.reporter.txt

```text
from: analyst
to: reporter
task: task_001
message: please brief
```

The orchestrator is the only message publisher. `from` must match an existing
source task's agent; `to` must identify a regular executable under `agents/`.
The message must be nonempty, single-line and within the field limit.
`send analyst reporter task_001 "please brief"` creates the example above.
Every send rescans all final `msg_*.txt` files for max+1. `.tmp` files are never
considered delivered messages, so restart cannot reuse final message numbers.
Successful messages are not deleted.

### Atomic publication and file ownership

| Artifact | Sole writer | Readers |
|---|---|---|
| tasks/*.task | User for authored work orders; orchestrator for canonical copies | Orchestrator |
| status/*.status | Orchestrator | CLI/advisor/humans |
| reports/*.report | Agent owning the task ID | Orchestrator/other agents |
| task output / briefing | Agent owning that output | Other agents/humans |
| messages/*.txt | Orchestrator | Addressed agent |
| logs/*.log.tmp | Child stdout/stderr via orchestrator's dup2 | No protocol consumer |
| logs/*.log | Same child data, published by parent after reap | Humans/CLI |

`atomic_begin` creates `filename.tmp` in the same directory; stale regular temps
may be removed, symlink/nonregular temps are refused and creation uses O_EXCL.
The writer checks stream errors, fflush and fclose before rename. On failure,
the previous final remains untouched. The test lowers RLIMIT_FSIZE to force a
real write/flush failure, verifies the original final, and tests rename failure
and temp symlink rejection. Logs are captured to `.log.tmp` and renamed after
the child exits; `log` while running therefore has no final log yet. Only the
child writes log data; the parent controls the publication name.

## 4. submit to execvp argument construction

Validation and canonical task publication happen before fork. The orchestrator
constructs an argv array with exactly these entries and a NULL terminator:

```c
char *args[] = {
    "./agents/analyst", "task_001", "data/survey.csv",
    "data/task_001.out", NULL
};
execvp(args[0], args);
```

There is no string concatenation into a shell command. Missing id/agent/input,
invalid paths, an inaccessible executable/input or conflicting output are clear
preflight errors with no fork. Missing output receives the documented default.
Each real agent requires argc==4. `run` similarly passes the parsed program and
args directly to execvp; a user can explicitly run `/bin/sh -c` if desired.

The child dup2s its log FD onto stdout and stderr, restores its signal mask,
then execs. Its error-pipe write end has FD_CLOEXEC. Successful exec closes it;
EOF at the parent means the child reached exec. Failed exec/setup sends errno
and calls `_exit(127)`. An agent that legitimately returns127 sends no errno,
so it remains done:127. The signal handler only sets a volatile sig_atomic_t
flag. SIGCHLD is blocked between loop iterations and atomically unblocked by
pselect; safe main-loop code uses waitpid(-1, ..., WNOHANG). A 250ms poll also
monitors recovered processes. list/status explicitly reap first. wait uses
waitpid(-1, ..., 0), distributing every child's result, including non-target
children. A transient waitpid error records error but retains ownership until
the child is actually reaped.

## 5. Crash recovery and precise limits

Startup scans final status files into a fresh table and loads corresponding task
context. Tasks without statuses remain pending. Done/error, PID, exit_code,
collected metadata, reports, log snapshots and messages survive an ordinary
restart. Corrupt persisted status is rejected at startup; it is not silently
converted into success. The OS releases the dead orchestrator's fcntl lock.

For saved running PIDs, `kill(pid,0)` distinguishes an absent process from a
possibly-live process (EPERM counts as possibly live). The new orchestrator
cannot waitpid another parent's process. It observes liveness only. Once that
PID disappears it writes error with `exit status unavailable after restart`.
Even a plausible completed report does not justify inventing exit0. Explicit
wait can wait for a recovered PID to disappear; exit waits only for its own
children and does not signal a recovered PID.

Tests cover dead persisted PIDs, a live process parented/reaped by the test
shell, and an actual SIGKILL of an idle orchestrator followed by restart.
The tests do not claim to have exhaustively killed the orchestrator at every
instruction or simulated a physical power failure.

Important limits:

- PID reuse can make an unrelated process look like an old live agent. No
  recovered PID is killed; its true identity/exit code cannot be proven.
- Reservation protects IDs before launch, but fork, PID publication, output,
  report and status are separate operations. A crash immediately after fork
  but before recording its PID can leave an untracked orphan. Child reaping
  is guaranteed for normally running/gracefully exiting orchestrators, not
  after the parent is killed. The host init/reaper owns orphan cleanup.
- Atomic rename prevents partial final files; no fsync is used. This is not a
  power-loss durability guarantee. Agent output and standard report are not
  one transaction: output may exist when report publishing fails.
- Reporter consumption is durable after its successful own report. A crash
  between briefing publication and that report can leave an unacknowledged
  message and orphan briefing. This is not a general exactly-once queue.
- Agent reports produced by a child that exits after restart remain useful
  evidence, but the process status may still correctly be error/unknown.
- The workspace and binaries are trusted, and advisory locks need cooperating
  participants. Arbitrary external run programs can fork grandchildren or
  alter files; the orchestrator tracks only its own children. External programs
  are not sandboxed and must respect the project's air-gap discipline.
- exit waits for own children indefinitely; timeout/retry and process-tree
  killing are outside scope. Custom external output paths are not removed by
  clean. Generated task/report/log/message namespaces are reserved for runtime;
  keep authored examples outside those reserved names.
- Priority scheduling, JSON, GUI and local LLM bonuses are deliberately absent.
  Scout is the implemented third-agent bonus. There is no known failing case
  in the documented acceptance suite; these limits remain explicit.

## 6. Advisor rules

The advisor never launches a command. It applies global priorities in this order:

1. Error or normal nonzero exit: suggest log and repair with a **new ID**.
2. Running task: suggest tasks/status.
3. Done task with an available uncollected report: suggest collect.
4. Successful analyst report without a briefing or reporter message: suggest send.
5. Reporter message without a completed briefing and valid pending reporter:
   suggest submit for that task.
6. No launched tasks: suggest the prepared task_001.
7. Otherwise: `No pending suggestions.`

Task state and persisted report/collected/message/briefing evidence drive these
rules. Because all nonzero exits are surfaced for review, scout's no-match exit1
also triggers a log suggestion; it still remains a normal done task.

## 7. Actual validation and demo output

`make clean && make && make test` was run after every phase. Final native macOS
and Ubuntu 24.04 arm64/GCC13 runs each passed 16 test groups. A separate full
Ubuntu suite passed with AddressSanitizer and UndefinedBehaviorSanitizer enabled.
Clang static analysis of src/agents/tools completed without diagnostics after
fixes. Fault injection is linked only into build/fault-agency, never agency.

The Linux environment was a real Linux kernel through Docker Desktop, with
Ubuntu userland, `--init` for orphan reaping and `--network none` during tests.
It was not a physical Ubuntu host or x86_64 validation. The toolchain image was
installed through development network access, not by the core application.
GitHub Actions has been authored but not run; no remote repository is configured.

Detailed T01–T18 mapping: [docs/ACCEPTANCE.md](docs/ACCEPTANCE.md).
Full raw evidence: [Linux build/test](docs/validation-linux.txt),
[Linux sanitizers](docs/validation-sanitizers-linux.txt),
[macOS test run](docs/validation-macos.txt), and
[Linux demo](docs/demo-linux.txt).
The intentional `Unclosed quote` stderr line is a passing negative-parser test.

The following is copied from the actual Linux demo output (PIDs are real from
that container run). The complete transcript includes inputs, build commands,
status while running, two different suggestions, restart, ps and atomic writer
source. `make demo` reproduces it in a disposable workspace.

```text
Task submitted: task_001 (agent: analyst, pid 102)
id: task_001
agent: analyst
pid: 102
state: running
collected: no
canonical_created: no
agent: analyst
input: data/survey.csv
rows_processed: 4
sum: 320
average: 80.00
status: success
send analyst reporter task_001 "please brief"
Message sent: messages/msg_001.analyst.reporter.txt
submit tasks/task_002.task
Task submitted: task_002 (agent: reporter, pid 103)
agent: reporter
input: data/report_request.txt
source_task: task_001
source_message: msg_001.analyst.reporter.txt
briefing: reports/task_001.briefing
status: success
```

The actual generated briefing:

```text
Agency briefing
Source task: task_001
Source message: msg_001.analyst.reporter.txt
agent: analyst
input: data/survey.csv
rows_processed: 4
sum: 320
average: 80.00
status: success
```

The professor's literal `ps aux | grep defunct` was executed and matched only
its own grep process. `ps aux | grep '[d]efunct'` then produced no output.
All four owned demo child PIDs (102,103,104,101) were separately checked absent
from ps after exit. The FIFO-idle and fault-injection tests also check individual
child PIDs, so unrelated host processes are not presented as project zombies.

## 8. Delivery and remaining student action

Submit tracked source/header files, Makefile, tests and tools, authored demo
inputs/tasks, `.gitkeep` directories, README, this report, original instructions
and verification evidence. Build/runtime products are ignored. Fill the student
identity fields before submitting. If a GitHub/Classroom URL is required, the
student must provide a real authorized repository and push the existing local
history there. No GitHub URL/account or remote CI success is claimed here.

Phase history is recorded in [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) and Git.
A source archive can be produced with `git archive --format=zip HEAD`. Git
history itself remains in this local repository (or a separate git bundle).

## 9. AI usage disclosure

OpenAI Codex assisted with reading the supplied requirements, designing and
implementing the C modules and Makefile, writing fixtures and fault-injection
checks, debugging compiler/static-analysis findings, running local commands,
and drafting README/report/test evidence. The runtime contains no AI/ML model
and makes no LLM/API calls. The developer tool's assistance is distinct from
agent functionality. The actual command outputs are retained; unexecuted tests
or CI runs are not labeled passed. The student must review the code and be
able to explain process ownership, exec-error detection, atomic publication,
recovery limits and every submitted implementation detail.
