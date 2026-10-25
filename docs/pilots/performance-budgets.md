# Performance budgets (MILESTONE-2 and REL-003)

Status: **enforced.** The machine-independent scaling checks and the Release
best-of-five frame-time reference check are both executable tests.
MILESTONE-2 compares CPU time with the original implementations and REL-003
wants performance checks with budgets. The scaling checks can run on any
machine; the frame-time gate uses the reference machine and Release
configuration chosen by the owner.

## Enforced now: algorithmic scaling

`cna_engine_performance_tests` (label `performance`) times culling plus
sorting, per-instance culling and action updates at n and 16n items and fails
when the work grows faster than n log n (ratio limit 64; measured 16–21). This
holds on any machine and under load, because both sizes run on the same
machine back to back (best of five).

## Engine frame cost next to the pilots

The scene comparisons time each frame twice: the pilot's renderer and the
engine drawing the same view. Measured (Debug, software renderer, on the
shared development machine at load average ~20):

| Comparison | Pilot | Engine | Engine / pilot |
| --- | --- | --- | --- |
| Street, first viewpoint (no traffic), several runs | 6.0–10.8 s | 6.1–9.9 s | 0.92–1.07 |
| Street with traffic, three views | 10.2–11.8 s | 10.7–11.4 s | 0.96–1.07 |
| car-simulator, day, several runs | 5.2–9.6 s | 6.2–10.0 s | 0.95–1.54 |

The spread comes from the machine, not the code: the same binaries swing by
50 % between runs while other agents build. The enforced budget is:

- **Budget:** the engine's frame time is at most **1.15 ×** the pilot's for
  the same view, both measured as the best of five alternating runs in a
  **Release** build on an otherwise idle reference machine.
- **Scope:** the views of the two scene comparisons (Street with traffic,
  car-simulator by day and at night).
- **Rationale:** the engine and the pilots issue the same draws (identical
  draw-call counts), so the difference is the engine's own CPU work — queue,
  culling, binder — which should stay a small fraction of a frame.

## Release measurements from the pilots' adoption (2026-10-20)

Not the budget measurement (the machine was busy), but the first Release
numbers with the pilots' own renderers on the engine, each before and after
the renderer step, alternating runs on the same machine
(`docs/pilots/migration.md`):

| Pilot | Measure | Before | After | After / before |
| --- | --- | --- | --- | --- |
| car-simulator | world pass, best of 3, 320 × 180, load ~8 | 1,313 ms | 1,358 ms | 1.034 |
| street-simulator | CPU frame, `baseline`, best of 2, load ~22 | 2,068 ms | 1,924 ms | 0.93 |
| street-simulator | CPU frame, `crowd`, best of 2, load ~22 | 1,547 ms | 1,486 ms | 0.96 |

All are inside the 1.15 × budget; the software renderer's rasterizer
dominates these frames, so they bound the engine's overhead only loosely.

On the machine's hardware GPU (AMD Radeon 780M, radeonsi, through SDL's
offscreen video driver; GL builds of both pilots, 1280 × 720, load ~5–8),
where frames are bound by CPU submission and the engine's own work shows:

| Pilot | Measure | Before | After | After / before |
| --- | --- | --- | --- | --- |
| car-simulator | world pass, best of 3 | 6.73 ms | 4.71 ms | 0.70 |
| car-simulator | CPU draw, best of 3 | 9.66 ms | 7.20 ms | 0.75 |
| street-simulator | CPU frame, `baseline`, best of 3 | 11.54 ms | 10.82 ms | 0.94 |
| street-simulator | CPU frame, `crowd` (133 skinned figures), best of 5 | 10.34 ms | 10.34 ms | 1.00 |

Single runs swing by 50 % with the machine's load (a first three-round
`crowd` comparison showed +13 % best-to-best; five rounds gave equal bests),
which is why the budget asks for an idle machine and best of five.

## Decision (2026-10-20)

The owner chose **this development machine, a Release build, measured later
when the machine is idle** as the reference.

## Reference measurement and enforcement (2026-10-25)

`build-probe/` was configured as Release against the exact dependency pins.
Immediately before measuring, `top` reported 98.8 % CPU idle, no I/O wait and
no swap; the one-minute load average was still decaying from the preceding
three-job build. Five sequential paired runs gave:

| Comparison view | Pilot best | Engine best | Engine / pilot |
| --- | ---: | ---: | ---: |
| car, day | 937.1 ms | 956.0 ms | 1.020 |
| car, night | 828.6 ms | 850.4 ms | 1.026 |
| car, headlights | 1,131.3 ms | 1,117.2 ms | 0.988 |
| car, mirror | 310.2 ms | 318.2 ms | 1.026 |
| Street, first viewpoint | 1,324.6 ms | 1,329.3 ms | 1.004 |
| Street, walk | 1,242.7 ms | 1,212.7 ms | 0.976 |
| Street, figure close-up | 1,141.6 ms | 1,125.9 ms | 0.986 |

Every view is inside 1.15×; the worst measured ratio is 1.026. Release builds
with both pilot scene comparisons register the serial
`cna_engine.performance.frame_budget` test. `tools/check_frame_budget.py`
alternates which comparison starts each of five rounds, takes the best pilot
and engine time per view, fails above 1.15×, and refuses to measure when the
one-minute load per CPU exceeds 0.35. This test is the repeatable reference
gate; ordinary Debug CI keeps the machine-independent scaling tests only.
The first registered CTest run passed **1 of 1** in 190.10 seconds.
