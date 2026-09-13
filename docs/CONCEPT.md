# Tidemind — a city that learns to live

Build a compact, self-sufficient estuary town. Connect streets, provide homes,
food, water, energy and work, and balance the budget while residents learn
which neighbourhoods fit them. Reach a thriving town, then keep building.

The visual direction is a warm, illustrated isometric landscape surrounded by
deep blue water, with a restrained cream-and-ink civic planning interface.
Buildings and scenery are drawn from original geometry in C++.

## Why this belongs among these repositories

- `isolith/README.md`: isometric geometry, deterministic procedural content,
  adaptive play and an offline game that does not require an account.
- `keepsake/README.md` and `keepsake/AGENTS.md`: C++ gameplay separated from
  storage, local saves, and verification by actually playing the game.
- `experai/README.md`: local model training, deterministic seeds and learned
  state. Tidemind uses a small native network rather than a language service.
- `digital-person/README.md`: persistent identity and differing preferences
  inform resident cohorts with different neighbourhood priorities.

This is a new sibling project; no existing project is repurposed or modified.
Social syncing would fit the wider ecosystem, but the completed core game
needs no network, account, API key or external model.

## Completion criteria

A native interactive game with a generated map, construction and demolition,
connected infrastructure, residents, economy, resource production, progression,
failure and recovery/restart, controls and onboarding, useful AI inspection,
save/load, reproducible tests, and a verified build. The learned model must
alter actual resident housing decisions and retain its weights in saves.

## Implementation evidence from the repository survey

`isolith/src/Level/Generation/AdaptiveDirector.cs` exposes readable adaptation
metrics and reasons; `SectionGenerator.cs` seeds reproducible geometry. That
informed Tidemind's visible inference metrics and seeded island generation.
`experai/src/training/trainer.rs` includes seeded model initialisation,
gradient clipping and checkpoint persistence. Tidemind applies those patterns
in an independently implemented, much smaller C++ network.

The native interface uses SDL2, already installed on this machine. API checks
used the upstream documentation for
[logical-size rendering](https://wiki.libsdl.org/SDL2/SDL_RenderSetLogicalSize)
and [geometry rendering](https://wiki.libsdl.org/SDL2/SDL_RenderGeometry).
No source was copied from the user's other repositories.
