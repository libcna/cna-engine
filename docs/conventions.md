# API conventions and stability rules

Plan item: `ENG-013`. Applies to every public header under `include/CnaEngine/`.

## Profiles and headers

- Strict modules: `Core`, `Input`, `Diagnostics` (target `CnaEngine::Core`) and
  `Graphics`, `Assets` (target `CnaEngine::Graphics`). They may use only XNA
  4.0 types of CNA, Sharp Runtime and the standard library
  (ADR-001). `tools/check_strict_boundary.py` enforces this; every strict
  header must also compile on its own (`cna_engine_strict_header_check`).
- Enhanced module: `Extended` (target `CnaEngine::ExtendedIntegration`), the
  only place that may include `CNA/Extended/...`. Its headers start with a
  comment saying so.
- Public headers live in `include/CnaEngine/<Module>/`; everything in `src/` is
  private. A header in `src/` is never installed and never included by a
  public header.
- `Detail` namespaces are public only for technical reasons (templates,
  cross-file helpers) and carry no compatibility promise.

## Naming

- Namespaces mirror modules: `CnaEngine::Input`, `CnaEngine::Graphics`, ...
- Types and functions are PascalCase, like CNA's XNA surface. Engine APIs use
  plain accessors (`Position()`, `SetPosition()`); the XNA
  `getXProperty()`/`setXProperty()` style is reserved for XNA-compatible types.
- No identifier ends in `EXT`, and no engine name contains `CnaExt` or
  `CNA::`, even in the enhanced module, so names never collide with CNA's
  extension conventions.

## Errors

- Programming errors (invalid arguments, out-of-range ids, violated
  preconditions) throw `std::invalid_argument`, `std::out_of_range` or
  `std::logic_error`, and leave the object unchanged.
- Expected runtime failures that a game must handle (an asset that fails to
  load, a screenshot that cannot be written) are reported as values:
  `CaptureResult::success/error`, `AssetRegistry::TryGet()` and the
  registry's failure list, `BindingLoadResult::warnings`. They do not throw.
- Nothing is silently ignored: every fallback is visible through a report,
  statistic or failure record.

## Ownership and lifetime

- Values are owned by value or `std::unique_ptr`; shared ownership
  (`std::shared_ptr`) only where several holders are inherent (assets).
- Borrowed references are raw pointers or references and are documented as
  borrowed; the engine never deletes a borrowed object (e.g. `GpuMesh::Borrow`).
- GPU resources are RAII and move-only. Their documentation states what
  happens on device reset, context loss and device disposal.
- Nullable handles are `std::optional` or pointers; references are never null.

## Determinism

- Engine systems that depend on time or input take them as parameters
  (`InputSnapshot`, `elapsedSeconds`, injectable clocks); they never poll
  devices or read the wall clock implicitly, except in clearly named
  convenience functions (`InputSnapshot::Capture`).
- File names and text output are locale-independent and deterministic.

## Stability

- Everything is **experimental** while the version is `0.x` (ADR-001).
  Minor versions may break the API; the package uses `SameMinorVersion`.
- An API becomes stable only after it is exercised by both pilots
  (street-simulator and car-simulator) — plan item `PILOT-003`.
- `docs/api-status.md` lists each public facility with its status
  (`experimental`, `pilot-validated`, `stable`) and the consumers that
  exercise it.
- Pilot-validated headers are fingerprinted in `docs/api-snapshot.txt`; the
  `cna_engine.api_snapshot` test fails on any declaration change until the
  snapshot is regenerated (`tools/api_snapshot.py --update`) and the change is
  recorded in `CHANGELOG.md`.

## ABI

- No ABI stability is promised; consumers build the engine with the same
  compiler, standard library and CNA revision (`cmake/CnaEngineBaseline.cmake`).

## Contribution checklist

Before adding or generalizing a facility:

1. Name at least two materially different consumers (pilot projects, or a
   pilot plus an engine example) that need it. A facility with one consumer
   stays in that game.
2. Check that it does not duplicate CNA or cna-extended. Reuse first; fix
   upstream defects there (recorded in `upstream-issues.md`).
3. Decide strict or enhanced placement; strict code must pass the boundary
   check.
4. Record provenance and license compatibility of any adapted code.
5. Add unit tests; GPU-facing code also gets a software-renderer test that
   checks pixels.
6. Update `docs/api-status.md`.
