# Pilot: car-simulator (strict-XNA)

Plan item: `PILOT-002`. The pilot is `libcnalab/car-simulator` (MIT). It is
never modified from this repository; probes compile selected pilot sources
read-only next to engine code (`tests/pilots/`) and compare behaviour on
identical inputs.

## Input actions (ENG-020) — validated

Probe: `tests/pilots/CarSimulatorControlsTests.cpp`, compiled with
`simulator/src/Input/InputMapper.cpp` from the pilot checkout.

The engine `ActionMap` is built from the pilot's own default tables
(`InputMapper::DefaultBindings()`, `DefaultPadBindings()`): 51 actions, every
keyboard and pad binding, plus the analog-derived holds (triggers past half
travel, curved stick past half). On 4 × 1500 frames of deterministic random
input (overlapping keys, pad buttons, analog levels, pad disconnects):

| Behaviour | Result |
| --- | --- |
| `Held` for every action | identical on every frame |
| `Pressed` | identical except one documented difference (below), checked as an exact relation |
| Throttle, brake, steering (`DriverControls`) | identical to `BuildDriverControls()` on every frame |
| Binding text round trip | the full binding set saves and reloads unchanged |
| Shared keys (Shift: `ShiftUp` / `ToggleRun`) | reported by `FindConflicts()`, not rejected |

### Generalizations made because of this pilot

- **Response curves**: `AxisBinding::ForGamePad(..., exponent)` — the pilot
  steers with `|x|^1.6`.
- **Axis composition**: key pairs form one digital composite (opposite keys
  cancel, same-direction keys do not add) and compete with analog inputs by
  largest magnitude, keys winning ties. This is the pilot's
  `DriverControls` rule and matches common engine practice.

### Intentional difference

`Pressed`: the pilot reports a press whenever *any binding* goes down, even
while the action is already held through another binding (holding W, then
pressing Up, presses Throttle again). The engine reports *action-level*
edges: an action is pressed only when it goes from released to held. The probe
asserts `engine.Pressed == pilot.Pressed && !heldLastFrame` for digital
inputs, and confirms the difference occurs in the random histories. For
analog-derived holds the pilot never reports a press; the engine does when the
threshold is crossed.

Adopting the engine in the pilot would change toggles (e.g. `ToggleHelp`
bound to one key only) in no observable way; it would only suppress repeated
toggles from a second key while the first is held.

### Not covered

- Steering-wheel profile (`wheel == true`: linear steering). A game expresses
  it by swapping the stick binding's exponent to 1 when it detects a wheel;
  device-type detection stays in the game.
- Key and button display names (the pilot's are localized-style strings such
  as "Left Shift"); the engine provides stable identifiers only.

## Still open for this pilot

Camera data, mesh data/upload, diagnostics and screenshot capture probes
(the plan requires at least two engine abstractions exercised by real pilot
code; input is the first).
