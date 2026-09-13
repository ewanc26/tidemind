# Verification record

Verified on Apple Silicon macOS with Apple Clang 21, CMake, SDL2 and SDL2_ttf.

- Release build compiles the C++20 core and native SDL2 interface.
- CTest exercises simulation/neural tests, the complete SDL input flow and CLI help.
- AddressSanitizer and UndefinedBehaviorSanitizer exercise the core and campaign.
- The actual macOS display runs the same interface smoke test successfully.
- The native window was rendered and visually inspected at 1400 × 900.
- A campaign started with 24 residents and 1,700 coins, paid normal construction
  costs and waited for income when necessary. It reached 240 residents by day
  291, positive daily cash flow, around 80% wellbeing, and the independence goal.
- `docs/town.png` is a screenshot of that completed campaign, not a mockup.

The campaign uses the same build, cost and daily simulation methods as player
input. It does not grant test-only money or set the victory flag. See
`playCampaign()` in `tests/core_tests.cpp` for its reproducible construction plan.

GitHub Actions repeats release and sanitizer checks on macOS and Linux.
The remote check results are available in the repository's Actions tab.

## Civilisations update

Tests cover autonomous population and district growth, cultural production
bonuses, envoy cost and cooldown, trade prerequisites, import/export resource
conservation, ending routes, alliance relief, full regional save/load and
continued deterministic simulation. Version-one saves migrate, while damaged
civilisation data is rejected without altering the current town.

The native SDL input test opens Civilisations, sends an envoy, and advances the
whole simulation from the regional screen. The view was rendered and visually
inspected; `civilisations.png` records that actual screen.
