# Performance budgets (proposal for MILESTONE-2 and REL-003)

Status: **proposed, partly enforced, awaiting the owner's agreement.**
MILESTONE-2 compares CPU time with the original implementations and REL-003
wants performance checks with budgets. What the engine can enforce on any
machine is enforced already; the rest needs a reference machine the owner
chooses.

## Enforced now: algorithmic scaling

`cna_engine_performance_tests` (label `performance`) times culling plus
sorting, per-instance culling and action updates at n and 16n items and fails
when the work grows faster than n log n (ratio limit 64; measured 16–21). This
holds on any machine and under load, because both sizes run on the same
machine back to back (best of five).

## Proposed: the engine's frame cost next to the pilots'

The scene comparisons time each frame twice: the pilot's renderer and the
engine drawing the same view. Measured (Debug, software renderer, on the
shared development machine at load average ~20):

| Comparison | Pilot | Engine | Engine / pilot |
| --- | --- | --- | --- |
| Street, first viewpoint (no traffic), several runs | 6.0–10.8 s | 6.1–9.9 s | 0.92–1.07 |
| Street with traffic, three views | 10.2–11.8 s | 10.7–11.4 s | 0.96–1.07 |
| car-simulator, day, several runs | 5.2–9.6 s | 6.2–10.0 s | 0.95–1.54 |

The spread comes from the machine, not the code: the same binaries swing by
50 % between runs while other agents build. A budget is therefore proposed,
but not enforced yet:

- **Budget:** the engine's frame time is at most **1.15 ×** the pilot's for
  the same view, both measured as the best of five alternating runs in a
  **Release** build on an otherwise idle reference machine.
- **Scope:** the views of the two scene comparisons (Street with traffic,
  car-simulator by day and at night).
- **Rationale:** the engine and the pilots issue the same draws (identical
  draw-call counts), so the difference is the engine's own CPU work — queue,
  culling, binder — which should stay a small fraction of a frame.

## Decision (2026-10-21)

The owner chose **this development machine, a Release build, measured later
when the machine is idle** as the reference. Until that measurement exists the
budget (1.15 ×, best of five) is recorded but not enforced; when it is taken,
the comparisons time best-of-five and fail on the budget the same way they
fail on the screenshot tolerance (`docs/pilots/screenshot-tolerances.md`).
