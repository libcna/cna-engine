# Verification record

Newest first. Each entry states the dependency revisions and worktree state,
the configuration, the commands, and the result. Failures and skips are
listed, never folded into a pass.

## 2026-10-22 — Baseline move: CNA `cb377fa04` (UP-001), cna-extended `c19c6e5`

CNA published `cb377fa04`, which installs a relocatable C++ module SDK
(`58ebc46fd`, UP-001); Sharp Runtime stays at `1dc8785b` (its package);
cna-extended `c19c6e5` (CI pins only after `86870ad`). Clean clones as below.

- `build/`, baseline enforced (all three match): **339 of 339**.
- cna-extended's suite against CNA `cb377fa04`: **2372 of 2372**, nothing
  skipped.
- `tools/run_consumer_probes.sh`: **3 of 3**, now from installed packages
  alone. The script builds CNA's SDK (`CNA_INSTALL_CXX=ON`, static, SOFTWARE,
  HEADLESS, NULL audio, target `cna_cpp_sdk`) and installs its `SharpRuntime`
  and `CNACpp` components, installs cna-extended's package and the engine's;
  the consumer adds no source tree, and the engine's configuration finds CNA
  (runtime 0.1.0) and cna-extended 0.2.0. The strict and enhanced consumers
  and a game made with the installed `cna_engine_add_game` build and run.
- `build-probe/`, baseline enforced, pilot suites: **50 of 50** (car-simulator's
  suite, street-simulator's 13 programs, the equivalence probes).

## 2026-10-22 — Baseline move: CNA `08c1387ec`, Sharp Runtime `1dc8785b` (ADR-002)

CNA's published `next` `08c1387ec` carries the fixes for UP-006 and UP-019;
Sharp Runtime's published `next` `1dc8785b` carries the Uri members CNA needs
since `abbd8a4ea` (UP-020). Clean local clones `~/deps/cna-baseline` and
`~/deps/sharp-runtime-baseline` at those revisions; cna-extended `86870ad`.

- `build/` (Debug, SOFTWARE/HEADLESS/NULL, SDL and video off) configured with
  `CNA_ENGINE_ENFORCE_BASELINE=ON` (all three dependencies match):
  **339 of 339** (`ctest -E carsim_suite`), with
  `ShadowReceptionTest.CalibrationFindsTheSoftwareRenderersOrientation` now
  expecting the software renderer the right way up (UP-019 fixed).
- `tools/run_consumer_probes.sh`: **3 of 3**, cna-extended from its installed
  package 0.2.0.
- cna-extended's suite (its `build/`, Debug, against the same clones):
  **2372 of 2372**, nothing skipped.
- `build-probe/` reconfigured natively (it had held an Emscripten build) with
  the baseline enforced, `CNA_ENGINE_CNA_CNAEXT=ON` and
  `CNA_ENGINE_PILOT_SUITES=ON`: the 50 `pilot` tests pass, among them
  car-simulator's own suite (`main` `3a34c80`, UP-009's load-budget test
  excluded by name in Debug as before) and street-simulator's 13 test
  programs (`develop` `14145cd`).

## 2026-10-22 — cna-extended fixes (EXT-002, UP-005/011/015), its package (REL-001), CNA reports

The owner of cna-extended delegated its fixes; CNA defects went to the Codex
session `cna-demos`. CNA `~/deps/cna-baseline` `900d0db3b`, Sharp Runtime
`~/deps/sharp-runtime-baseline` `f2aecfef`; cna-extended branch
`engine-upstream` (worktree `libcna/cna-extended-engine`, fast-forwarded into
the local `develop` at `86870ad`, not pushed).

- cna-extended `build/` (Debug, SOFTWARE/HEADLESS/NULL, SDL and video off,
  through ccache with the shared base directory): before, 2371 tests with 5
  failures (the four bitmap-font fixture tests, UP-011; the skinned animation
  test, UP-005) and 2 skips (EXT-002); after, **2372 of 2372 pass, none
  skipped** (the new one keeps `Version.hpp` equal to the project version).
  `cna_extended_minimal`, `cna_extended_tiled_demo` and
  `cna_extended_world3d_demo` (which failed with "BlendIndices0 is missing"
  before `59f179f`) run headlessly. Clang `-Wmismatched-tags` no longer warns
  on `SizeF` (it did before `a6b0c56`).
- `tools/check_package.sh` (cna-extended): installed the package into
  `build-consumer/install` and built and ran a consumer
  (`find_package(CnaExtended 0.2)`, an `OrthographicCamera` on a software
  device): ok. Without CNA's targets the package reports which are missing;
  `find_package(CnaExtended 0.3)` does not match 0.2.0.
- Engine `build/` against the local cna-extended `develop`: **339 of 339**
  (`ctest -E carsim_suite`). `tools/run_consumer_probes.sh`: **3 of 3**, with
  `CNA::Extended` taken from the installed cna-extended package 0.2.0.
- CNA (`codex queue` to `cna-demos`): UP-002 documented (CNA `99b54079e`);
  UP-018 judged not reproducible there and a regression test added
  (`a6e731cc2`), but the engine's shadow-map test at `900d0db3b` still reads
  `-nan` from a `Single` target on SOFTWARE (precision probe: 8 bits), so a
  reproducer from the engine's exact sequence (Clear, then a fogged
  BasicEffect draw into a Single target with Depth24) was sent; UP-019
  reproduced there; UP-006 may be fixed by CNA `470b12a5d` (not in the pin);
  UP-001 accepted as a gap. Nothing is pushed in CNA yet.
- UP-018 correction (later the same day): the Codex session showed the
  `Single` target on CNA's default (Reach) device is a Color target, as in
  XNA, and the engine kept the requested format; `ShadowMap` now takes the
  target's own format. `ShadowMapTest`, `ShadowReceptionTest`, the sky,
  EngineGame and example tests: 19 of 19.
- CNA pin trial (evening): CNA published `next` at `08c1387ec` with UP-006's
  and UP-019's fixes, but it does not compile against the published Sharp
  Runtime `f2aecfef` (UP-020: it needs the unpublished `c26614e8`). Against
  CNA `08c1387ec` and Sharp Runtime `c26614e8` (clean local clones, baseline
  enforced for those revisions) the engine builds and passes 338 of 339; the
  one failure is `ShadowReceptionTest.CalibrationFindsTheSoftwareRenderersOrientation`,
  as expected once UP-019 is fixed (the software renderer is no longer upside
  down). The pin stays at `900d0db3b`/`f2aecfef` and the clones went back.
- Pin move (ADR-002): cna-extended `develop` pushed (`0061e3e..86870ad`);
  `cmake/CnaEngineBaseline.cmake`, CI and the ADR table name `86870ad`; the
  engine's `build/` configured with `CNA_ENGINE_ENFORCE_BASELINE=ON` reports
  CNA, Sharp Runtime and cna-extended matching their pins; the 339 tests and
  3 consumer probes above ran against exactly these revisions.

## 2026-10-22 — GAME-064/065: shadows on baked light, shadows and menus in backrooms and school

Engine `develop` `2d893cb` (`Material::bakedKeyLight`) and `fd5a478` (menu
pointer); CNA `~/deps/cna-baseline` `900d0db3b`, Sharp Runtime
`~/deps/sharp-runtime-baseline` `f2aecfef`, easy-gl `v0.1.1`.

- `build/` (as in the Phase 7 entry): **338 of 339 tests pass** after
  `2d893cb`; the one failure was `cna_engine.api_snapshot`, expected for the
  new `Material` field, and passes after the snapshot update in the same
  commit. New: `ForwardRendererTest.BakedLightKeepsAllButTheKeyLightsShare`,
  `ShadowReceptionTest.BakedLightLosesItsKeyShareInTheShadow`; the menu
  test covers a resting pointer (`cna_engine_ui_tests`, 10 of 10).
- backrooms-simulator, branch `engine-shadows-menu` (worktree
  `libcnalab/backrooms-simulator-engine`, `build/` OpenGL ES 3 with the CNA
  checkout's Linux SDL prebuilt, Release): 30 deterministic captures
  (`--capture`) on the Radeon 780M (`SDL_VIDEODRIVER=offscreen`) after each
  step. Build step identical; shadows: the 24 views without creatures
  identical, five creature views differ only under the creatures (at most
  one level elsewhere), the fading one identical; menu step identical.
  llvmpipe shows the same shadow. `tools/validate_controls.py` (adapted to
  the menu) passes on a private Xvfb; a scripted X11 run rebound a key,
  clicked Resume and saved `settings.ini`/`bindings.ini`. Browser build
  (`build-consumer/`, Emscripten, the CNA checkout's emscripten SDL
  read-only): in headless Chrome (SwiftShader) the creature's shadow falls
  under it, Escape opens the menu (no Quit in a page), Down and Enter the
  settings, Escape returns and resumes; key presses shorter than one of
  SwiftShader's slow frames are missed (CNA polls the keyboard).
- school-simulator, branch `engine-shadows-menu` (worktree
  `libcnalab/school-simulator-engine`, `build/`, -j2): 19 reference
  captures plus six outdoor views (fixed step 0.05) on the Radeon 780M.
  Build step identical; sun shadows: outdoor views changed (shadows, and
  surfaces turned from the sun darker), indoor views only through windows;
  a repeated run is byte-identical; `--no-shadows` is pixel-identical to the
  reference (two views); llvmpipe needed an extra bias of 0.001 against
  stripes on the self-shadowing ground. Menu step identical; real X11 input
  (private Xvfb) opened the menu, the settings, went back, resumed without
  turning the view (fixed in the game: `59ed114`), quit, clicked Resume and
  rebound Use. `school_app_tests`: 3,705,866 checks, 0 failures. Browser
  build (`build-consumer/`, configured by hand as before, `JOBS=2
  tools/build_web.sh`): in headless Chrome the 2048 sun map is created
  (float, 23 bits), Escape opens the menu without Quit and resumes.

## 2026-10-22 — Phase 7: a new game from CNA and the engine alone

Engine `develop` from `8f2aa7f` (plan) to `76a9b9a`; CNA `~/deps/cna-baseline`
`900d0db3b`, Sharp Runtime `~/deps/sharp-runtime-baseline` `f2aecfef` (the
pins, clean), cna-extended `../cna-extended` `0061e3e` (its pin, clean),
easy-gl `v0.1.1`, meta-gl `v0.4.1`.

- `build/` (GCC, Debug, software/headless/null, SDL and video off, against
  the pinned clones): **337 of 337 tests pass** (`ctest -E carsim_suite`),
  among them the new App, Collision, Animation, Sky, ShadowMap, LocalLights,
  PostProcess, Audio (null audio platform), UI, SaveSlots, ModelImport
  (CNA's run-time glTF import; palettes equal to CNA's `AnimationPlayer`
  within 2e-4), ShadowReception, LocalLight and ParticleBillboards tests,
  `cna_engine.new_game`, `cna_engine.fetch_dependencies_dry_run`, and eight
  examples (`hello_engine`, `template_game`, `first_person`, `third_person`,
  `day_night` new). The strict boundary check covers App, Collision,
  Animation, Audio and UI; the API snapshot moved for three additive
  accessors (CHANGELOG).
- `tools/run_consumer_probes.sh` (`build-consumer/`): the installed package
  with the new components App, Audio and UI; **3 of 3** — the strict and
  enhanced consumers and a game built with the installed
  `cna_engine_add_game` (the hello example, run with a screenshot).
- A new user's path: `tools/new_game.sh build-consumer/new_game "Crate Walk"`,
  configured on its own through `cmake/CnaEngineGame.cmake` (CNA's default
  OpenGL ES 3, SDL3 from the CNA checkout's Linux prebuilt, Release): built,
  and run on the GPU (`SDL_VIDEODRIVER=offscreen`, Mesa 25.0.7 radeonsi):
  sky, the crates' and the platform's shadows away from the sun, the
  crosshair and hint. Its `tools/build_web.sh` (Emscripten 6.0.9, the CNA
  checkout's `.sdl-prebuilt-emscripten` read-only): in headless Chrome
  (SwiftShader, a private Xvfb) W walks up the stairs, Escape opens the pause
  menu, Down and Enter its settings (volume, the Jump and Run keys); WebGL 2
  shadows fall the right way; no error in the console.
- The engine's `hello_engine` was also built under Emscripten in
  `build-probe/` and ran in headless Chrome (the preloaded texture, the
  page's hint).

Found on the way (`upstream-issues.md`): UP-018 (the software renderer keeps
a `Single` render target as 8-bit colour) and UP-019 (shadow receivers read a
render-target shadow map the other way up on the software renderer than on
EasyGL; the engine calibrates per device). Not checked: other GPUs and
browsers than the above, audible sound output, Windows and macOS.

## 2026-10-21 — easy-3d oracle made self-contained (PILOT-004 closure)

The test-only portions of easy-3d used by `Easy3DEquivalenceTests` were frozen
under `tests/fixtures/easy3d_legacy` from easy-3d `ad63bf9`, with its MIT
license and provenance. `tests/pilots/CMakeLists.txt` no longer names or checks
for an easy-3d checkout: with `CNA_ENGINE_PILOT_PROBES=ON`, the five tests are
unconditional and cannot silently disappear when the sibling repository is
absent.

- `build/` (GCC, Debug, software/headless/null, SDL and video disabled): the
  oracle and equivalence executable rebuilt successfully with `-Werror`;
  **5 of 5 Easy3DEquivalenceTests passed**.
- The build used moving development worktrees (CNA 54 commits and Sharp
  Runtime 12 commits after their recorded baselines), so this is closure
  evidence for the self-contained oracle, not new baseline evidence.
- Source/configuration search found no remaining `CNA_ENGINE_EASY3D_DIR`,
  easy-3d source path, include or link dependency. PILOT-004 is complete.

## 2026-10-20, 21:40 — easy-3d taken over; galaxy-eggbert on the engine (PILOT-004)

Engine `develop` at `e56fa0f` (easy-3d facilities) and `d86c62d` (build
fixes); CNA worktree `../cna` at `16b38aac`, then `c875d324` (moved by other
agents; `build/` is not baseline evidence).

- `build/` (GCC, Debug): **213 of 213 tests passed** at both commits,
  including `Easy3DEquivalenceTests` (5 tests: atlas regions and grids,
  200 random faced and plain cubes, 200 plates and triple crosses, 200
  billboards, 200 camera matrix pairs, orbit positions and 300 follow steps
  — all bit-identical to easy-3d's sources compiled read-only).
- Clang: all 74 engine translation units (`build/compile_commands.json`,
  compiler swapped, `-fsyntax-only`) compile without a warning with
  `-Werror` after `d86c62d`; before it, `RenderQueue`'s unused member and
  cna-extended's `SizeF` (UP-015) failed. Emscripten `em++` 6.0.9 compiles
  the strict modules against CNA's `WEBGL2` headers.
- galaxy-eggbert (`engine-adoption`, worktree
  `openeggbert/galaxy-eggbert-engine`, Release, `OPENGLES3`, CNA
  `~/deps/cna-baseline` `900d0db3b` and Sharp Runtime `f2aecfef`, both
  confirmed by the engine's baseline check, easy-gl v0.1.1, meta-gl v0.4.1):
  before the first step and after each of the four code steps 93/93 CTest
  cases pass (`-E easy-gl-resource-smoke-tests`), and the golden frames
  60/120/180, the behavioral trace and both startup screenshots are
  byte-identical to the pre-migration build under Xvfb (llvmpipe) and with
  `SDL_VIDEODRIVER=offscreen` (Radeon 780M) — 12 of 12 files every time.
  `OPENGL33` build (`build-probe/` there): menus and editor driven under
  Xvfb with xdotool. Not run: the Emscripten build (only its changed sources
  were compiled with `em++`) and Android.

## 2026-10-20, 17:50 — pin moved to CNA `900d0db3b` (UP-004's fix published)

CNA published `next` at `900d0db3b52d76d3115ebda0a0e04a25fefad1a5`
(142 commits after `688f3f23`; contains `5ab1c7729` and a regression test for
the orthographic case). `~/deps/cna-baseline` checked out there (clean), Sharp
Runtime unchanged at `f2aecfef` (its published `next`), cna-extended at
`0061e3e1`.

- Baseline tree `build-probe/` (baseline enforced, both pilots' own suites,
  scene comparisons): build 4 min 51 s, **213 of 213 tests passed** (15 min).
- cna-extended suite (`build-consumer/cna-extended/`, CI's lane
  configuration): 2,371 tests — 2,364 passed under CTest, 2 skipped (UP-004's
  tests, skipped unconditionally in cna-extended's sources), 1 known failure
  (UP-005), 4 fixture-path failures that pass from the build root (UP-011):
  the same 2,368 as at the previous pin.

## 2026-10-20, 12:55 — CI green on GitHub

After `develop` and the pilots' branches were pushed (owner's request), the
workflow ran on GitHub for the first time. Four workflow defects surfaced and
were fixed: Sharp Runtime was checked out without its GoogleTest submodule;
the "no test may be skipped" step matched test names containing "Disabled"
instead of CTest's markers; the consumer probes passed a relative
`CMAKE_PREFIX_PATH` (and so found the installed package only from a warm
cache) and exported this machine's `/rv/cnaccache`; and the lane needed
UP-011's four tests as named known failures. Run 39409759296
(`workflow_dispatch`, `develop` at `711557a`): engine build and tests
(boundary 5, unit 164, examples 3, performance 3, all 175, consumer probes
2) green, cna-extended lane green, canary against the upstream heads green.

## 2026-10-20, 10:22 — sanitizers and full baseline after the adoption APIs

With `Camera::SetMatrices` and `QualitySchema::PresetValues`:
`build-asan/` (`CNA_ENGINE_SANITIZE=address,undefined`) **175 of 175 tests
passed**; the pinned baseline tree `build-probe/` (CNA `688f3f23`, Sharp
Runtime `f2aecfef`, baseline enforced, both pilots' own suites) **213 of 213
passed** in 15 min with 3 jobs; `build/` 198 of 198.

## 2026-10-20, 09:10 — street-simulator adopts the engine (branch `engine-adoption`)

Engine `build/` with `QualitySchema::PresetValues` (`7947b6e`): **198 of 198
tests passed**.

street-simulator worktree `libcnalab/street-simulator-engine`, branch
`engine-adoption` from `86b4a2e`, build `build/` (Release, software renderer,
headless, null audio, shared CNA and Sharp Runtime worktrees, cna-engine
from this repository; first build 9 min 34 s with 3 jobs): after each of the
five commits Street's own CTest suite passed (**15 of 15**). Captures
(`--capture`, 640 × 360, all 18 named viewpoints, plus `--night`) of the
renderer step (`b137025`) against the commit before it: all 19 inside the
agreed tolerance — mean difference 0.000–0.066/255, visibly different pixels
2–1,081 of 230,400 (at most 0.47 %), around glass. Benchmarks (320 × 180,
no probes, best of two alternating runs, load ~22): identical draw calls
(1,571 baseline, 1,294 crowd), best CPU frame 2,068 → 1,924 ms and 1,547 →
1,486 ms. Quality-preset step (`fe36c1d`): settings dumps after every preset
and after four preset/flag combinations byte-identical to the previous
commit. Asset step (`513dd93`): capture pixel-identical, start-up log
identical.

OpenGL 3.3 build of the same worktree (`build-probe/`, Release, SDL3 platform
under `xvfb-run`, Mesa 25.0.7 llvmpipe, OpenGL 4.5 core; 5 min 7 s with 3
jobs): the 18 viewpoints and the night view, renderer files of `64d1c07`
against `b137025`, all 19 inside the tolerance — mean 0.000–0.138/255,
visibly different pixels 0–1,091 of 230,400. The atmospheric sky (custom
effects, absent on the software renderer) draws through the engine's
`BeforeOpaque` pass. On the hardware GPU (AMD Radeon 780M, radeonsi,
`SDL_VIDEODRIVER=offscreen`): all 19 inside the tolerance, mean
0.000–0.133/255, visibly different pixels 1–1,114 of 230,400.

## 2026-10-20, 08:20 — cna-extended suite against CNA `688f3f23` (ADR-002)

cna-extended `0061e3e1` (clean) configured as CI's lane does (Debug, tests and
examples, SDL and video off) in `build-consumer/cna-extended/`, against the
clean pinned clones `~/deps/cna-baseline` (`688f3f23`) and
`~/deps/sharp-runtime-baseline` (`f2aecfef`); build 7 min 52 s with 3 jobs.
2,371 tests: **2,364 passed under CTest, 5 failed, 2 skipped.**

- Skipped (known): the two `OrthographicCameraTest.Contains*` cases — UP-004,
  fixed upstream only after this pin.
- Failed (known): `AnimationSystem3DEXTTest.DifferentAnimationTimes_ProduceDifferentPixelOutput`
  — UP-005.
- Failed (environment): the four `BitmapFontFileReaderTests` — their fixture
  path comes from `__FILE__`, which ccache's base directory makes relative;
  run from the build root they pass (UP-011).

So the result equals the previous pin's: 2,368 passing, 2 known skips, 1
known test defect.

## 2026-10-20, 07:50 — car-simulator adopts the engine (branch `engine-adoption`)

Engine `build/` with `Camera::SetMatrices` (`98c8c28`): **197 of 197 tests
passed** (three new camera tests, one new camera-sync test).

car-simulator worktree `libcnalab/car-simulator-engine`, branch
`engine-adoption` from `cd6d948`, build `build/software` (Release, software
renderer, headless, no audio, CNA and Sharp Runtime from the shared
worktrees, cna-engine from this repository): after each of the eight
adoption commits its own CTest suite passed (**7 of 7**) and
`scripts/check_xna_only.py` reported OK (235 files). Deterministic captures
(`--frames 3 --lockstep --time-scale 0 --no-save --no-audio`) of the world
renderer step (`dc83bb4`) against the commit before it, under the agreed
tolerance:

| Scenario | Mean | Largest | Visible (> 3/255) |
| --- | --- | --- | --- |
| day, 10:30, clear | 0.055/255 | 2 | 0 |
| night, 22:00, headlights | 0.003/255 | 1 | 0 |
| rain | 0.027/255 | 1 | 0 |
| snow | 0.066/255 | 2 | 0 |
| fog | 0.031/255 | 1 | 0 |
| cockpit with mirrors | 0.039/255 | 2 | 0 |
| low tier | 0.055/255 | 2 | 0 |

The plate-cache and quality-tier steps were pixel-identical. On OpenGL ES 3
(`build-probe/` in the worktree, Release, SDL3 under Xvfb, Mesa 25.0.7
llvmpipe, OpenGL ES 3.2) the same seven captures, world-renderer files of
`6a927fb` against `dc83bb4`: mean 0.002–0.064/255, largest 16/255, at most
one pixel per image visibly different. The same on the hardware GPU (AMD
Radeon 780M, radeonsi; `SDL_VIDEODRIVER=offscreen`, which EGL resolves to
the GPU): mean 0.002–0.065/255, largest 12/255, at most one pixel visibly
different. World pass CPU
time (best of three alternating Release runs, 320 × 180, load ~8): 1,313 ms
before, 1,358 ms after (+3.4 %).

## 2026-10-20, 06:38 — pin moved to the published CNA `688f3f23`

`~/deps/cna-baseline` checked out at `688f3f23` (clean; `origin/next` on
GitHub), `cmake/CnaEngineBaseline.cmake` and CI updated. `build-probe/`
(baseline enforced and matching all three pins) rebuilt in 3 min 49 s, then
everything:

Result: **208 of 208 passed** in 799 s — engine tests, examples, pilot
probes, both scene comparisons within the agreed screenshot tolerance, both
pilots' own suites.

## 2026-10-20, 05:11 — full baseline run after reflective materials

`build-probe/`, everything: **208 of 208 passed** in 1,249 s (the machine
was loaded; the earlier full run took 770 s).

## 2026-10-20, 04:46 — reflective materials

`build/` **193 of 193** (new: environment-mapped materials pixel-identical
to a hand-configured `EnvironmentMapEffect` with car-simulator's paint
values); sanitizers **170 of 170**; consumer probes 2/2.

## 2026-10-20, 04:42 — full baseline run

`build-probe/` (clean pinned CNA and Sharp Runtime clones, baseline enforced
and matching, CNAEXT on, pilot suites on), everything:

    ctest --test-dir build-probe --parallel 3

Result: **207 of 207 passed** in 770 s — engine unit, boundary and
performance tests, examples, every pilot probe, both scene comparisons (Street
three views; car-simulator day, night, headlights and mirror, all within the
proposed screenshot tolerance) and both pilots' own suites.

## 2026-10-20, 04:23 — mirrored cameras

car-simulator comparison: rear-view mirror (mirrored, 320 m cap) 145/145 draw
calls, every batch count equal to car-simulator's own; 3 visibly different
pixels. `build/` **192 of 192**; consumer probes 2/2.

## 2026-10-20, 04:15 — car-simulator headlights, alpha-tested blending

car-simulator comparison (`build-probe/`): day 867/867, night 674/674,
headlights 798/798 draw calls (124 headlight-fill draws through nested
renderers), all within the proposed tolerance. `build/` **191 of 191**;
sanitizers **168 of 168**.

## 2026-10-20, 03:49 — tolerances enforced, clean Sharp Runtime clone

The shared Sharp Runtime worktree gained uncommitted edits (another agent),
which the enforced baseline check rejected; `build-probe/` now uses a clean
clone at the pin, `~/deps/sharp-runtime-baseline` (rebuild 13 min with 3
jobs). Results:

- `build-probe/`: **204 of 204** with `-E carsim_suite`, and the
  car-simulator suite on its own with UP-009's test excluded by name: passed
  in 869 s — **205 of 205** in all.
- `build/`: **190 of 190**.
- Both scene comparisons now enforce the proposed screenshot tolerance
  (`docs/pilots/screenshot-tolerances.md`); every view is within it.

## 2026-10-20, 03:30 — static analysis

`clang-tidy` 19 over the engine's 30 source files (and their headers) with
`bugprone-*`, `performance-*` and `clang-analyzer-*` (minus swappable
parameters, narrowing, enum size and implicit widening): 10 findings, none a
defect — an intentional empty catch in a destructor (now marked), optional
values assigned through a dereference after validation (now assigned
directly), identical switch branches (merged), and three string
concatenations in error messages (left). Non-pilot tests 167/167 after the
cleanup.

## 2026-10-20, 03:23 — the pilots' own suites against the pinned CNA (ADR-002)

`build-probe/` (pinned, baseline enforced and matching) reconfigured with
`-DCNA_ENGINE_PILOT_SUITES=ON`; both pilots' test sources compiled read-only
(1 min 16 s with 3 jobs).

    ctest --test-dir build-probe --parallel 3 -R "carsim_suite|street_suite"

- street-simulator: **13 of 13** test programs pass.
- car-simulator: **320 of 321** tests pass in 18 min; the failure is
  `SampleMap.LipovaLoadsAndIsFullyConnected`'s 6 s production load budget
  (16.0 s in this Debug build on a loaded machine), excluded by name since
  (UP-009).

## 2026-10-20, 03:00 — Street with traffic

Street comparison with traffic and pedestrians (`build-probe/`, pinned):
first view 1,110 / 192 / 123 visible, 1,284 / 1,284 draw calls, 68 pixels
differ; walk 954 / 157 / 78, 1,087 draws, 884 pixels; close-up 787 / 104 /
42, 869 draws, 781 pixels, none on the 4,112 figure pixels. 176 s, 2.7 GB.

## 2026-10-20, 02:52 — car-simulator at night, material fog

Sanitizer tree `build-asan/` rebuilt with every change of 2026-10-20/21:
**165 of 165** non-pilot tests pass with ASan, UBSan (fatal) and LSan.
`build/` 188/188, baseline `build-probe/` 189/189.

car-simulator comparison (`build-probe/`, pinned): day 867/867 draw calls
(0.05/255), night at 22:00 674/674 draw calls including 15 additive lamp
lights (800 pixels differ, at most 20/255).

## 2026-10-20, 02:41 — car-simulator signs, baseline 188/188

Baseline `build-probe/`: **188 of 188** (before the sign change). The
car-simulator comparison now takes over sign batches as well: 867 of 867 draw
calls, mean image difference 0.05/255.

## 2026-10-20, 02:34 — GPU memory estimate, walk view

`build/` **184 of 184**. Street comparison (`build-probe/`, pinned): three
views now — first viewpoint (0.01/255), a walk driven by engine input
(621/157/72 visible, 748 draws, 0.20/255), pedestrian close-up (0.39/255) —
plus GPU resource estimates (Street's mesh accounting is 8 bytes per vertex
high, UP-008).

## 2026-10-20, 02:20 — display names

`build/` (shared CNA worktree): **182 of 182**. Baseline `build-probe/`
(pinned, enforced, CNAEXT on): **183 of 183**, both scene comparisons
included.

## 2026-10-20, 02:10 — sanitizers

`build-asan/` configured with `-DCNA_ENGINE_SANITIZE=address,undefined
-DCNA_ENGINE_PILOT_PROBES=OFF` (shared CNA worktree at `d77ee1df`, not
baseline evidence; only the engine's libraries, tests and examples are
instrumented). Build 5 min 42 s with 3 jobs.

    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        ctest --test-dir build-asan --parallel 3

Result: **158 of 158 passed** (every non-pilot test, examples included), with
UBSan findings fatal and LeakSanitizer on; also 158/158 with leak detection
off.

## 2026-10-20, 02:02 — reflection-probe selector, quick-start game

`build/` **180 of 180**, baseline `build-probe/` **181 of 181** (new:
`cna_engine.example.quick_start`, the selector test). Street comparison in
probes mode, run once by hand (4 min 53 s, 2.7 GB peak): first view 75 of
129,600 pixels differ, close-up 785 (2 on figures); visibility and draw
calls identical (`docs/pilots/street-simulator.md`).

## 2026-10-20, 01:45 — skinned render items

Baseline `build-probe/` (clean pinned dependencies, baseline enforced and
matching; CNAEXT on), Debug, software renderer: **179 of 179 tests passed**.
Day-to-day `build/` (shared CNA worktree, CNAEXT off): **178 of 178 passed**.

Street scene comparison now runs with Street's pedestrians: first viewpoint
652/652 items, 192/192 instances, 117/117 figures, 820/820 draw calls, mean
image difference 0.01/255 (53 pixels); close-up of a pedestrian: 57 figures
covering 10,412 pixels, 2 of them differ, 772 pixels in the frame
(coplanar ground meshes under a 1.7e-6 view-matrix difference).

## 2026-10-20, 01:05 — blend states, samplers, Street lighting, diagnostics

Baseline `build-probe/` (clean pinned dependencies, baseline enforced and
matching: CNA `5a44d2c5`, Sharp Runtime `f2aecfef`, cna-extended `0061e3e1`;
`CNA_ENGINE_CNA_CNAEXT=ON`), Debug, software renderer:

    cmake --build build-probe --parallel 3
    ctest --test-dir build-probe --parallel 3

Result: **175 of 175 tests passed**. Scene comparisons in that run:

- Street: visible items 652/652, instances 192/192, draw calls 703/703, mean
  image difference 0.01/255 (53 of 129,600 pixels differ; 49 explained by
  Street's depth writes in its transparent pass).
- car-simulator: terrain 207, roads 77, objects 503, trees 33 — equal under
  both frustum tests; mean image difference 0.1/255.

Day-to-day `build/` (shared CNA worktree at `b0940e0e` with uncommitted math
edits by another agent; CNAEXT off): **174 of 174 passed** (all but the Street
comparison, which needs CNAEXT). In that worktree CNA's
`BoundingFrustum::Intersects(sphere)` is a GJK query (XNA's algorithm), so
car-simulator draws 506 object and 34 tree batches where the engine's plane
test keeps 503 and 33, with the same image; the comparison checks the
engine's rules under car-simulator's frustum test (506/34, equal).

## 2026-10-20, 00:21 — car-simulator clock probe

Day-to-day `build/` (shared CNA worktree at `b0940e0e`, which other agents
edit — not baseline evidence; CNAEXT off), Debug, software renderer.

    cmake --build build --parallel 3
    ctest --test-dir build --parallel 3

Result: **167 of 167 tests passed**, including the three new
`cna_engine.pilot.CarSimulatorClockProbe.*` tests (car-simulator's
`Vehicle::Update` step loop against `FixedStepClock`; see
`docs/pilots/car-simulator.md`).

## 2026-10-19 (night) — CNAEXT configuration and the Street scene comparison

`build-probe/` reconfigured with `-DCNA_ENGINE_CNA_CNAEXT=ON` (same clean
pinned dependencies, baseline still enforced and matching) so that Street's
whole core, which needs CNA's CNAEXT shader packages, compiles read-only.

- Engine suite in that configuration: **158 of 158 tests passed** (including
  `cna_engine.pilot.street_scene_comparison`), showing the engine also builds
  and runs against CNA with its extension layer enabled.
- Street scene comparison (`docs/pilots/street-simulator.md`): identical
  visible items (652), visible instances (192) and draw calls (703); engine CPU
  frame time lower in every run; mean image difference 19/255 from Street's
  lighting model.
- Found and recorded UP-006: Street's `CNA` umbrella (libcna.so) plus the
  engine's static CNA modules duplicated CNA's statics and crashed at exit;
  the probe links Street against the static modules.

Day-to-day `build/` (shared CNA worktree, CNAEXT off): 157/157 at that
point; later the same night the car-simulator scene comparison was added
there (strict, no CNAEXT needed): terrain 207/207, roads 77/77, objects
503/503, trees 33/33, mean image difference 0.1/255; suite 164/164.

## 2026-10-19 (baseline) — clean pinned dependencies, enforced

The CNA worktree used by the earlier runs carries other agents' uncommitted
edits (by now `modules/math/src/Vector{2,3,4}.cpp`), so it cannot give
baseline evidence. A clean checkout of the pinned CNA revision was created as
a shared dependency (`~/deps/cna-baseline`, a local clone; the CNA repository
itself is untouched).

| Repository | Revision | Worktree |
| --- | --- | --- |
| CNA (`~/deps/cna-baseline`) | `5a44d2c507cd5e49eab9ab35570a8a98c4b7a0f3` | clean |
| Sharp Runtime | `f2aecfefeac171b331787e5cdc4849507363ef0e` | clean |
| cna-extended | `0061e3e17c30b2a3a302bce617e64765883079aa` | clean |

Configuration (`build-probe/`): as CI — Debug, Ninja, ccache,
`CNA_ENGINE_ENFORCE_BASELINE=ON` (all three revisions verified at configure
time), `CNA_ENABLE_SDL=OFF`, `CNA_ENABLE_VIDEO=OFF`, software/headless/null,
pilot probes on.

Result: build 8 min 13 s with 3 jobs; **150 of 150 tests passed** (unit,
boundary, examples, pilot probes). This is the engine's baseline evidence for
ADR-002.

## 2026-10-19 (later) — new CNA pin, pilots, cna-extended rerun

Dependencies: CNA `next` worktree at `5a44d2c507cd5e49eab9ab35570a8a98c4b7a0f3`
(new pin; at the time tracked modifications only in `CNA_DEMOS_REPAIR_*`
documents and `modules/c-api/tests/pure_c/GeometrySmoke.c`, none built here;
the worktree later moved on, see the baseline entry above), Sharp Runtime
`f2aecfef` (clean), cna-extended `0061e3e1` (clean). Pilots read-only:
car-simulator `cd6d948e`, street-simulator `86b4a2e7`.

Engine (`build/`, same configuration as below, plus `CCACHE_NOHASHDIR=1`):
**133 of 133 tests passed** — unit, boundary, example, and pilot probes
(car-simulator: controls, cameras, meshes, screenshots, checker allowance;
street-simulator: camera, controls, tangent-frame meshes).

Installed-package consumer probes (`tools/run_consumer_probes.sh`,
`build-consumer/`): 2 of 2 passed; the strict consumer passes the boundary
checker and its link closure contains no cna-extended target.

cna-extended suite against the new pin (`build-probe/`, reconfigured from the
2026-10-14 audit tree; rebuild 3 min 42 s with 3 jobs):
2,371 tests — **2,368 passed, 1 failed, 2 skipped**.

- Skipped (known): the two `OrthographicCameraTest.Contains*` cases — CNA
  defect UP-004.
- Failed (new since the audit): `AnimationSystem3DEXTTest.DifferentAnimationTimes_ProduceDifferentPixelOutput`
  — a cna-extended test builds a skinned mesh from an undeclared vertex
  buffer, which CNA rejects since `e7bd98e46` (2026-10-15). Test defect,
  UP-005. Not fixed here (cna-extended is not modified from this repository).

Street and car-simulator full suites were not rerun (their own build trees
would be needed); the pilot probes above cover the facilities the engine
takes over.

## 2026-10-19 — first engine build (ENG-010 … ENG-025 initial slice)

Dependencies (configure-time baseline check):

| Repository | Revision | Worktree |
| --- | --- | --- |
| CNA `next` | `705ea232439135654ce93279dfe87c1ab047ef5c` | 3 modified tracked files from concurrent work (`CNA_DEMOS_REPAIR_HANDOFF.md`, `CNA_DEMOS_REPAIR_MATRIX.json`, `modules/c-api/CMakeLists.txt`); none is compiled here (C API off). Configure warned: not baseline evidence. |
| Sharp Runtime `next` | `f2aecfefeac171b331787e5cdc4849507363ef0e` | clean |
| cna-extended `develop` | `0061e3e17c30b2a3a302bce617e64765883079aa` | clean |

Configuration: `build/`, Ninja, Debug, GCC 14.2, ccache (`/rv/cnaccache`,
`CCACHE_BASEDIR=/rv`), CNA renderer `SOFTWARE`, platform `HEADLESS`, audio
`NULL`, `CNA_ENGINE_WITH_EXTENDED=ON`.

Commands:

```sh
cmake --build build --parallel 3
ctest --test-dir build --parallel 3 --timeout 300
```

Result: **88 of 88 tests passed** — 85 unit tests (82 strict, 3 enhanced),
2 boundary tests (`strict_boundary` passes; `strict_boundary_detects_leaks`
fails as required), 1 example (`orbit_scene` renders 30 frames, 3 draws per
frame, writes a PNG and checks the centre pixel). The strict header
self-containment target (`cna_engine_strict_header_check`) compiled every
strict public header on its own.

Notes:

- The first full build compiled 869 steps in 11 min 23 s with 3 jobs. ccache
  hit 1 of 933 cacheable calls: the CNA revision is newer than anything in the
  shared cache, so this run populated it.
- Two first-run failures were correct XNA Reach-profile behaviour (32-bit
  index buffers and `GetBackBufferData` are HiDef-only). `GpuMesh::Upload`
  now rejects 32-bit indices on a Reach device before allocating, and
  `CaptureBackBuffer` reports the Reach failure as a value; tests cover both
  profiles.
- CNA's C++ `Color` has a vtable, so CNA vertex structs are not byte images of
  the GPU layout. Engine interleaving writes the packed color explicitly; the
  GPU draw tests confirm the layouts render correctly.
