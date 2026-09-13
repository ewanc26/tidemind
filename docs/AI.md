# Resident learning

## Model and observations

The model is implemented directly in C++ in `src/neural.cpp` with 225 trainable
parameters: 192 input weights, 16 hidden biases, 16 output weights and one output
bias. It has 12 inputs, 16 tanh hidden units and a sigmoid appeal output.

Inputs, in order:

1. Connection to the town-hall street network.
2. Available water / population, capped at one (zero if disconnected).
3. Available power / population, capped at one (zero if disconnected).
4. Local garden and clinic coverage, capped at one.
5. Effective jobs / population, capped at one.
6. Nearby workshop pollution.
7. Tax rate, normalised from 5–20%.
8. Food reserve / three days of demand, capped at one.
9. Water within three tiles (coastal location).
10. Family cohort indicator.
11. Worker cohort indicator.
12. Coastal cohort indicator.

`experiencedUtility()` defines simulated resident wellbeing: essential services
benefit everyone, families value garden coverage more, workers value employment
more, and coastal households value proximity to water more. Tax and pollution
reduce utility. This explicit simulation function produces training labels;
it does **not** pick housing destinations.

The network is seeded and bootstrapped with 6,000 synthetic, normalised
experiences, cycling through all cohorts. Training uses mean squared error,
the full tanh/sigmoid chain rule, stochastic gradient descent and bounded
per-weight steps. The bootstrap uses a separate random generator so changing
training does not change the town's terrain.

Each day, each occupied home/cohort pair provides one observation of experienced
utility. It updates the shared network online at learning rate 0.035. All model
parameters, sample count and exponential moving average of squared error are
persisted with 17-digit floating-point precision. Loading does not retrain or
replace the saved model.

## Gameplay impact

For each cohort, the simulation runs neural inference over eligible homes.
The highest-scoring available connected cottage above the 0.48 threshold
receives an arrival when food, jobs and the economy permit it. One existing
resident per cohort can also move if an available home's score improves on
their current home by more than 0.05.

Construction legality, resource accounting, immigration limits, departure
conditions, campaign goals and council advice remain explicit game rules.
The network learns the preference surface inside these constraints. It does
not control the player's building placement or make up explanations.

The inspector's node colours reflect live input and hidden activations.
Connection lines are a schematic, not a claim of feature attribution. The
output is an actual inference for the selected tile and cohort. Empty tiles
show their **current** services (zero connectivity until built); the overlay
colours existing homes. Prediction error is training MSE, not an accuracy
percentage or an external benchmark score.

## Evidence

The test suite demonstrates lower prediction error after backpropagation,
higher scores for supplied homes, online sample collection, exact neural-state
save/load, and identical continuation after a save.

A counterfactual test starts two identical cities with two available homes,
only one near a garden. It changes **only the neural weights** by training
opposite preferences, then advances both simulations. One model sends new
and relocating residents to the garden home; the other sends them to the
other home. This verifies that neural inference materially controls gameplay.

This is a deliberately small synthetic world model, trained on simulated
preferences rather than surveyed human behaviour. There is no external AI
service, Python runtime, downloaded pretrained model or cloud dependency.
