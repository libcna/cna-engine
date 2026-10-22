# CNA Engine Handoff

Date: 2026-10-20 (first written 2026-10-19; supersedes the 2026-10-14
handoff, which is in git history)

Audience: the next AI agent working on `cna-engine`

## Mission

Build a higher-level C++23 game-engine library above CNA that makes ordinary
game development materially easier than CNA's XNA 4.0-compatible API alone,
without hiding CNA, and reusing `cna-extended` instead of duplicating it.
Pilots: `libcnalab/street-simulator` (enhanced profile) and
`libcnalab/car-simulator` (strict-XNA profile). Both moved from
`libcna/cna-street` and `libcna/cna-car-simulator` after the original audit.

## Owner constraints (binding)

- **Modify only this repository.** Do not edit `cna`, `sharp-runtime`,
  `cna-extended`, the simulators or any other app — with one exception granted
  on 2026-10-20: car-simulator and street-simulator may be changed **on new
  branches** to adopt the engine (one revertible commit per step of
  `docs/pilots/migration.md`; the branches were pushed at the owner's
  request on 2026-10-20). A second exception the same day: galaxy-eggbert
  moves from easy-3d to the engine on a new branch (PILOT-004; merged and
  pushed at the owner's request). On 2026-10-21 the owner allowed the same
  for backrooms-simulator and school-simulator (PILOT-005; branches
  `engine-adoption`, merged into both games' `main` and pushed at the
  owner's request), and later for train-simulator (PILOT-006; branch
  `engine-adoption`, merged into its `develop` and pushed at the owner's
  request); island, museum, house and room
  were only surveyed and stay untouched. Defects found elsewhere are
  written to `upstream-issues.md` (UP-001 … UP-015 so far). Pilot code may be
  *read and compiled read-only* (that is what `tests/pilots/` does).
- Commit regularly on branch `develop`; do not push unless asked.
- At most 3 parallel compile/test jobs. Shared ccache only:
  `CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv`, plus `CCACHE_NOHASHDIR=1`
  (UP-003). Never a new cache, never `CCACHE_MAXSIZE`, never build in `/tmp`
  or the scratchpad, only the build directory names of `../CLAUDE.md`.
- Documents stay in English.

## Build trees

| Directory | Content |
| --- | --- |
| `build/` | day-to-day engine build; since 2026-10-22 against the baseline clones `~/deps/cna-baseline` and `~/deps/sharp-runtime-baseline` (the shared `../cna` is being edited by another agent), `CNA_ENABLE_SDL=OFF`, `CNA_ENABLE_VIDEO=OFF`, GoogleTest from `../cna-extended/vendor/googletest` (the baseline Sharp Runtime clone has no submodules) |
| `build-probe/` | baseline build against `~/deps/cna-baseline` and `~/deps/sharp-runtime-baseline` (clean clones at the pins) with `CNA_ENGINE_ENFORCE_BASELINE=ON`, `CNA_ENGINE_CNA_CNAEXT=ON` and `CNA_ENGINE_PILOT_SUITES=ON`; the only tree that builds the Street scene comparison and runs both pilots' own suites (the car-simulator suite alone takes ~15 min; `-E "carsim_suite"` skips it) |
| `build-consumer/` | installed-package consumer probes (`tools/run_consumer_probes.sh`); `build-consumer/cna-extended/` is cna-extended's own suite against the pinned clones (CI's lane configuration, 2.0 GB, kept for the next pin move) |
| `build-probe/` (2026-10-22) | an Emscripten build of the engine's examples (WebGL 2, the CNA checkout's `.sdl-prebuilt-emscripten` read-only) for the browser path of `cna_engine_add_game` |
| `build-consumer/new_game/` (2026-10-22) | a game written by `tools/new_game.sh` and built on its own (`build/` OpenGL ES 3 with the CNA checkout's Linux SDL prebuilt, `build-web/` Emscripten) -- a new user's path |
| `build-asan/` | `CNA_ENGINE_SANITIZE=address,undefined`, pilot probes off, shared CNA worktree; engine targets instrumented only |

`~/deps/cna-baseline` and `~/deps/sharp-runtime-baseline` are local clones of
CNA and Sharp Runtime at the pins (`build-probe/` uses both; cna-extended's
shared worktree is still clean at its pin); move them with
`git -C <clone> fetch && git -C <clone> checkout <rev>` when a pin changes
(ADR-002 "Local reproduction").

## State

**Shadows and menus in backrooms and school (2026-10-22, GAME-064/065).**
Asked by the owner after pushing Phase 7: both games bake their light into
unlit vertex colours, which CNA's shadow receivers do not darken, so the
engine got `Material::bakedKeyLight` (the key light's share of baked light,
drawn through the lit BasicEffect). Both games now have shadows and an
engine pause menu with saved settings on branch `engine-shadows-menu` in
the worktrees `libcnalab/backrooms-simulator-engine` and
`libcnalab/school-simulator-engine` (desktop `build/` and browser
`build-consumer/` there) -- not merged or pushed; that needs the owner's
word. The engine commits after `172e839` (`2d893cb`, `fd5a478` and the
records) are not pushed either. Details in `plan.md` GAME-065 and
`docs/verification.md`.

**Phase 7 (2026-10-22, `plan.md`): a new game from CNA and the engine
alone.** Asked by the owner after the SHARE-001 work: everything is built,
tested and committed on `develop` (pushed 2026-10-22, `172e839`). Part A: `cna_engine_add_game`
and `cmake/CnaEngineGame.cmake` (desktop and browser), `tools/fetch_dependencies.sh`,
`App/EngineGame`, `templates/game` with `tools/new_game.sh`, `docs/tutorial.md`
and three examples run as tests. Part B: models (`Graphics/ModelView`,
`Enhanced/ModelImport`), animation (`Animation/`), collision and a character
controller (`Collision/`), sky and time of day (`Graphics/Sky`), a sun
shadow map (`Graphics/ShadowMap`, reception in `Enhanced/ShadowReception`,
calibrated per device: UP-019), sound (`Audio/`), text, HUD and menus
(`UI/`). Part C: post-processing, local lights, cna-extended particles,
save slots. New strict targets `CnaEngine::App`, `CnaEngine::Audio`,
`CnaEngine::UI`; the boundary checker's strict modules grew by App,
Collision, Animation, Audio and UI. The evidence from the games is in
`docs/game-facilities-survey.md`; new upstream findings UP-018 and UP-019.
No game was changed: adopting any of this in a game needs the owner's
request.

All work is committed on `develop`. Latest evidence (`docs/verification.md`):
213 tests pass in `build/` (21:40, with the easy-3d facilities and their
equivalence probe), 213 in the baseline build (17:50, before them; `build-probe/`, which
also runs the Street scene comparison and both pilots' own suites; the
car-simulator suite alone takes ~15 min and excludes UP-009's test by name);
2 consumer probes pass.

Both pilots' real scenes render through the engine's `ForwardRenderer`
practically identically to the pilots' own renderers: Street's procedural
city with the PBR binder carrying Street's lighting and its sky as a custom
pass, traffic and pedestrians included (vehicles as dynamic items, people
as skinned render items; visible items, instances, figures and all 1,284
draw calls identical; mean image difference 0.01/255) and car-simulator's
`lipova` map with the strict stock binder (every visibility count and all
draw calls identical by day, at night, with the headlights on and in a
rear-view mirror — additive lamp light, the headlight fill as nested
renderers and the mirrored camera included; 0.05/255 by day). Both
comparisons enforce the agreed screenshot tolerance. See `docs/pilots/`.

car-simulator itself now runs on the engine on its branch `engine-adoption`
(worktree `libcnalab/car-simulator-engine`, build `build/software` there):
its world renderer draws through `ForwardRenderer` with car-simulator's own
matrices (`Camera::SetMatrices`), its suite and checker are green and its
captures stay inside the tolerance. street-simulator does the same on its
branch `engine-adoption` (worktree `libcnalab/street-simulator-engine`,
build `build/` there): `SceneRenderer` draws through `ForwardRenderer` and
the `PbrMaterialBinder`, its suite is green and all its named viewpoints
stay inside the tolerance.

The shared `../cna` worktree is changed by other agents all the time (on
2026-10-20 e.g. XNA-exact `BoundingFrustum` containment and GJK
`Intersects`, CNA `5ab1c7729`, which also resolves UP-004); `build/` results
move with it, `build-probe/` (pinned) is the evidence.

Done (see `plan.md` for per-task results):

- ADR-001 (+ addendum for `EnhancedGraphics`), ADR-002 with a clean,
  enforced baseline run, ADR-003 (what a material describes and what a binder
  decides — read it before adding material fields).
- MILESTONE-1 (strict foundation): ENG-020 … ENG-025 complete, validated
  against both pilots by read-only equivalence probes (`docs/pilots/`); the
  pilot's own strict checker accepts the strict surface with the ADR-001
  allowance.
- Rendering (ENG-031 … ENG-034 complete): materials with per-material
  samplers and premultiplied/straight blending chosen by the binder, the
  stock binder (BasicEffect, AlphaTestEffect, DualTextureEffect,
  SkinnedEffect) and the PBR binder (PbrEffect, SkinnedPbrEffect, image-based
  environment, exposure, per-item environment selector for reflection
  probes), render queue and state contract, culling/LOD with diagnostics,
  instancing, skinned render items.
- Enhanced integration: `EngineWorldScreen`, debug overlay, quality schema
  (ENG-036), `street_slice` example.
- Release readiness: LICENSE, NOTICE, CHANGELOG, API snapshot of the 21
  pilot-validated headers, profile guides, `quick_start` game example,
  verification matrix, CI workflow with the EXT-003 upstream lane and canary
  (all green on GitHub since 2026-10-20).

## Open, in priority order

0. SHARE-001 (2026-10-21): four facilities taken from what the simulators
   each wrote — `Graphics/Image.hpp` with `Core/Noise.hpp`,
   `Core/SunPosition.hpp`, `Core/SettingsFile.hpp`, `Graphics/AutomatedRun.hpp`
   — experimental, with probes against car-, train-, school-, room-, street-
   and house-simulator (`plan.md`). Adopted on 2026-10-21 in car-, school-,
   train- and street-simulator, each on a branch `engine-shared` in its
   `libcnalab/*-engine` worktree (car and school: mips; train: mips,
   settings, automated runs; street: noise), each game's pictures or baked
   surfaces identical to those from before the adoption (apart from the
   train's two avatar views, which vary between runs anyway); the probes
   now compare frozen copies of the
   replaced code (`tests/fixtures/simulator_oracles`). The branches were
   fast-forwarded into car's and school's `main` and train's and street's
   `develop` and pushed on 2026-10-22. Open: UP-017 (street's capture runs
   out of memory).
1. Pilot adoption (PILOT-001/002/003) is done: both `engine-adoption`
   branches are merged into car-simulator's `main` and street-simulator's
   `develop` (2026-10-20). car-simulator:
   done on branch `engine-adoption` in the worktree
   `libcnalab/car-simulator-engine` (eight commits; the camera stays the
   game's, see `docs/pilots/migration.md`). street-simulator: done on
   branch `engine-adoption` in the worktree
   `libcnalab/street-simulator-engine` (five commits: build, mesh views,
   `SceneRenderer` through the engine, presets as a `QualitySchema`,
   derived materials in an `AssetRegistry`). What
   is left for both: nothing; both are checked on the
   software renderer, on OpenGL via Mesa llvmpipe and on the machine's AMD
   GPU (`SDL_VIDEODRIVER=offscreen` reaches it headless).
   PILOT-004: galaxy-eggbert runs on the engine instead of easy-3d on its
   branch `engine-adoption` (worktree `openeggbert/galaxy-eggbert-engine`,
   builds `build/` = `OPENGLES3`, `build-probe/` = `OPENGL33`; eight
   commits; merged and pushed by the owner). Every step left its suite green
   and its captures byte-identical. Its minimal equivalence oracle is frozen
   and MIT-attributed under `tests/fixtures/easy3d_legacy`; no easy-3d checkout
   is needed. PILOT-004 is complete and easy-3d may be archived or removed.
   PILOT-005: backrooms-simulator runs on the engine on its branch
   `engine-adoption` (worktree `libcnalab/backrooms-simulator-engine`, build
   `build/` = Release `OPENGLES3` against the baseline clones and easy-gl
   `v0.1.1`; eight commits; 30 captures byte-identical after
   every step, see `docs/pilots/migration.md`). school-simulator likewise
   (worktree `libcnalab/school-simulator-engine`, build `build/` = Release
   `OPENGLES3`, two jobs; nine commits; 19 captures identical
   after every step). Both branches were fast-forwarded into the games'
   `main` on 2026-10-21 and pushed at the owner's request, with this
   repository's `develop`. There is no `libcnalab/cna-engine` sibling, so
   builds in the games' main worktrees pass
   `-DCNA_ENGINE_ROOT=/rv/data/development/github.com/libcna/cna-engine`.
   PILOT-006: train-simulator runs on the engine on its branch
   `engine-adoption` (worktree `libcnalab/train-simulator-engine`, build
   `build/` = Release `OPENGL33` against the baseline clones, easy-gl
   `v0.1.1` and the CNA checkout's system SDL, three jobs; nine commits
   (the last fixes an avatar crash the game has with any CNA since
   2026-10-19); 54 `--lockstep` captures pixel-identical after every
   step, the last also on the GPU). It uses this repository's commits
   `52ccbf6` (byte captures) and `ece1943` (`RenderBucket::Sequenced`).
   The branch was fast-forwarded into the game's `develop` and pushed on
   2026-10-21 at the owner's request, after this repository's `develop`.
   The game's main worktree `libcnalab/train-simulator` has no `build/`
   yet and no `libcnalab/cna-engine` sibling: configure it with
   `-DTRAINSIM_CNA_ENGINE_ROOT=/rv/data/development/github.com/libcna/cna-engine`.
2. MILESTONE-2 is complete. Its performance budget (1.15 ×, best of five)
   is recorded but not enforced until it is measured on this machine in a
   Release build when the machine is idle (`docs/pilots/performance-budgets.md`).
3. CI: green on GitHub (engine job, cna-extended lane, canary; run
   39409759296). The known cna-extended failures are named in the lane
   (UP-005, UP-011) and its UP-004 skips are reported when they disappear.
4. ENG-035 is done: shadows (GAME-031, on baked light GAME-064) and
   post-processing (GAME-060) came with Phase 7; cascades wait for a game
   that needs them. Reflection probes go through the PBR binder's selector.
5. The pins are CNA `cb377fa04` and Sharp Runtime `1dc8785b` since
   2026-10-22 (UP-006 and UP-019 fixed upstream, UP-020 resolved by
   publishing Sharp Runtime, UP-001 by CNA's installed C++ SDK).
   cna-extended's pin is `c19c6e5` since 2026-10-22 (`86870ad` plus CI pins): its owner
   delegated the fixes (EXT-002's skips gone, UP-005/011/015 fixed, a CMake
   package from 0.2.0); its CI lane allows no failure or skip. Every CNA
   report to the Codex session `cna-demos` is closed; UP-018 was the
   engine's own error (fixed). A game builds from installed CNA, cna-extended
   and engine packages alone (consumer probes).
6. Upstream, for their owners: UP-001 (no CNA package), UP-005 (cna-extended
   skinned test), UP-006 (CNA umbrella vs module targets), UP-007 (unpushed
   pin), UP-008 (Street's mesh-memory accounting), UP-009 (car-simulator's
   load budget in Debug builds), UP-010 (Street test discards a nodiscard
   result), UP-011 (cna-extended fixture paths under ccache's base
   directory), UP-012 (Street's blended instance groups missing from its
   draw statistics), UP-013 (Street's asset pipeline: stale checksums,
   Blender 4.3 tree export, MPFB first run), UP-014 (galaxy-eggbert's
   editor draws wireframe on OpenGL ES contexts that cannot), UP-015
   (cna-extended's `SizeF` class/struct mismatch under Clang); UP-004 is
   resolved upstream.

## Pointers

- `README.md`, `docs/guide-strict.md`, `docs/guide-enhanced.md`
- `plan.md` — tasks with `[x]` / `[~]` / `[ ]` and results
- `docs/adr/`, `docs/conventions.md`, `docs/api-status.md`,
  `docs/verification.md`, `docs/verification-matrix.md`, `docs/pilots/`
- `upstream-issues.md`, `NOTICE.md`, `CHANGELOG.md`
- `analysis.md`, `gap-audit.md` — the original study and audit
