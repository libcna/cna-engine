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
- Status: open — on 2026-10-20 the shared CNA worktree had uncommitted work
  in progress implementing XNA's tolerance in `Contains(Vector3)` (and XNA's
  GJK query for `Intersects(BoundingSphere)`, see
  `docs/pilots/car-simulator.md`); re-check when it is committed
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
- Status: open (blocks the engine's CI, not local work)
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
