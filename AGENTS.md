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
