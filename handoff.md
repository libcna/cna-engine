# CNA Engine Handoff

Date: 2026-10-19 (supersedes the 2026-10-14 handoff, which is in git history)

Audience: the next AI agent working on `cna-engine`

## Mission

Build a higher-level C++23 game-engine library above CNA that makes ordinary
game development materially easier than CNA's XNA 4.0-compatible API alone,
without hiding CNA, and reusing `cna-extended` instead of duplicating it.
Validation projects: `libcnalab/street-simulator` (enhanced profile) and
`libcnalab/car-simulator` (strict-XNA profile). Both moved from
`libcna/cna-street` and `libcna/cna-car-simulator` after the original audit.

## Owner constraints (binding)

- **Modify only this repository.** Do not edit `cna`, `sharp-runtime`,
  `cna-extended`, the simulators or any other app. Defects found elsewhere are
  written to `upstream-issues.md` (UP-001 … UP-004 so far).
- Commit regularly on branch `develop` (the owner created it from `main` and
  pushed it; do not push unless asked).
- Builds and tests: at most 3 parallel jobs (`--parallel 3`), shared ccache
  `CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv`, never a new cache, never
  `CCACHE_MAXSIZE`, never build in `/tmp` or the scratchpad, only the build
  directory names `build`, `build-probe`, `build-consumer`, `build-asan`,
  `build-ubsan`, `build-tsan`. Read `../CLAUDE.md` (= `../AGENTS.md`).
- Export `CCACHE_NOHASHDIR=1` as well, otherwise Debug objects are never
  shared between build trees (UP-003).
- Documents stay in English.

## State at hand-off

All work is committed on `develop`. `ctest --test-dir build --parallel 3`
passes (see `docs/verification.md` for the latest exact numbers).

Build trees:

| Directory | Content |
| --- | --- |
| `build/` | engine build (Debug, SOFTWARE/HEADLESS/NULL, with cna-extended) |
| `build-consumer/` | installed-package consumer probes (`tools/run_consumer_probes.sh`) |
| `build-probe/` | the 2026-10-14 cna-extended audit tree (configured against an older CNA); reuse it for the ADR-002 cna-extended rerun |

### Done

- ADR-001 (strict/enhanced boundary) and ADR-002 (pinned baseline, configure
  check) in `docs/adr/`.
- CMake skeleton with `CnaEngine::Core`, `::Graphics` (strict),
  `::ExtendedIntegration` (only cna-extended user), `::Engine`; install and
  `CnaEngineConfig.cmake`.
- Strict boundary checker (`tools/check_strict_boundary.py`, car-simulator
  rules) + deliberate-leak fixture + per-header self-containment build.
- Strict facilities: input actions/axes/rebinding/persistence/binding capture;
  camera + fly/walk controllers; SoA mesh data, builder, vertex layouts;
  RAII GPU meshes; materials; render queue; visibility + LOD; forward renderer
  over stock effects with custom passes, state restore and reported fallbacks;
  quality presets with capability resolution; instancing (hardware path +
  per-instance fallback); screenshots; asset registry; frame statistics and
  CPU profiler.
- Enhanced facilities: `EngineWorldScreen` (World3DScreenEXT driven by engine
  input/camera), World3D camera sync.
- Example `examples/orbit_scene` (headless, strict only, writes a PNG).
- Consumer probes `tests/consumer` (installed package; strict and enhanced).
- EXT-002 dispositioned: CNA defect, UP-004.

### Open, in priority order

1. ADR-002 reruns against the pin: cna-extended suite (reuse `build-probe/`),
   street-simulator, car-simulator (they need build trees; use
   `build-consumer/`-style reuse, never per-ticket directories).
2. Pilot validation (PILOT-001/002): express each simulator's core controls
   with `ActionMap`, replace one local cache with `AssetRegistry`, drive one
   camera through the engine — as integration probes inside this repository
   (the simulators themselves must not be modified).
3. ENG-035 later steps: shadows/cascades, post-processing hooks (only when a
   pilot needs them); ENG-037 debug presentation via cna-extended fonts.
4. CI job definition (EXT-003, REL-003).

## Pointers

- `plan.md` — task list with `[x]` / `[~]` / `[ ]` status and per-task results
- `docs/conventions.md`, `docs/api-status.md`, `docs/verification.md`
- `upstream-issues.md` — recorded defects elsewhere
- `analysis.md`, `gap-audit.md` — the original study and audit
