# Contributing to Tidemind

Tidemind is a playable, offline C++20 city builder where a small neural network learns what makes a neighbourhood a good home: build an isometric estuary town, balance food, water, energy, jobs and money, and grow from 24 settlers to an independent town. Thank you for wanting to help!

## Getting started

```sh
brew install cmake sdl2 sdl2_ttf   # macOS (Ubuntu: build-essential cmake libsdl2-dev libsdl2-ttf-dev)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/tidemind
```

No account, model download, GPU compute library, API key or internet connection is needed; a redistributable font is included.

## Notes for this project

- **Offline forever** is a design commitment, like "the game starts paused" and "an existing save restores automatically, paused". Don't add connectivity or accounts.
- The neural network is part of the experience: households value gardens, employment and the coast differently, and their learned preferences drive where they move. Keep it small, inspectable and trained locally; the appeal overlay (Tab) must keep reflecting what the network actually predicts.
- The economy rules in the README are the tuned game, not placeholders (farm yields, 35% winter reduction, 0.65 food/resident/day, tax appeal trade-off, one-third demolition refunds). Balance changes deserve an issue with play evidence first.
- Street connectivity semantics are exact: a building must touch the hall or a connected street, and buildings do not pass the connection onward — a "fix" that changes this is a gameplay redesign.
- Win/lose conditions (sustain 180 residents, 65% wellbeing, 10 days independence; ten consecutive days below −500 coins ends the campaign) are the campaign contract; changing them changes the game.

## Before opening a PR

- The build passes and you played the affected part of the game, ideally through a winter.
- Conventional commits; one focused PR per change.
- AI-assisted PRs are welcome; stand behind every line.

## Licence

AGPL-3.0 (see [LICENSE](LICENSE)). Contributions are accepted under the same terms.