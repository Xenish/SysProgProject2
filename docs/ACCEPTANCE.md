# T01–T18 acceptance review

Local execution on macOS arm64 and Ubuntu 24.04 arm64 in Docker. Linux containers
used `--network none --init`. The toolchain was installed as a development step;
the application and acceptance runs used no network. A GitHub Actions workflow
is supplied, but no remote/CI run exists. `Unclosed quote` in test output is an
expected negative tokenizer test, not a build warning or failing assertion.

| ID | Evidence and result |
|---|---|
| T01 | PASS: `make clean && make`; C11, `-Wall -Wextra -Werror`, Clang and GCC. |
| T02 | PASS: echo text in log; list/status/wait/log/report paths tested. |
| T03 | PASS: FIFO keeps CLI idle; short child reaches done without wait/status/list; its PID is absent from `ps`. |
| T04 | PASS: analyst rows=4, sum=320, average=80.00; own report/output; done:0. |
| T05 | PASS: duplicate ID rejected; prior status/report left intact. |
| T06 | PASS: missing id/agent/input, missing executable/input, reserved output, output ownership and canonical conflict rejected before fork. |
| T07 | PASS: malformed CSV reports failure and produces done:2; zero ID/NaN/Inf/empty/extra columns/NUL rejected. |
| T08 | PASS: normal exit 127 remains done; missing/forced exec and signal death become error. Injected fork/waitpid errors also tested with child PID checks. |
| T09 | PASS: normal fixture has no report; collect prints `no report produced`. |
| T10 | PASS: msg_001 then msg_002 after restart; disk max999 advances to1000. |
| T11 | PASS: analyst→reporter briefing + own report/source_message; consumed messages skipped; numeric999 selected before1000; missing source gives exit1. Concurrent reporter rejected. |
| T12 | PASS: initial, running (demo), collect, send, reporter submit, failure and complete suggestions. No automatic task execution. |
| T13 | PASS: clean restart and actual idle orchestrator SIGKILL/restart retain statuses/counters. Dead saved PID and live detached PID become error when exit status is unavailable. |
| T14 | PASS: protocol writer uses tmp/flush/close/rename. RLIMIT_FSIZE write failure preserves original; rename failure removes temp; temp symlink rejected; failed status reservation prevents launch. Logs publish on reap. |
| T15 | PASS: actual make clean in a disposable source/workspace copy; runtime artifacts removed; placeholders, demo tasks/input and authored example preserved. Active lock prevents clean. |
| T16 | PASS: scout 5 rows/2 ERROR lines; lowercase ignored; no match gives exit1 and empty output. |
| T17 | PASS: full suite on macOS and actual Linux kernel; owned child PID checks, literal professor ps command and corrected self-excluding grep in Linux demo. No project zombies found. |
| T18 | PASS for document content: report contains all required sections, real demo excerpts, protocol/argv/recovery/limitations/AI disclosure. Student identity remains an explicit placeholder for the student to fill before submission. |

See `validation-linux.txt`, `validation-sanitizers-linux.txt`, `demo-linux.txt`.
The test script reports 16 PASS groups; these groups contain multiple assertions,
not sixteen individual assertions. Both AddressSanitizer and UndefinedBehaviorSanitizer
were enabled for a separate full Linux suite. Clang static analysis completed with
no diagnostics after the final source review.

Known acceptance scope: Linux was tested in an arm64 container rather than a
physical Ubuntu installation or x86_64 host. Power-loss durability, hostile
workspace mutation, arbitrary grandchildren of external `run` programs and
unbounded external program completion are not guaranteed. See report.md.
