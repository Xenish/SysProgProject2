# Phase execution record

The repository initially contained only four instruction documents and no .git.
Those documents were read in full before implementation and retained unchanged.
No unresolved mandatory/design conflict was found. A local Git repository was
initialized with the user's existing Git author configuration; no remote URL
or identity was invented. Phases were implemented sequentially, with no approval
pause and no subagents. Each phase ran `make clean && make && make test`, with
failures fixed before its commit or the next phase.

| Phase | Local commit | Scope and evidence at phase boundary |
|---|---|---|
| P0 | 25279b6 | Workspace/.gitkeep, sample CSV/request/tasks, ignore/build/test skeleton. Skeleton test passed; no application algorithm claimed. |
| P1 | 67828bf | Modular CLI/process/protocol/workspace; normal nonzero/127 vs exec/signal; logs, wait, status; idle FIFO reaping with absent child PID. |
| P2 | 0a8d6aa | submit/tasks/collect, preflight, canonical task/default output, duplicates and persisted state/ID recovery. |
| P3 | 431d07b | Real analyst/reporter binaries; CSV metrics and malformed CSV done:2; reporter missing-message failure contract. |
| P4 | 28a8f50 | send, disk message counters, durable reporter consumption and analyst→reporter briefing. |
| P5 | 80c65a0 | Ordered read-only advisor; initial/collect/send/submit/failure/complete scenarios. |
| P6 | 2c5ffcc | Scout, event fixture/task, case sensitivity and normal no-match exit1. |
| P7 tests | 8288232 | GCC portability correction, safe ID reservation/log publication/cleanup, injected faults, recovery/concurrency/bounds tests, demo and CI configuration. Final 16 PASS groups on macOS/Linux and Linux ASan/UBSan. |
| P7 docs | See final `git log` entry | README, report, acceptance matrix and genuine captured validation/demo evidence. |

P0–P6 validations were native macOS. P7 additionally used Ubuntu 24.04 arm64
containers on Linux 6.12.76-linuxkit with GCC13, `--network none --init`.
Docker Desktop initially was stopped; it was started locally. A development-only
Ubuntu toolchain image was built. No core application network access was added.

Problems found and fixed during P7:

- GCC format-truncation warning in executable path construction; bounded and
  checked buffer construction now passes GCC and Clang with -Werror.
- A copied macOS cleanup executable was not runnable in the first Linux test
  harness attempt; the harness now discards copied build outputs before compiling.
- An added test assumed the wrong generated run ID after pending fixtures;
  the fixture now explicitly reserves the intended maximum task number.
- Transient waitpid errors must preserve child ownership until reaping.
- A status reservation must succeed before fork; log contents publish on reap.
- Cleanup must not unlink the lock inode, run against a live workspace, or
  erase authored examples/placeholders. Generated canonical provenance is persisted.
- Parser embedded-NUL handling, stream EOF/error checks and temp symlink checks
  were strengthened. Final Clang static analysis produced no diagnostics.

The Linux/ASan/UBSan/native runs recorded in docs were successful runs after the
relevant corrections, not the earlier failed attempts. GitHub Actions itself
was not executed because no authorized remote exists. Physical Ubuntu and
x86_64 execution were not performed. Student identity fields remain for the
student; documented POSIX recovery/transaction limitations are not concealed.
