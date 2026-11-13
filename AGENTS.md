# Tidemind

A C++20/SDL2 offline isometric city builder. Keep the simulation independent
of SDL so its economy, neural learning, and persistence can be tested headlessly.

- Use CMake and CTest. Build and run the game before declaring a change complete.
- Source belongs in `src/`, tests in `tests/`, design evidence in `docs/`.
- Neural AI must run real inference and backpropagation and affect residents.
- Save the random generator and learned weights; loading must resume deterministically.
- Generated geometry is original. Document the provenance of bundled assets.
- Keep builds and personal saves out of Git. Commit coherent milestones.
- Do not publish or push unless the user asks.

<!-- allowed-agents -->
## Allowed agents

Only **Claude** (Anthropic) and **Mistral** (Mistral AI) may be used as coding agents in this repository. This rule supersedes any earlier agent-attribution guidance in this file.

- Agent-assisted commits must credit only one of those agents:
  - `Co-Authored-By: Claude <noreply@anthropic.com>`
  - `Co-Authored-By: Mistral Vibe <vibe@mistral.ai>`
- Allowed agent footers are `🤖 Generated with [Claude Code](https://claude.com/claude-code)` and `🤖 Generated with [Mistral Vibe](https://mistral.ai)`.
- Do not add `Co-Authored-By:` trailers, `Generated with` footers, author names, or committer identities for any other agent or AI tool.
- Do not credit another agent in commit metadata.
