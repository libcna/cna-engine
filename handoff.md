# CNA Engine Handoff

Date: 2026-10-14

Audience: the next AI agent working on `cna-engine` and its immediate
`cna-extended` prerequisite

## Mission

Build a higher-level C++23 game-engine library above CNA. The engine should make
normal game development materially easier than using CNA's XNA 4.0-compatible
API directly, while preserving access to lower-level CNA facilities. It must
reuse `cna-extended` instead of recreating its screens, ECS, animation,
particles, fonts, tilemaps, collision helpers, and `World3DEXT` systems.

The intended long-term validation projects are:

- `cna-street`, representing the enhanced 3D rendering profile;
- `cna-car-simulator`, representing the strict-XNA profile.

Do not start by implementing a large engine surface. The upstream loader defect
is resolved; pin the dependency baseline, approve the strict/enhanced boundary,
and then build a small two-consumer vertical slice.

## User constraints

- Use no more than three CPU jobs for compilation. Use
  `cmake --build <dir> --parallel 3` or a lower value.
- Also keep tests at no more than three concurrent processes unless the user
  explicitly changes that instruction.
- Do not rebuild in `/tmp`.
- Use the single shared ccache at `/rv/cnaccache` with
  `CCACHE_BASEDIR=/rv`. Never create another ccache or set
  `CCACHE_MAXSIZE`.
- Reuse only the approved build directory names: `build`, `build-probe`,
  `build-consumer`, `build-asan`, `build-ubsan`, and `build-tsan`.
- Preserve unrelated and concurrent changes in sibling repositories.
- The analysis and planning documents are intentionally in English.

The repository-wide rules are in `../AGENTS.md`; read them before configuring
or building anything.

## Current `cna-engine` state

No engine implementation exists yet. The current repository has no commits and
the following untracked analysis artifacts:

- `analysis.md` — broad feasibility study across the simulator repositories;
- `gap-audit.md` — targeted build/API audit of CNA Extended, Street, and Car
  Simulator;
- `plan.md` — task plan with IDs, dependencies, acceptance criteria, milestones,
  and effort ranges;
- `cna-extended-readonly-loaders.patch` — historical copy of the upstream fix,
  retained for provenance; do not apply it again;
- `handoff.md` — this document.

The current `build`, `build-probe`, and `build-consumer` directories are audit
artifacts for Street, CNA Extended, and Car Simulator respectively. They are not
engine implementation trees. Do not delete them without first checking for
other active users as required by `../AGENTS.md`.

## Repository baselines observed during the audit

| Repository | Branch | Observed HEAD | State |
| --- | --- | --- | --- |
| `cna` | `next` | `de310727ce74bcd4b65e6f5147275a5bb1e637a6` at final verification | dirty (193 status entries) and changing concurrently |
| `cna-extended` | `develop` | `0061e3e17c30b2a3a302bce617e64765883079aa` | clean; matches `origin/develop` |
| `cna-street` | `develop` | `86b4a2e7c8744fbba45cff7a45803dee3cf70757` | clean |
| `cna-car-simulator` | `main` | `523b93b7444aa97437b583d0710d93a2fdf64bc1` | clean |

Always recheck these values. In particular, do not treat the observed CNA HEAD
as a reproducible baseline: its worktree contained 193 status entries at final
verification and was being changed by other work while the audit ran.

## Completed upstream fix

A second agent applied and committed the filename-loader correction directly in
`cna-extended`. Commit `0061e3e17c30b2a3a302bce617e64765883079aa`
(`fix: open filename loaders read-only`) is the current clean `develop` HEAD and
matches `origin/develop`. Its diff contains the expected seven files, 28
insertions, and 15 deletions; `git diff --check HEAD^ HEAD` is clean.

The file `cna-extended-readonly-loaders.patch` is retained only as historical
provenance for the issue and proposed change. Do not apply it to the current CNA
Extended tree. `EXT-001` is complete and is no longer an implementation blocker.

## Loader defect: exact cause

Sharp Runtime intentionally follows the .NET two-argument constructor contract:

```cpp
FileStream(path, FileMode::Open)
```

defaults to `FileAccess::ReadWrite`. Its single-path convenience constructor
delegates to that overload. A filename loader that only intends to read must
therefore request read access explicitly.

Two CNA Extended production call sites incorrectly relied on the single-path
constructor:

1. `Content::OpenFile(path)` in
   `src/CNA/Extended/Content/ExternalResourceResolvers.cpp`;
2. `BitmapFonts::Read(path)` in
   `src/CNA/Extended/Content/BitmapFonts/BitmapFontFileReader.cpp`.

They fail when a valid asset exists in a read-only checkout because the
underlying `std::fstream` requests write access. This caused four BMFont fixture
failures and the tiled example's failure to open `tileset.png` during the audit.

The required production form is:

```cpp
System::IO::FileStream stream(
    path,
    System::IO::FileMode::Open,
    System::IO::FileAccess::Read);
```

The committed fix also:

- corrects the misleading header comment claiming that the single-path
  constructor is read-only;
- asserts that `Content::OpenFile()` returns a readable, non-writable stream;
- exercises the filename overload of `BitmapFonts::Read()` directly;
- changes all remaining test-only single-path `FileStream` constructions to
  explicit read access.

## Completed filename-loader audit

The audit searched public headers, implementations, examples, and tests for
`FileStream`, `File`, `TitleContainer::OpenStream`, standard file streams, C file
APIs, and filename-oriented `Read`, `Load`, `Open`, `Parse`, `Deserialize`, and
`FromFile` entry points.

### Production sites corrected by the committed fix

| Path | Reason |
| --- | --- |
| `Content::OpenFile` | Direct single-path `FileStream`; central resolver used by tilemap code |
| `BitmapFontFileReader::Read(path)` | Direct single-path `FileStream` |

`Content::OpenFile` transitively covers external resources used by Tiled, LDtk,
Ogmo, `TilemapFactory`, image-dimension probing, and the tiled example. These
consumers should not each receive a separate workaround.

### Already read-only and needs no change

| API/callers | Evidence |
| --- | --- |
| `System::IO::File::ReadAllText` and `ReadAllBytes` | Sharp Runtime implements them with `std::ifstream` |
| `TexturePackerFileReader::Read(path)` | Uses `File::ReadAllText` |
| `ParticleEffectSerializer::Deserialize(fileName)` | Uses `File::ReadAllText` |
| `TiledTmxParser` main TMX load | Uses `File::ReadAllText`; external resources go through fixed `OpenFile` |
| `Tilemap3DFactoryEXT::BuildFromJsonFileEXT` | Uses `File::ReadAllText` |
| `BitmapFont::FromFile` and page textures | Use `TitleContainer::OpenStream` |
| JSON content loaders via `ContentManagerExtensions::OpenStream` | Use `TitleContainer::OpenStream` |

CNA's current `TitleContainer::OpenStream` explicitly constructs
`FileStream(realName, FileMode::Open, FileAccess::Read)`, so those call sites are
safe. No production `std::fstream`, `std::ifstream`, or `fopen` loader requiring
another fix was found inside CNA Extended.

### Test-only single-path constructions covered by the fix

- three remaining BMFont stream-overload cases;
- `TexturePackerFileReaderTests.ReadFromStreamParsesDocument`;
- `Tilemap3DFactoryEXTJsonTests.BuildFromJsonStreamEXT_ParsesDocument`.

The binary BMFont test is changed to call `Read(path)` so the production
filename overload has direct regression coverage.

## Completed verification

The committed fix was independently checked against the existing
software/headless/null `build-probe` tree. Compilation used
`cmake --build build-probe --parallel 3` with the shared ccache in read-only
mode. The results were:

- all 13 targeted `BitmapFontFileReaderTests`,
  `ExternalResourceResolversTests`, `TexturePackerFileReaderTests`, and
  `Tilemap3DFactoryEXTJsonTests` passed;
- the full suite completed with 2,369 passes, zero failures, and two known
  skipped `OrthographicCamera` containment tests out of 2,371 tests;
- the minimal example ran successfully;
- `tiled_demo` loaded its read-only map and tileset, rendered nine frames,
  reached its goal, and wrote `/tmp/cna_extended_tiled_demo_frame.png`;
- `world3d_demo` rendered ten frames and wrote
  `/tmp/cna_extended_world3d_demo_frame.png`.

A static search found no remaining single-argument `FileStream` construction in
CNA Extended public headers, implementations, examples, or tests. Other
filename-based loaders use the already read-only `File::ReadAllText`,
`File::ReadAllBytes`, or `TitleContainer::OpenStream` paths.

The verification used the observed dirty CNA working tree, so it establishes
the correctness of the loader fix but does not satisfy ADR-002's requirement
for a clean, reproducible compatibility baseline. Do not mark `EXT-002`
complete: the two camera skips remain a separate issue.

## Confirmed audit results

### CNA Extended

- Commit `0061e3e17c30b2a3a302bce617e64765883079aa` built against the
  then-current CNA working tree.
- 2,371 tests were discovered.
- All 13 tests directly affected by the loader fix passed.
- The full suite completed with 2,369 passes, zero failures, and two known
  orthographic-camera containment skips.
- Minimal, tiled, and World3D examples all passed.

### CNA Street

- Compiled successfully against the audited CNA tree.
- All 15 CTest entries passed.
- Street uses CNA's built-in `CNA_CNAEXT=ON`; that option is not the separate
  `cna-extended` library.

### Strict-XNA Car Simulator

- Compiled successfully with `CNA_CNAEXT=OFF`.
- Its static policy checker passed: 235 files scanned against 544 known XNA 4.0
  types.
- Four of seven CTest entries passed in the constrained audit environment.
- Two software-rendered display smoke tests hit 120-second timeouts.
- The aggregate 321-test binary hit its 900-second limit during `TrafficSoak`.
  Before timeout, one map-load performance assertion measured 13.44 seconds
  against a six-second budget.
- Treat these as unresolved environment/performance results, not as a clean pass
  and not as an API compilation failure.

## Recommended engine boundary

The names remain provisional until `ADR-001` is accepted:

```text
game
  |
  +-- CnaEngine::Core
  |     lifecycle helpers, input actions, diagnostics
  |
  +-- CnaEngine::Graphics
  |     cameras, mesh data/upload, assets, submissions, capture
  |
  +-- CnaEngine::ExtendedIntegration
        adapters for CNA::Extended screens, ECS, World3D, animation, particles
```

`Core` and `Graphics` should use only CNA's XNA-compatible public surface plus
Sharp Runtime and the standard library. `ExtendedIntegration` is the only
engine target that should depend on `CNA::Extended`.

This split is needed because the Car Simulator checker currently rejects
`CNA/` includes, `CNA::` namespaces, `EXT`-suffixed identifiers, and other
non-XNA APIs. Its eventual engine adoption requires a narrowly scoped allowance
for certified engine headers, not a general relaxation of CNA extension rules.

Street should use the enhanced path and reuse CNA Extended. If the strict target
is rejected during ADR review, Car Simulator remains a requirements oracle and
validation project rather than a direct consumer.

## What belongs where

### Keep or improve in CNA Extended

- screens and transitions;
- ECS and `World3DEXT`;
- animation and skinned-model systems;
- particles, fonts, tilemaps, debug drawing, billboards, and voxel helpers;
- generic collision primitives and spatial helpers;
- the read-only filename-loader fix;
- the separate orthographic-camera test resolution.

### Build in CNA Engine

- named input actions and rebinding;
- camera state and reusable controller interfaces;
- generic CPU mesh data and RAII GPU mesh upload;
- typed asset registry and lifetime policy;
- material and render-item descriptions;
- render queues and pass orchestration;
- culling, LOD, instancing, quality profiles, and capability fallbacks;
- diagnostics, render statistics, and deterministic screenshot capture;
- optional adapters that coordinate existing CNA Extended systems.

### Keep in games

- vehicle, train, ship, or domain physics;
- traffic, pedestrians, roads, city generation, and map-specific rules;
- chase/cockpit semantics;
- game-specific material catalogs, shader tuning, atmosphere, and art direction;
- any data format that has only one proven consumer.

## Next engine steps

Follow `plan.md`; do not skip the gates:

1. complete `ADR-001`, deciding strict/enhanced target dependencies, public
   namespace/include prefixes, and the Car Simulator checker contract;
2. complete `ADR-002`, pinning clean CNA and CNA Extended revisions;
3. create the C++23 CMake skeleton and installed-package consumer probes;
4. add automated public dependency-boundary checks;
5. implement the strict-compatible vertical slice in the order input actions,
   camera data, CPU mesh data, GPU mesh ownership, asset registry, diagnostics;
6. validate real code in Car Simulator before stabilizing those APIs;
7. add CNA Extended integration and one Street rendering vertical slice;
8. generalize only APIs proven by both pilots.

The expected effort is approximately 180–300 active AI-agent hours for a useful
MVP and 400–650 hours for a production-oriented first release validated by both
pilots. A broader Street-class renderer and extensive migration can reach
650–1,000 hours.

## Definition of a safe handoff continuation

Before claiming further progress, the next agent should be able to show:

- the exact writable repository and clean starting status;
- no build command exceeding three parallel jobs;
- no changes to the dirty CNA worktree beyond those already present;
- an accepted strict/enhanced boundary or an explicit statement that work is
  still limited to pre-ADR investigation;
- a clean, pinned compatibility baseline before publishing engine API;
- rerun evidence if the CNA or CNA Extended revisions change after this handoff.
