# Upstream issues found while building cna-engine

Defects and gaps found in sibling repositories (CNA, Sharp Runtime,
cna-extended, simulators). By agreement with the repository owner they are
**recorded here only** and not fixed in those repositories from cna-engine
work. Each entry says how cna-engine copes in the meantime.

Status values: `open` (unresolved upstream), `worked around` (engine copes, an
upstream change would still be better), `resolved` (fixed upstream; keep the
entry for provenance).

---

## UP-001 — CNA and cna-extended install no CMake package

- Repositories: `libcna/cna` (`next`), `libcna/cna-extended` (`develop`)
- Status: worked around
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
- Status: open (informational)
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
- Status: worked around in cna-engine scripts; workspace decision needed
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
  `Contains` for on-plane points. The engine's pin (`5a44d2c5`) predates it;
  cna-extended's two skipped tests can be re-enabled by its owner once the
  pin moves past it (the CI lane reports when the skips disappear). The same
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
- Status: open
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
- Status: worked around in the engine's probes; matters for engine adoption
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
  pin to the published `688f3f23`; CI can fetch every pin now
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
- Status: open (accounting only; rendering is unaffected)
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
- Status: open (test design; the engine excludes it by name in Debug probes)
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
- Status: open (latent: breaks the build only with `-DCNA_STREET_WERROR=ON`)
- Found: 2026-10-20, compiling Street's suite read-only for ADR-002

`GeometryCollector::builder()` is declared `[[nodiscard]]`; the test calls it
twice only for its side effect (registering a material), and GCC warns
(`-Wunused-result`). Street's warnings are not errors by default, so its own
build passes; with its `CNA_STREET_WERROR` option on, this test stops
compiling.

Suggestion: `(void)collector.builder(&b);` or `static_cast<void>(…)`, as the
test intends the call only for its effect.

Engine handling: none (the engine compiles Street's tests without `-Werror`).

