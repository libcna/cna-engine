# Verification matrix

Plan item: `REL-003`. What is checked, where, and how a failure shows.

## Check categories

| Category | CTest label / tool | What it proves | Runs in |
| --- | --- | --- | --- |
| Unit | `unit` (gtest executables `cna_engine_tests`, `cna_engine_enhanced_tests`, `cna_engine_extended_tests`) | behaviour of every facility, including software-renderer pixel checks | local, CI |
| Boundary | `boundary`: `strict_boundary`, `strict_boundary_detects_leaks` (must fail), `api_snapshot`; build target `cna_engine_strict_header_check` | strict headers use only XNA/Sharp Runtime/std; every strict header compiles alone; pilot-validated API changes are deliberate | local, CI |
| Shared runtime | `shared-runtime`: engine-owned UP-006 executables in both link orders, plus ELF symbol/dependency inspection | a game may link CNA's shared umbrella beside an engine library without a second CNA runtime | local, CI on native ELF shared-CNA builds |
| Examples | `example`: `quick_start` and `orbit_scene` (strict), `street_slice` (enhanced) | complete programs render, write a PNG and assert their own invariants | local, CI |
| Installed consumers | `tools/run_consumer_probes.sh` (`build-consumer/`) | the installed package works for strict and enhanced consumers; the strict link closure has no cna-extended target | local, CI |
| Pilot suites | `pilot`: `carsim_suite`, `street_suite.*` with `CNA_ENGINE_PILOT_SUITES=ON` | the pilots' own tests pass against the engine's (pinned) dependency tree — ADR-002's consumer checks | local (`build-probe/`) |
| Pilot probes | `pilot` (needs the pilot checkouts) | engine facilities match car-simulator's and street-simulator's own code on identical inputs; car-simulator's checker accepts the strict surface | local only (pilots are separate repositories) |
| Baseline | configure with `CNA_ENGINE_ENFORCE_BASELINE=ON` | dependencies are exactly the pinned revisions with clean tracked files | CI, local baseline runs |
| Sanitizers | `build-asan/` with `CNA_ENGINE_SANITIZE=address,undefined`, all non-pilot tests (UBSan fatal, LeakSanitizer on) | no memory error, undefined behaviour or leak in engine code under the tests | local |
| Upstream compatibility | CI job `cna-extended` (cna-extended's suite and examples at the pins), weekly `upstream-canary` (engine against upstream heads) | the enhanced dependency still works on the pin; upstream API breaks show before a pin move | CI (green since 2026-10-20), local on pin updates |
| Visual | examples' PNG output, pixel-equality pilot probes, scene comparisons with the agreed screenshot tolerance (`docs/pilots/screenshot-tolerances.md`) | rendering matches the pilots exactly where both draw the same thing, and within mean 0.5/255 and 1 % visibly different pixels on whole scenes | local |
| Pilot adoption | the pilots' own suites and before/after captures of each adoption step on their `engine-adoption` branches (`docs/pilots/migration.md`) | the pilots, running on the engine, keep their tests green and their pictures inside the agreed tolerance, on every renderer below | local (pilot worktrees) |
| Performance | `performance`: scaling checks (`cna_engine_performance_tests`) | culling, sorting, instance culling and action updates grow linearly or as n log n (16x the items within 64x the time; measured 16-21x); frame-time budgets proposed in `docs/pilots/performance-budgets.md`, pending a reference machine | local, CI (serial step) |

## Supported combinations

| Axis | Verified | Expected to work, not verified here |
| --- | --- | --- |
| Renderer | `SOFTWARE` (engine suite, examples, scene comparisons, pilot adoption); `OPENGL33` and `OPENGLES3` (pilot adoption captures, on Mesa llvmpipe and on an AMD Radeon 780M through radeonsi) | `VULKAN`, … (engine code uses only the XNA graphics API and `PbrEffect`) |
| Platform | `HEADLESS`; `SDL3` (pilot adoption: under Xvfb, and with SDL's offscreen video driver, which reaches the GPU without a display) | `SDL2` |
| Audio | `NULL` | engine has no audio code |
| Profile | Reach and HiDef (both tested; Reach limits reported) | — |
| Compiler | GCC 14.2, C++23, Debug (engine trees) and Release (pilot adoption builds) | Clang 19 (CNA's other supported compiler) |
| OS | Linux x86-64 | others supported by CNA |

## No silent success

- Skipped or disabled tests fail the CI step "No test may be skipped
  silently".
- Known upstream skips and failures are not run by this repository; they are
  listed in `upstream-issues.md` and in `docs/verification.md` with counts.
- Build and test commands use at most three parallel jobs (`--parallel 3`), in
  CI and locally.
