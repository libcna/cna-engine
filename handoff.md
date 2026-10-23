# CNA Engine Handoff

Date: 2026-10-23. Audience: the next AI agent, starting in a fresh context.
Supersedes the earlier handoffs (they are in git history).

Read this file first, then `plan.md` (tasks and their results),
`upstream-issues.md` (defects found elsewhere) and `docs/verification.md`
(evidence, newest first). Everything below is true as of `develop` at
`cfab224`.

## Mission

`cna-engine` is a C++23 game-engine library above CNA (a C++ implementation of
XNA 4.0) that makes ordinary game development easier than CNA alone, without
hiding CNA and reusing `cna-extended` instead of duplicating it. It has a
strict profile (XNA 4.0 types only: `CnaEngine::Core`, `Graphics`, `App`,
`Audio`, `UI`) and an enhanced profile (CNA extensions: `EnhancedGraphics`;
cna-extended: `ExtendedIntegration`). Seven games use it.

## Binding rules

From the owner (Robert Vokac) and the workspace (`../CLAUDE.md`,
`../AGENTS.md`):

- **Where you may change code.** This repository always. `libcna/cna-extended`
  too: its owner delegated its fixes on 2026-10-22 (work in a separate git
  worktree and branch, fast-forward `develop` when done). The games only when
  the owner asks for a specific change in a specific game, each time on a new
  branch in a separate worktree (`libcnalab/<game>-engine`), one revertible
  commit per step, with screenshot comparisons. island-, museum-, house- and
  room-simulator are never to be changed. **Never edit CNA or Sharp
  Runtime.**
- **CNA and Sharp Runtime defects** go to the Codex session `cna-demos`, which
  works on CNA:
  `codex queue --thread 01a15506-a40f-7dc0-9c2f-72e9ed1d6f8e --message "..."`.
  Ask it to name the issue (`cna-engine UP-0NN`) in its commit messages, then
  watch CNA's history (`git -C ../cna log --all --grep 'UP-0NN'`) and its
  notes in `../cna/CNA_ENGINE_REPAIR_HANDOFF.md`. Codex may not push; CNA or
  Sharp Runtime get published only when the owner pushes or authorizes it.
  Other projects' defects are recorded in `upstream-issues.md` and, if the
  owner asks, in a known-issues file of the game itself.
- **Push only when the owner asks**, every time; approval for one push does
  not cover the next. Commit regularly on `develop`. No attribution lines in
  commits. Documents in English (the owner writes in Czech; answer in Czech).
- **Builds.** Shared ccache only: `CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv`
  (plus `CCACHE_NOHASHDIR=1`; the cache sets `hash_dir = false` since
  2026-10-22). Never a new cache, never `CCACHE_MAXSIZE`, never build in
  `/tmp` or the scratchpad, build directories only from the closed list
  `build/`, `build-asan/`, `build-ubsan/`, `build-tsan/`, `build-probe/`,
  `build-consumer/`. Jobs: at most 3 for this repository, 2 for
  school-simulator, 4 for train-simulator, 8 anywhere. One heavy job at a
  time from one session. The machine has 30 GB RAM, no swap, ~10 agents.
- **Processes.** Never kill by pattern (`pkill -f`, `pgrep -f`), kill by PID.
  For X11 runs start a private `Xvfb -displayfd` and kill only its PID; never
  touch other sessions' Xvfb or `/tmp/xvfb-run.*`. Before deleting anything
  outside your own output, check that no live process uses it.

## Repositories and their state (2026-10-23)

| Repository | Branch | Head | Pushed |
| --- | --- | --- | --- |
| `libcna/cna-engine` | `develop` | `cfab224` | yes |
| `libcna/cna-extended` | `develop` | `c19c6e5` | yes |
| `libcnalab/backrooms-simulator` | `main` | `12527a7` (shadows, pause menu) | yes |
| `libcnalab/school-simulator` | `main` | `c1aee2a` (sun shadows, pause menu) | yes |
| `libcnalab/car-simulator` | `main` | `3a34c80` (`docs/known-issues.md`) | yes |
| `libcnalab/street-simulator` | `develop` | `14145cd` (`docs/known-issues.md`) | yes |
| `libcnalab/train-simulator` | `develop` | `2fd191d` | yes |
| `openeggbert/galaxy-eggbert` | `develop` | `5adf55b` (editor bug note) | yes |

CI for `cfab224` (engine) and `c19c6e5` (cna-extended) was still running at
handoff; check `gh run list -R libcna/cna-engine --branch develop` first. The
engine job now also builds CNA's C++ SDK in the consumer probes, so it runs
longer than before (it had taken 15 to 25 minutes).

Two game worktrees remain, merged and pushed, removable when the owner agrees:
`libcnalab/backrooms-simulator-engine` and `libcnalab/school-simulator-engine`
(branch `engine-shadows-menu`; desktop `build/` and browser `build-consumer/`
inside).

## Dependency pins (ADR-002)

| Dependency | Pin | Notes |
| --- | --- | --- |
| CNA | `cb377fa04` (`next`) | installs a relocatable C++ SDK (`CNA_INSTALL_CXX`, UP-001) |
| Sharp Runtime | `1dc8785b` (`next`) | needed by CNA since `abbd8a4ea` (UP-020) |
| cna-extended | `c19c6e5` (`develop`) | 0.2.0, installs a `CnaExtended` package |
| easy-gl / meta-gl | `f4d6afa` / `dd3b300` | CNA requires easy-gl 0.1.1, meta-gl 0.4.1 |

They live in `cmake/CnaEngineBaseline.cmake`, `.github/workflows/ci.yml` (env)
and `docs/adr/0002-compatibility-baseline.md` (table); change all three in one
commit. Local runs use clean clones at the pins, never the shared `../cna` or
`../sharp-runtime` worktrees (other agents edit them):
`~/deps/cna-baseline` and `~/deps/sharp-runtime-baseline` (their `origin` is
the local shared worktree: `git -C <clone> fetch && git -C <clone> checkout <rev>`);
cna-extended is used from `../cna-extended` (keep it clean at the pin).

Moving a pin: check that the candidate is **published** (`git ls-remote
git@github-libcna:libcna/<repo> refs/heads/next`) and builds against the other
published pins, then run (1) the engine suite with
`-DCNA_ENGINE_ENFORCE_BASELINE=ON`, (2) `tools/run_consumer_probes.sh`, (3)
cna-extended's suite against the clones, (4) the pilot suites in
`build-probe/`, and record the results in `docs/verification.md`.

## Build trees

| Directory | Content |
| --- | --- |
| `build/` (3 GB) | day-to-day Debug build against the clones: `-DCNA_ENGINE_CNA_DIR=$HOME/deps/cna-baseline -DCNA_ENGINE_SHARP_RUNTIME_DIR=$HOME/deps/sharp-runtime-baseline -DCNA_ENABLE_SDL=OFF -DCNA_ENABLE_VIDEO=OFF -DCNA_ENGINE_GOOGLETEST_DIR=../cna-extended/vendor/googletest`; SOFTWARE renderer, headless, null audio |
| `build-probe/` (3.6 GB) | the same plus `-DCNA_ENGINE_ENFORCE_BASELINE=ON -DCNA_ENGINE_CNA_CNAEXT=ON -DCNA_ENGINE_PILOT_SUITES=ON`: the pilots' own suites (`ctest -L pilot`, 50 tests; car's suite alone ~15 min) |
| `build-consumer/` (5.5 GB) | `tools/run_consumer_probes.sh build build-consumer`: `cna-sdk/` (CNA's SDK build), `extended-package/` (cna-extended's), `install/` (the prefix), the three consumers; `new_game/` is a game written by `tools/new_game.sh` |

`../cna-extended/build/` is cna-extended's own suite (Debug, against the
clones; `ctest --test-dir build`: 2372 tests, none may fail or skip).

## Common commands

```sh
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv CCACHE_NOHASHDIR=1
cmake --build build --parallel 3 && (cd build && ctest -j3 -E carsim_suite)   # 339 tests
tools/run_consumer_probes.sh build build-consumer                             # 3 probes
python3 tools/api_snapshot.py --root . --check      # pilot-validated headers; --update after a deliberate change, plus CHANGELOG
cmake --build build-probe --parallel 3 && (cd build-probe && ctest -j3 -L pilot)
```

Games build against the same clones plus
`-DCNA_EASYGL_ROOT=/rv/data/development/github.com/libcna/easy-gl
-DCNA_ENABLE_DRACO=OFF -DCNA_USE_SYSTEM_SDL=ON
-DCMAKE_PREFIX_PATH=/rv/data/development/github.com/libcna/cna/.sdl-prebuilt-Linux-x86_64-wayland/install`
and `-DCNA_ENGINE_ROOT=<this checkout>` (there is no `libcnalab/cna-engine`).
Browser builds: Emscripten from `~/emsdk`, SDL from the CNA checkout's
`.sdl-prebuilt-emscripten/install` (`-DSDL3_DIR=… -DSDL3_mixer_DIR=…`,
read-only), into the game's `build-consumer/`.

Pictures: the machine's Radeon 780M headless with
`env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEODRIVER=offscreen`; Mesa llvmpipe
under a private Xvfb. Real input: `xdotool keydown`, wait ~0.15 s, `keyup`
(the games poll input; a `key` tap between two frames is missed). Browser
smoke tests: school-simulator's `tools/web_drive.mjs` (headless Chrome,
SwiftShader) behind a static server; hold keys ~0.9 s there (slow frames).

## What exists (details in `plan.md`)

- Phases 0–6: strict foundation (input actions, cameras, meshes, materials and
  binders, render queue, forward renderer, culling, instancing, skinning,
  quality schemas, screenshots, assets, diagnostics), the PBR binder, the
  cna-extended adapters, two pilots (car-, street-simulator) adopted and
  merged, release material (LICENSE, NOTICE, CHANGELOG, API snapshot of 23
  pilot-validated headers, guides, CI with an upstream lane and a canary).
- Adoptions: galaxy-eggbert (from easy-3d), backrooms-, school- and
  train-simulator; SHARE-001 facilities (Image/mips, Noise, SunPosition,
  SettingsFile, AutomatedRun) in car, school, train, street.
- Phase 7 (GAME-001…065): a new game from CNA and the engine alone —
  `cna_engine_add_game` (desktop and browser), `tools/fetch_dependencies.sh`,
  `tools/new_game.sh` with `templates/game`, `App/EngineGame`, models and
  animation, collision and a character controller, sky and time of day, a sun
  shadow map (also on baked light: `Material::bakedKeyLight`), sound, text,
  HUD and menus, post-processing, local lights, particles, save slots,
  `docs/tutorial.md`, examples run as tests; shadows and pause menus adopted
  in backrooms and school.
- Packages: a game can be built from installed CNA (C++ SDK), cna-extended
  and engine packages alone; `CnaEngineConfig.cmake` finds CNA's SDK and
  cna-extended's package when the consumer has not defined their targets.

## Open

1. **REL-001**: version ranges for CNA and cna-extended, once releases are
   tagged (e.g. cna-extended `v0.2.0`, engine `v0.1.0`). Tags only with the
   owner's word.
2. **REL-003**: enforce the frame-time budget (1.15x, best of five), which
   waits for a measurement in a Release build on an idle machine
   (`docs/pilots/performance-budgets.md`).
3. **SHARE-001**: only UP-017 (street-simulator's `--capture` runs out of
   memory) remains, the game owner's. Never retry that capture on this
   machine; verify street with `bake-assets --content`.
4. Upstream records still open, all recorded in the games' own files:
   UP-008, 010, 013, 017 (street), UP-009, 016 (car), UP-014 (galaxy-eggbert).
   Every report sent to `cna-demos` is closed.

## Gotchas learned the hard way

- XNA render-target formats are preferences: CNA's default `GraphicsDevice`
  is **Reach**, where `Single` becomes `Color`; read
  `getFormatProperty()` (UP-018 was this mistake in the engine). Games that
  need float shadow maps set HiDef.
- CTest runs each GoogleTest test of a fixture as its own process, in
  parallel: give every test its own scratch directory, and assert an
  `optional` before dereferencing it.
- CNA's shadow receivers darken only direct light. For baked (unlit,
  vertex-coloured) worlds use `Material::bakedKeyLight` with normals in the
  mesh (`VertexLayout::PositionNormalColorTexture`).
- `CalibrateShadowMap` still measures the receiver orientation per device
  (UP-019 is fixed upstream; the test expects "not upside down" now).
- The first mouse read after switching relative mode on returns the
  pointer's position, not a motion: skip it (school and backrooms do).
- Never name a GoogleTest helper `Run` (it hides `testing::Test::Run`).
- A published CNA revision can depend on unpublished Sharp Runtime commits
  (UP-020): CNA's version check passes and the build fails at compile time.
- The strict boundary checker allows private same-directory headers in
  `src/`; anything public in a strict module may use XNA 4.0 types only.
- `CNA_SHARED_LIBRARY`: the engine and every game build CNA statically. CNA
  fixed the mixed-runtime problem (UP-006), but the engine has not checked a
  mixed consumer itself, so the guidance stays.

## Pointers

- `plan.md` — every task with `[x]`/`[~]`/`[ ]` and its result
- `upstream-issues.md` — UP-001 … UP-020 with status and evidence
- `docs/verification.md` (evidence, newest first), `docs/verification-matrix.md`
- `docs/adr/` (ADR-001 profiles, ADR-002 baseline, ADR-003 materials and binders)
- `README.md`, `docs/tutorial.md`, `docs/guide-strict.md`, `docs/guide-enhanced.md`,
  `docs/conventions.md`, `docs/api-status.md`, `CHANGELOG.md`, `NOTICE.md`
- `docs/game-facilities-survey.md`, `docs/pilots/` — evidence from the games
