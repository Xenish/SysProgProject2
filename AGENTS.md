# Codex repository instructions

Read `project2-the-air-gapped-agency.md` and `IMPLEMENTATION_SPEC.md` before modifications. The professor's assignment takes precedence on mandatory requirements, and `IMPLEMENTATION_SPEC.md` settles all unassigned design choices. For phase-by-phase tasks use `CODEX_EXECUTION_GUIDE.md`.

- Implement C11/POSIX only for the orchestrator and agents. Build with `-Wall -Wextra -Werror`.
- Never use network communication, sockets, HTTP, APIs, `system()`, GUI or LLM for core functionality.
- Use `fork`, `execvp`, `waitpid`, `dup2` and safe process tracking. Do not confuse nonzero normal exits (`done`) with infrastructure failures (`error`).
- Shared files publish via `filename.tmp` followed by `rename()`. Maintain exactly one writer per file.
- Disk workspace is the source of truth; task and message IDs are derived from persisted state.
- Separate protocol, CLI, process, mailbox, workspace, advisor and agent modules as planned.
- Implement one defined phase at a time, and run `make clean && make && make test` before claiming success.
- Do not claim a test ran unless it actually did. Do not invent student details, GitHub repository URLs, or demo transcripts.
- Do not alter the professor's original assignment document.
- Keep runtime-generated files out of Git; preserve `.gitkeep` and authored examples during cleanup.
