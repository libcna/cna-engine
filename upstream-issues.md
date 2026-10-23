# Upstream issues found while building cna-engine

Defects and gaps found in sibling repositories (CNA, Sharp Runtime,
cna-extended, simulators). By agreement with the repository owner they are
**recorded here only** and not fixed in those repositories from cna-engine
work. Each entry says how cna-engine copes in the meantime. Since 2026-10-22
the owner of cna-extended has delegated its fixes to cna-engine's agent
(done in cna-extended itself), and CNA defects are sent to the Codex session
`cna-demos`, which works on CNA (`codex queue --thread <its id>`).

Status values: `open` (unresolved upstream), `worked around` (engine copes, an
upstream change would still be better), `resolved` (fixed upstream; keep the
entry for provenance).

---

## UP-001 — CNA and cna-extended install no CMake package

- Repositories: `libcna/cna` (`next`), `libcna/cna-extended` (`develop`)
- Status: resolved on 2026-10-22: cna-extended installs a package since 0.2.0 (`86870ad`), Sharp Runtime since `42847935`/`1dc8785b`, and CNA a relocatable C++ module SDK since `58ebc46fd` (`CNA_INSTALL_CXX`, made by the Codex session `cna-demos` at the owner's request). The engine pins CNA `cb377fa04`; its installed configuration looks up `find_package(CNA CONFIG COMPONENTS ...)` and `find_package(CnaExtended)` when the consumer has not defined their targets, and the consumer probes build from installed packages only
- Found: 2026-10-19, ENG-010

Neither project has `install(EXPORT ...)` or a `<Name>Config.cmake`; the only
supported consumption is `add_subdirectory()` of a source tree (cna-extended
consumes CNA that way too). A binary consumer of cna-engine therefore cannot
`find_package` its dependencies.

Engine handling: CNA targets are linked with `$<BUILD_INTERFACE:...>` only.
The installed `CnaEngineConfig.cmake` verifies that the consumer has already
defined `CNA::Core`, `CNA::Math`, `CNA::Input`, `CNA::GraphicsCore`,
`CNA::Content` (and `CNA::Extended` for the `ExtendedIntegration` component)
and re-attaches them to the imported targets. Once CNA exports a package, the
config file should call `find_dependency(CNA)` instead.

## UP-002 — Several XNA-compatible constructors are tagged `CNAEXT`

- Repository: `libcna/cna`, `modules/input` and `modules/math`
- Status: open (informational), reported to CNA on 2026-10-22 through the Codex session `cna-demos` (`codex queue`, at the owner's request); cna-engine watches CNA's history for commits naming this issue
- Found: 2026-10-19, ENG-020

`KeyboardState()`, `MouseState()`, `GamePadState()` default constructors and
compound assignment operators such as `Vector3::operator+=` are marked
`CNAEXT`. In C# these are implicit (struct default constructors) or synthesized
(`+=` from `+`), so strict-XNA code uses them without naming anything
CNA-specific. The strict boundary checker (identifier based) correctly accepts
them, but the tag makes it look as if strict code used extensions. Worth a note
in CNA's CNAEXT documentation that these are C++-language artefacts of XNA
semantics rather than extensions.

## UP-003 — Shared ccache cannot hit across build trees for `-g` builds

- Scope: workspace build rules (`../CLAUDE.md` / `../AGENTS.md`) and the shared
  cache configuration (`/rv/cnaccache/ccache.conf`)
- Status: resolved on 2026-10-22 by the owner's decision: the shared cache's `ccache.conf` sets `hash_dir = false`, and rule 1 of the workspace rules (`CLAUDE.md`/`AGENTS.md`) says so
- Found: 2026-10-19, ENG-012

The rules expect "77 of 78 builds of the same engine to be almost entirely
cache hits" thanks to `CCACHE_BASEDIR=/rv`. That cannot happen for Debug
builds: with `-g`, ccache hashes the current working directory unless
`hash_dir = false` (`CCACHE_NOHASHDIR`), and the shared configuration has the
default `hash_dir = true`. Observed: building the identical CNA revision with
identical flags in `cna-engine/build/` and then in `cna-engine/build-consumer/`
missed the cache for (nearly) every translation unit; the compile commands
differ only in the absolute build-directory include paths, which `base_dir`
already makes relative.

Suggested fix (not applied; the cache is shared): set `hash_dir = false` in
`/rv/cnaccache/ccache.conf`, or export `CCACHE_NOHASHDIR=1` in the documented
environment. The only cost is that `DW_AT_comp_dir` in cached debug info may
name the directory of the first compilation.

cna-engine's `tools/run_consumer_probes.sh` exports `CCACHE_NOHASHDIR=1` for
its own builds.

## UP-004 — `BoundingFrustum::Contains(Vector3)` deviates from XNA 4.0 (EXT-002 root cause)

- Repository: `libcna/cna`, `modules/math/src/BoundingFrustum.cpp`
  (`Contains(const Vector3&, ContainmentType&)`, around line 140 at
  `705ea232`)
- Status: resolved upstream in CNA `5ab1c7729` (2026-10-20, "Match XNA
  convex containment and squared-distance boundaries"): `Contains(Vector3)`
  now tests all six planes against XNA's `1e-5` tolerance and reports
  `Contains` for on-plane points. Published with a regression test for the
  orthographic case in `900d0db3b` (2026-10-20), the engine's pin since then;
  cna-extended's two tests are still skipped unconditionally in its sources
  and can be re-enabled by its owner (the CI lane reports when the skips
  disappear). The same
  commit implements `Intersects(BoundingSphere)` with XNA's GJK query, see
  `docs/pilots/car-simulator.md`.
- Found: 2026-10-19, while dispositioning plan item EXT-002

XNA 4.0 (`xna4-decomp/reference/xna4/decompiled/windows/Microsoft.Xna.Framework/Microsoft.Xna.Framework/BoundingFrustum.cs`,
`Contains(ref Vector3, out ContainmentType)`, line 572) classifies a point as
`Disjoint` when any plane distance exceeds `1E-05f` and as `Contains`
otherwise; it never returns `Intersects` for a point.

CNA instead:

1. compares against `0.0f` without XNA's `1E-05f` tolerance, so points within
   rounding distance outside a plane become `Disjoint` where XNA says
   `Contains`;
2. returns `Intersects` for a point exactly on a plane, which XNA never does;
3. `break`s out of the plane loop at the first exactly-on-plane hit, so a
   point lying on one plane but outside a later plane is reported
   `Intersects` instead of `Disjoint`.

Effect: the two skipped cna-extended tests
`OrthographicCameraTests.ContainsPoint_WithDefaultCamera_ReturnsCorrectContainment`
and `ContainsVector2_WithDefaultCamera_ReturnsCorrectContainment` test points
at Z = 0, exactly on the orthographic camera's far plane (near −1, far 0), and
get `Intersects`. Disposition for EXT-002: **implementation defect in CNA**, not
an invalid expectation and not a floating-point ambiguity in cna-extended. The
tests should be re-enabled unchanged once CNA follows the XNA reference.

Suggested fix: the XNA loop above (`if (distance > 1E-05f) { Disjoint; return; }`
for every plane, then `Contains`).

Engine handling: engine culling uses sphere and box containment, not point
containment, so it is unaffected; no workaround is added.

## UP-005 — cna-extended skinned-animation test uses an undeclared vertex buffer

- Repository: `libcna/cna-extended` (`develop` at `0061e3e1`), test
  `tests/CNA/Extended/World3DEXT/AnimationSystem3DEXTTests.cpp`
  (`MakeMovingTriangleModel`, line ~104)
- Status: resolved in cna-extended `6925fa3` (the test) and `59f179f` (the four production buffers and the World3D demo), published in `86870ad` (2026-10-22, the owner delegated cna-extended work); the engine pins it
- Found: 2026-10-19, ADR-002 rerun of the cna-extended suite against CNA
  `5a44d2c5`

`AnimationSystem3DEXTTest.DifferentAnimationTimes_ProduceDifferentPixelOutput`
now fails deterministically with:

> The current vertex declaration does not include all the elements required by
> the current vertex shader. BlendIndices0 is missing (SkinnedEffect needs a
> VertexDeclaration naming its elements).

Cause: CNA commit `e7bd98e46` (2026-10-15, "XNA conformance gaps closed …
stock-effect declaration inputs") rejects skinned draws from buffers created
with the CNA-only `VertexBuffer(GraphicsDevice&, int)` constructor, which has
no `VertexDeclaration`; XNA has no undeclared buffers. The test builds its
skinned triangle that way, so this is a test defect exposed by an intentional
CNA tightening, not a CNA regression. The 2026-10-14 audit ran against CNA
`de310727`, which predates `e7bd98e46`.

Suggested fix: construct the buffer with
`VertexBuffer(graphicsDevice, VertexPositionNormalTextureSkinned::VertexDeclaration, 3, BufferUsage::None)`
(or the type's declaration accessor).

Related risk: four production files in cna-extended still create undeclared
buffers for non-skinned geometry (`BillboardMeshEXT.cpp:44`,
`Text3DEXT.cpp:117`, `DebugDrawSystemEXT.cpp:76`,
`TilemapChunkRenderer3DEXT.cpp:226`). CNA still infers those layouts from the
stride, but the same conformance direction would break them; declaring their
layouts would make them XNA-conformant.

Engine handling: engine GPU meshes always pass a `VertexDeclaration`
(`VertexLayout::Declaration()`), so the engine is unaffected.

## UP-006 — The `CNA` umbrella and CNA's module targets give two runtimes in one process

- Repository: `libcna/cna` (`CNA_SHARED_LIBRARY`, default ON on Linux/GCC/Clang)
- Status: resolved upstream in CNA `470b12a5d` (2026-10-20, ISSUE-016), confirmed on 2026-10-22 by the Codex session `cna-demos` with an engine-style regression test (`a2f0378cf`), published and in the engine's pin `08c1387ec` since 2026-10-22. The engine's guidance to build CNA statically stays until the engine checks a mixed consumer itself
- Found: 2026-10-19, MILESTONE-2 Street scene comparison

With `CNA_SHARED_LIBRARY=ON`, the `CNA` umbrella target links `libcna.so`,
while the modular targets (`CNA::Math`, `CNA::GraphicsCore`, …) link static
archives. An executable that links both — a game linking `CNA` (as
street-simulator does) plus any library built on the modular targets (as
cna-engine and cna-extended are) — contains two copies of CNA's statics.
Observed: the process ran correctly but crashed at exit destroying
`GraphicsAdapter::adapters_` twice (one copy from the executable, one from
`libcna.so`). Worse failures (two graphics device registries, two content
caches) are plausible.

Suggestions for CNA: document that the umbrella and the module targets must
not be mixed, or make the module targets resolve to the shared library too
when `CNA_SHARED_LIBRARY=ON`, or detect the mix at configure time.

Engine handling: the Street probe links Street's core against CNA's static
module targets (the same ones the engine uses). A game adopting the engine
must do the same (or configure `CNA_SHARED_LIBRARY=OFF`); this is recorded in
`docs/guide-enhanced.md`.

## UP-007 — The pinned CNA revision is not published

- Repository: `libcna/cna` (remote `github.com/libcna/cna`)
- Status: worked around — on 2026-10-20 the owner chose to move the engine's
  pin to the published `688f3f23`; CI fetches every pin and runs green since
  the same day
- Found: 2026-10-20, while adding the EXT-003 upstream lane

The engine's baseline CNA revision (`5a44d2c5`, ADR-002) is on the local
`next` branch of the shared worktree, which is far ahead of what the remote
has: `git ls-remote` on 2026-10-20 showed `next` at `688f3f23`
(2026-10-17), `main` at `10a4ef1c` and `develop` at `72b85561`, and none of
them contains the pin. Every CI job that checks out `libcna/cna` at the pin
(`.github/workflows/ci.yml`: the engine job and the cna-extended lane) cannot
fetch it until CNA's owner pushes. The Sharp Runtime pin (`origin/next`) and
the cna-extended pin (`origin/develop`) are published.

Suggestion: push CNA's `next`, or move the engine's pin to a published
revision (ADR-002 "Moving the baseline": re-run the baseline evidence first).

Engine handling: none needed locally (`~/deps/cna-baseline` is a clone of the
local worktree at the pin). The weekly canary job checks out the remote
`next` head and is unaffected.

## UP-008 — street-simulator overstates its mesh memory by the vertex struct's vtable

- Repository: `libcnalab/street-simulator`, `street/src/Render/GpuMesh.cpp`
  (`gpuBytes_`), reported through `CityScene::BuildStats::meshBytes` and the
  benchmark's `meshBytes`
- Status: open (accounting only; rendering is unaffected); recorded in street-simulator's `docs/known-issues.md` (`14145cd`, 2026-10-22, not pushed)
- Found: 2026-10-20, MILESTONE-2 GPU resource comparison

`gpuBytes_ = vertices × sizeof(Geometry::Vertex) + indices × 4`, where
`Geometry::Vertex` is CNA's `VertexPositionNormalTangentTexture`. CNA's
vertex structs implement `IVertexType` and carry a vtable: `sizeof` is 56,
while the vertex buffer's declared stride (what the GPU stores) is 48. Measured
on the default city: Street reports 79,966,804 bytes for the meshes it uploads
itself, the engine's `GpuMemoryEstimate` of the same buffers 69,668,412 —
exactly 8 bytes for each of the 1,287,299 vertices; the index bytes agree.

Suggestion: use `getVertexDeclarationStatic().getVertexStrideProperty()`
instead of `sizeof` (the skinned mesh's accounting likely has the same
pattern).

Engine handling: none needed; the engine estimates from the declared stride.

## UP-009 — car-simulator's map-load budget fails in unoptimized GCC/Clang builds

- Repository: `libcnalab/car-simulator`, `tests/Map/SampleMapTests.cpp`
  (`SampleMap.LipovaLoadsAndIsFullyConnected`, around line 73)
- Status: open (test design; the engine excludes it by name in Debug probes); recorded in car-simulator's `docs/known-issues.md` (`3a34c80`, 2026-10-22, not pushed)
- Found: 2026-10-20, running car-simulator's suite against the pinned CNA
  (ADR-002 consumer check)

The test asserts `loadSeconds < 6.0`, the production loading budget. It
relaxes the bound for sanitizer builds (30 s) and for MSVC Debug (60 s), but
not for unoptimized GCC/Clang builds: in the engine's Debug `build-probe/` on a
loaded machine (load average ~20) the map took 16.0 s, so the test fails
although every other assertion of it holds and the remaining 320 tests pass.
The same fixture loads in about 15.6 s in each of the following tests.

Suggestion: relax the bound for any build without `NDEBUG` (or move the budget
into a Release-only benchmark), so the suite's result does not depend on the
build type and the machine's load.

Engine handling: `cna_engine.pilot.carsim_suite` excludes exactly this test
in non-Release configurations (`tests/pilots/CMakeLists.txt`).

## UP-010 — street-simulator's layout test discards a `[[nodiscard]]` result

- Repository: `libcnalab/street-simulator`, `tests/LayoutTests.cpp` (around
  line 202, `collector.builder(&b); collector.builder(&c);`)
- Status: open (latent: breaks the build only with `-DCNA_STREET_WERROR=ON`); recorded in street-simulator's `docs/known-issues.md` (`14145cd`, not pushed)
- Found: 2026-10-20, compiling Street's suite read-only for ADR-002

`GeometryCollector::builder()` is declared `[[nodiscard]]`; the test calls it
twice only for its side effect (registering a material), and GCC warns
(`-Wunused-result`). Street's warnings are not errors by default, so its own
build passes; with its `CNA_STREET_WERROR` option on, this test stops
compiling.

Suggestion: `(void)collector.builder(&b);` or `static_cast<void>(…)`, as the
test intends the call only for its effect.

Engine handling: none (the engine compiles Street's tests without `-Werror`).

## UP-011 — cna-extended's bitmap-font tests find their fixtures through `__FILE__`

- Repository: `libcna/cna-extended` (`develop`),
  `tests/CNA/Extended/BitmapFonts/BitmapFontFileReaderTests.cpp` (`FixturePath`)
- Status: resolved in cna-extended `f70b669`, published in `86870ad`; the engine pins it
- Found: 2026-10-20, rerunning the cna-extended suite against CNA `688f3f23`

`FixturePath` builds `std::filesystem::path(__FILE__).parent_path() / "files"
/ "bmfont"`, which assumes `__FILE__` is absolute. Compiled through ccache with
a base directory (`CCACHE_BASEDIR`, which every build on the shared
development machine sets), the compiler is given a relative source path, so
`__FILE__` is relative to the build root. CTest runs the test binary from
`<build>/tests` (`gtest_discover_tests`' default working directory), where that
relative path points nowhere, and the four `BitmapFontFileReaderTests` fail
with "Could not find a part of the path". Run from the build root, all four
pass. The first GitHub run of the CI lane (2026-10-20, no ccache base
directory) failed the same way — `__FILE__` was `../cna-extended/tests/...`
there too — so any build where the compiler sees a relative source path is
affected, not only builds through ccache.

Suggestion: pass the fixture directory as a compile definition
(`target_compile_definitions(... CNA_EXTENDED_TEST_FILES="${CMAKE_CURRENT_SOURCE_DIR}/...")`)
instead of deriving it from `__FILE__`, or give `gtest_discover_tests` a
`WORKING_DIRECTORY` that matches.

Engine handling: the CI lane lists the four by exact name in
`KNOWN_FAILURE` (and reports when they pass); the local rerun records them as
passing when run from the build root (`docs/verification.md`).

## UP-012 — street-simulator's statistics leave out blended instance groups

- Repository: `libcnalab/street-simulator` (`develop`),
  `street/src/Render/SceneRenderer.cpp`, `drawTransparent`
- Status: resolved: street-simulator's renderer runs on the engine since the
  adoption was merged into `develop` (2026-10-20), and the engine counts blended
  instance groups (checked on `develop` at `2e40f0c`, 2026-10-22).
- Found: 2026-10-20, comparing Street's benchmark output before and after
  its renderer moved onto the engine

`drawOpaque` adds an instance group's draws to `Stats::drawCalls` and
`Stats::instancedDrawCalls` and its triangles to `Stats::triangles`;
`drawTransparent` adds blended groups' draws to `drawCalls` only. Blended
instance groups therefore never appear in `instancedDrawCalls` or
`triangles`: in the `baseline` benchmark preset 8 instanced draws and 1,676
triangles a frame go uncounted (`crowd`: 9 draws, 2,892 triangles), and the
benchmark's instanced-draw figure undercounts.

Suggestion: count them in `drawTransparent` as `drawOpaque` does.

Engine handling: none needed.

## UP-013 — street-simulator's asset pipeline needs manual help to reach its full look

- Repository: `libcnalab/street-simulator` (`develop`), `assets/external/manifest.json`,
  `scripts/blender-tree-lod.py`, `scripts/blender-people.py`
- Status: open, recorded in street-simulator's `docs/known-issues.md` (`14145cd`, not pushed) (the application runs without these assets; they decide how
  close it looks to its reference screenshots)
- Found: 2026-10-20, preparing a full-content build of the adoption branch

1. `scripts/fetch-assets.sh` fetched 346 of 351 files; five fail their recorded
   SHA-256 because the upstream files changed: `GlassVaseFlowers.glb` and the
   four `mounted_fluorescent_lights_*_1k.jpg` maps. The manifest's checksums
   (or pinned URLs) need updating.
2. `scripts/blender-tree-lod.py` cannot export `tree_small_02` with Blender
   4.3: the script deletes every UV layer but the first, while the
   `tree_small_02_branches` material still reads `UV_map_01` through an
   Attribute node, and the glTF exporter fails with
   `KeyError: ... key "UV_map_01" not found`. Run here with a pre-step that
   points that node at `UVMap` (and removes the scene's other objects); the
   island and jacaranda trees and `blender-vehicles.py` work unchanged.
3. `scripts/blender-people.py` fails on its first run (`No module named
   'bl_ext.user_default.mpfb'`): it unpacks MPFB into the extension repository
   after Blender has scanned it. A second run, with `BLENDER_USER_EXTENSIONS`
   as an absolute path, builds all eight people.
4. The sound set (`scripts/prepare-audio.py`) needs the NOX Sound Essentials
   packs, a manual download. The manifest's source,
   `https://www.asoundeffect.com/sound-library/essentials-series/`, now
   returns "Page not found"; the same CC0 pack is published on itch.io as
   `Essentials_Series_NOX_SOUND.zip` (https://nox-sound-design.itch.io/essentials-series-sfx-nox-sound).
   Unpacked to `/rv/tmp/Essentials_Series_NOX_SOUND`, it gives all 22 derived
   sounds, and Street reports "audio 22 of 22 sounds loaded". The manifest's
   `source` and `acquisitionNote` should point at the new location.
5. `docs/screenshots/` no longer matches what `develop` draws with the full
   content set (other paving, other parked-car placement): the references
   predate later changes, and `scripts/check-screenshots.sh` would compare
   against an older picture.

Engine handling: none needed. With the full content set, Street before and
after its renderer moved onto the engine (renderer files of `64d1c07` against
`b137025`, 1024 × 576, hardware GPU) differ by 0.083/255 on average, 0.53 %
of pixels visibly — inside the agreed tolerance.


## UP-014 — galaxy-eggbert's editor draws wireframe on contexts that cannot

- Repository: `openeggbert/galaxy-eggbert` (`develop`),
  `src/GalaxyEggbert/Editor/EditorHighlightRenderer.cpp`, `Draw`
- Status: resolved in galaxy-eggbert `2d59be1` (2026-10-23, not pushed): without `GraphicsCapability::WireFrame` the highlight box's edges are drawn as lines through `CnaEngine::Graphics::DebugDraw`; verified live on an `OPENGLES3` build under Xvfb (OpenGL ES 3.2, Mesa 25.0.7). Originally recorded in galaxy-eggbert's `editor-wireframe-gles-bug.md` and `NEXT.md` (`5adf55b`)
- Found: 2026-10-20, driving the migrated editor under Xvfb with the native
  default renderer (`OPENGLES3`)

The editor's highlight box draws its edges with `FillMode::WireFrame`
without asking `GraphicsDevice::SupportsCapability(GraphicsCapability::WireFrame)`.
On an OpenGL ES context without `GL_NV_polygon_mode` CNA's EasyGL renderer
can only emulate wireframe for a Position+Normal mesh drawn with a stock
effect; the highlight box is Position+Texture (as it was with easy-3d's
`CubeMeshRenderer`), so CNA throws `System::NotSupportedException` and the
game terminates as soon as the editor shows a placement preview. Neither
llvmpipe's nor the Radeon 780M's (radeonsi) OpenGL ES context on this
machine offers `GL_NV_polygon_mode`, so the native OpenGL ES 3 build's editor
cannot be used here; the OpenGL 3.3 build (galaxy-eggbert's verified
configuration, `build-cna-opengl3`) has native polygon mode and is unaffected.

Suggestion: draw the edges with `FillMode::Solid` (or as a line list) when
the capability is missing.

Engine handling: none needed. `CnaEngine::Graphics::DebugDraw` draws box
outlines as lines and works on every context.

## UP-015 — cna-extended declares `SizeF` as a class and defines it as a struct

- Repository: `libcna/cna-extended` (`develop`),
  `include/CNA/Extended/Size.hpp` (`class SizeF;`) and
  `include/CNA/Extended/SizeF.hpp` (`struct SizeF`)
- Status: resolved in cna-extended `a6b0c56`, published in `86870ad`; the engine pins it
- Found: 2026-10-20, compiling every engine translation unit with Clang

Clang warns (`-Wmismatched-tags`) in every translation unit that includes
both headers, so a project compiling them with `-Werror` under Clang fails;
with the Microsoft C++ ABI the mismatch can also give link errors.

Suggestion: forward-declare `struct SizeF;`.

Engine handling: the engine treats CNA's and cna-extended's headers as
system headers for its own targets (`cna_engine_link_external`), so their
warnings no longer fail the engine's `-Werror` build.

## UP-016 — car-simulator's sun azimuth is mirrored between 00:00 and 01:00

- Repository: `libcnalab/car-simulator` (`main`, `9bbfefc`),
  `simulator/src/Render/LightingRig.cpp` (`LightingRig::SetTimeOfDay`)
- Status: open; recorded in car-simulator's `docs/known-issues.md` (`3a34c80`, 2026-10-22, not pushed)
- Found: 2026-10-21, comparing `CnaEngine::SunAnglesFor` with the rig

The hour angle is `(hours - 13) * 15` degrees and is not wrapped, so before
01:00 it is below -180 degrees; the azimuth is `acos(...)` mirrored to the
west only when the hour angle is positive, so for those hours it lands on the
wrong side of north. At 49.8 degrees north and a declination of 20 the
azimuth runs 338 degrees at 23:30, 345 at 23:59, then 15 at 00:01 and back
to 0 at 01:00, instead of on through 352 to 360. The sun is below the
horizon then, but the night key light (the moon) is derived from the same
direction, so the scene's light turns by about 30 degrees at midnight and
back over the following hour.

Suggestion: wrap the hour angle into [-180, 180) before the azimuth, or use
the two-argument form `atan2(-cos d sin H, cos l sin d - sin l cos d cos H)`,
which needs no branch.

Engine handling: `CnaEngine::SunAnglesFor` uses the two-argument form; the
sun probe (`tests/pilots/SunEquivalenceTests.cpp`) checks the rig against it
outside that hour and the mirror inside it.

## UP-017 — street-simulator's `--capture` exhausts memory while importing its models

- Repository: `libcnalab/street-simulator` (`develop`, `3611348`), built
  against the live `libcna/cna` checkout (`926db4ac9`) and cna-engine
  `develop`: Release, CNA's software renderer, headless platform
- Status: open, cause not investigated; recorded in street-simulator's `docs/known-issues.md` (`14145cd`, not pushed)
- Found: 2026-10-21, preparing the SHARE-001 noise comparison

`cna-street --capture DIR --width 640 --height 360 --no-overlay` grew to
23 GB resident while importing the Poly Haven models, before writing any
image, and the kernel's OOM killer ended it (the machine has 30 GB and no
swap). The same capture of all 18 viewpoints ran on 2026-10-20 during the
street adoption (`docs/pilots/migration.md`, commit `b137025`), and the
models are older than that (September). What changed since is the CNA
checkout (this build recompiled 199 CNA objects) and Street's adoption
commits after `b137025` (`513dd93`, derived materials in an `AssetRegistry`,
among them); which of them grows the import is not known.

Engine handling: none needed for the engine; SHARE-001 checked Street's noise
with `bake-assets --content` (215 surfaces, 85 MB peak) instead of a capture.

## UP-018 — The software renderer keeps a `Single` render target as 8-bit colour

- Repository: `libcna/cna` (baseline `900d0db3b`), software renderer
- Status: resolved: not a CNA defect. The Codex session `cna-demos` found (2026-10-22, CNA `d49c080d9` adds regressions) that CNA's default `GraphicsDevice` is Reach, where a `Single` render target is a preference that CNA, like XNA, replaces with `Color` and reports through `getFormatProperty()`; HiDef keeps float precision through Clear, Depth24 and fogged draws. The engine stored the requested format instead of the target's, so it read Color bytes as floats. Fixed in the engine the same day (`ShadowMap` takes `getFormatProperty()`); the text below is the original, mistaken report
- Found: 2026-10-22, GAME-031 (the engine's shadow map)

A `RenderTarget2D` created with `SurfaceFormat::Single` on the software
renderer accepts the format, but stores what is drawn into it as 8-bit RGBA:
reading it with `GetData(float*)` returns the four colour bytes of each texel
as one float (a white texel is `0xFFFFFFFF`, a NaN), and `GetData(Color*)`
returns the 8-bit values. The shadow receivers sample the same 8-bit red, so a
shadow map on the software renderer has 256 depth steps, not a float's. XNA
would either keep a float surface or refuse the format.

Engine handling: `ShadowMap::SuggestedBias` assumes nothing about the
precision behind `Single`; the shadow tests read a `Single` map as colour
when its floats come back as NaN (`tests/Graphics/ShadowMapTests.cpp`), with an
8-bit tolerance.

## UP-019 — Shadow receivers read a render-target shadow map the other way up on the software renderer than on EasyGL

- Repository: `libcna/cna` (baseline `900d0db3b`), software and EasyGL
  (OpenGL ES 3) renderers
- Status: resolved upstream in CNA `144d9f909` (2026-10-22, by the Codex session `cna-demos`; four producer/receiver regressions on SOFTWARE, GLES, GL33, WEBGPU, SDL_GPU and FNA3D), in the engine's pin `08c1387ec` since 2026-10-22. `CalibrateShadowMap` stays (it measures); its test now expects the software renderer the right way up
- Found: 2026-10-22, GAME-031

`IShadowReceiverEXT` receivers look the map up at `v = y * 0.5 + 0.5` of the
light's clip space. A shadow map drawn into a `RenderTarget2D` with the
light's own view-projection is received correctly on EasyGL (Mesa radeonsi,
`SDL_VIDEODRIVER=offscreen`), but on the software renderer the shadows land
mirrored across the light's view and half the ground darkens; drawn with the
projection's Y negated, it is the other way round. One of the two renderers
disagrees with the other about which row of a render target `v = 0` is (XNA:
row 0 is the top of the picture, and texture coordinate 0 its first row). A
producer cannot know which without asking.

Engine handling: `Enhanced::CalibrateShadowMap` draws a small shadow through
a receiving BasicEffect once and sets `ShadowMap::SetDrawnUpsideDown` from
where it fell; `Enhanced::EnableShadows` does this. Test
`ShadowReceptionTest.CalibrationFindsTheSoftwareRenderersOrientation` pins
the software renderer's current answer.

## UP-020 — CNA's published `next` needs an unpublished Sharp Runtime commit

- Repositories: `libcna/cna` (`origin/next` at `08c1387ec`, 2026-10-22),
  `libcna/sharp-runtime` (`origin/next` at `f2aecfef`)
- Status: resolved on 2026-10-22: Sharp Runtime published `next` at `1dc8785b` (with `c26614e8`), and the engine's pins moved to CNA `08c1387ec` and Sharp Runtime `1dc8785b`
- Found: 2026-10-22, moving the engine's CNA pin to the published revision
  that contains the fixes for UP-006 and UP-019

CNA `abbd8a4ea` (2026-10-21, "Fix CNA-DEMOS-ISSUE-099/100/101 video contracts
and frame lifetime") calls `System::Uri::getIsFileProperty()` and
`getLocalPathProperty()` in `modules/media/src/Xna/Video/Video.cpp`. Those
members come from Sharp Runtime `c26614e8` ("Fix CNA-DEMOS-ISSUE-101 URI local
paths and NUL file existence"), which is on the local `next` of the shared
Sharp Runtime worktree, five commits after the published `f2aecfef`, but not
on GitHub. Every published CNA revision from `abbd8a4ea` on, among them all
that contain UP-019's fix `144d9f909`, therefore fails to compile against the
published Sharp Runtime (both declare version 0.1.0, so CNA's version check
passes). CI, which fetches published revisions only, cannot build them.

Suggestion: push Sharp Runtime's `next` (up to `c26614e8`), or raise the
version CNA requires when it needs newer Sharp Runtime members.

Engine handling: the CNA pin stays at `900d0db3b` until Sharp Runtime is
published; the engine was built and tested locally against CNA `08c1387ec`
with Sharp Runtime `c26614e8` (`docs/verification.md`).
