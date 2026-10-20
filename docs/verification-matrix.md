# Verification matrix

Plan item: `REL-003`. What is checked, where, and how a failure shows.

## Check categories

| Category | CTest label / tool | What it proves | Runs in |
| --- | --- | --- | --- |
| Unit | `unit` (gtest executables `cna_engine_tests`, `cna_engine_enhanced_tests`, `cna_engine_extended_tests`) | behaviour of every facility, including software-renderer pixel checks | local, CI |
| Boundary | `boundary`: `strict_boundary`, `strict_boundary_detects_leaks` (must fail), `api_snapshot`; build target `cna_engine_strict_header_check` | strict headers use only XNA/Sharp Runtime/std; every strict header compiles alone; pilot-validated API changes are deliberate | local, CI |
| Examples | `example`: `quick_start` and `orbit_scene` (strict), `street_slice` (enhanced) | complete programs render, write a PNG and assert their own invariants | local, CI |
| Installed consumers | `tools/run_consumer_probes.sh` (`build-consumer/`) | the installed package works for strict and enhanced consumers; the strict link closure has no cna-extended target | local, CI |
| Pilot probes | `pilot` (needs the pilot checkouts) | engine facilities match car-simulator's and street-simulator's own code on identical inputs; car-simulator's checker accepts the strict surface | local only (pilots are separate repositories) |
| Baseline | configure with `CNA_ENGINE_ENFORCE_BASELINE=ON` | dependencies are exactly the pinned revisions with clean tracked files | CI, local baseline runs |
| Upstream compatibility | cna-extended suite against the pinned CNA (`build-probe/`) | the enhanced dependency still works on the pin | local, on pin updates |
| Visual | examples' PNG output, pixel-equality pilot probes | rendering matches the pilots exactly where both draw the same thing | local |
| Performance | not yet automated | — (no pilot-agreed budget yet; see MILESTONE-2) | — |

## Supported combinations

| Axis | Verified | Expected to work, not verified here |
| --- | --- | --- |
| Renderer | `SOFTWARE` | `OPENGL33`, `OPENGLES3`, `VULKAN`, … (engine code uses only the XNA graphics API and `PbrEffect`) |
| Platform | `HEADLESS` | `SDL3`, `SDL2` |
| Audio | `NULL` | engine has no audio code |
| Profile | Reach and HiDef (both tested; Reach limits reported) | — |
| Compiler | GCC 14.2, C++23, Debug | Clang 19 (CNA's other supported compiler), Release |
| OS | Linux x86-64 | others supported by CNA |

## No silent success

- Skipped or disabled tests fail the CI step "No test may be skipped
  silently".
- Known upstream skips and failures are not run by this repository; they are
  listed in `upstream-issues.md` and in `docs/verification.md` with counts.
- Build and test commands use at most three parallel jobs (`--parallel 3`), in
  CI and locally.
