# ADR-001 — Strict and enhanced dependency boundary

Date: 2026-10-19

Status: accepted

Plan item: `ADR-001` in `plan.md`

## Context

`cna-engine` must serve two kinds of consumer:

- enhanced games such as `street-simulator` (formerly `cna-street`), which may
  use CNA extensions and `cna-extended`;
- strict-XNA games such as `car-simulator` (formerly `cna-car-simulator`),
  whose `scripts/check_xna_only.py` rejects `CNA/` includes, `CNA::`
  identifiers, any `EXT`-suffixed identifier, renderer identity queries, and
  platform headers.

A single engine target would force one policy on both. The gap audit therefore
proposed a split; this record accepts it and fixes the names.

## Decision

### Targets

| CMake target | Alias | Depends on | Profile |
| --- | --- | --- | --- |
| `cna_engine_core` | `CnaEngine::Core` | `CNA::Math`, `CNA::Input`, Sharp Runtime core, std | strict |
| `cna_engine_graphics` | `CnaEngine::Graphics` | `CnaEngine::Core`, `CNA::GraphicsCore`, `CNA::Content` | strict |
| `cna_engine_extended` | `CnaEngine::ExtendedIntegration` | strict targets, `CNA::Extended` | enhanced |
| `cna_engine` (INTERFACE) | `CnaEngine::Engine` | all of the above that were built | enhanced |

```text
                    game
          +-----------+-----------+
          |                       |
   (strict consumer)       (enhanced consumer)
          |                       |
          |               CnaEngine::Engine
          |                       |
          |          CnaEngine::ExtendedIntegration ---> CNA::Extended
          |                       |                          |
          +------> CnaEngine::Graphics <------+              |
                          |                                  |
                    CnaEngine::Core                          |
                          |                                  |
       CNA XNA-compatible modules (Math, Input, GraphicsCore, Content)
                          |
                    Sharp Runtime
```

`CnaEngine::ExtendedIntegration` is the **only** engine target that may link or
include `cna-extended`. It is built only when `CNA_ENGINE_WITH_EXTENDED=ON`.

### Public include prefixes and namespaces

| Target | Include prefix | Namespace |
| --- | --- | --- |
| Core | `CnaEngine/Core/`, `CnaEngine/Input/`, `CnaEngine/Diagnostics/` | `CnaEngine`, `CnaEngine::Input`, `CnaEngine::Diagnostics` |
| Graphics | `CnaEngine/Graphics/`, `CnaEngine/Assets/` | `CnaEngine::Graphics`, `CnaEngine::Assets` |
| ExtendedIntegration | `CnaEngine/Extended/` | `CnaEngine::Extended` |

The prefix `CnaEngine/` and the namespace `CnaEngine` were checked against the
car-simulator rules: `(^|/)CNA/` (case-insensitive), `CnaExt|CNAEXT` and
`\bCNA\s*::` do not match them.

### Strict surface rules

Every header and source file of `CnaEngine::Core` and `CnaEngine::Graphics`
must satisfy the same rules as the car-simulator checker:

- includes only `Microsoft/Xna/Framework/...` headers whose type is in the XNA
  4.0 type list, `System/...` and `SharpRuntime/...` headers, the C++ standard
  library, and `CnaEngine/` strict headers;
- no `CNA/`, `/Internal/`, `CnaExt`, SDL, GL, Vulkan, Direct3D or renderer
  headers;
- no `CNA::`, `CNAEXT`, `EXT`-suffixed identifiers or other forbidden
  identifiers (`ShaderEffect`, `PbrEffect`, `GraphicsCapability`, raw GL/VK
  calls, ...);
- no strict header may include `CnaEngine/Extended/`.

The rule set is enforced by `tools/check_strict_boundary.py`, registered as a
CTest test (`cna_engine.strict_boundary`). A deliberate violation fixture
(`tests/boundary/violations/`) proves the checker fails (ENG-011).

CNA's own XNA headers internally include `CNA/` helpers; this is acceptable
because the rules concern what *engine and game sources* name, which is also
how the car-simulator checker scans.

### Car-simulator checker contract

The engine does not modify `car-simulator`. When that project adopts the
engine, its checker needs one narrow allowance, proposed here as the contract:

1. accept `#include "CnaEngine/Core/..."`, `"CnaEngine/Input/..."`,
   `"CnaEngine/Diagnostics/..."`, `"CnaEngine/Graphics/..."` and
   `"CnaEngine/Assets/..."`;
2. keep rejecting `CnaEngine/Extended/...` and every other rule unchanged;
3. pin the engine revision whose strict boundary check passed, so the
   allowance covers audited headers only.

The engine side of the contract is the strict boundary check above: the
allowance is safe exactly because the strict headers pass the same rules.

Verified with the pilot's unmodified checker: the CTest test
`cna_engine.pilot.car_simulator_checker_allowance`
(`tests/pilots/run_pilot_checker.py`) runs car-simulator's
`scripts/check_xna_only.py` over copies of every strict engine header and
source and fails unless every reported violation is an include of a strict
`CnaEngine/<module>/` header.

### Versioning

- Project version `0.1.0`; everything is experimental until two-consumer
  validation (PILOT-003).
- `CnaEngine/Core/Version.hpp` exposes the version as constants.
- An installed package exports `CnaEngineTargets.cmake` and
  `CnaEngineConfig.cmake` with `SameMinorVersion` compatibility while the
  major version is 0.

### Dependency provisioning

CNA and `cna-extended` do not install or export CMake packages (see
`upstream-issues.md`). The engine therefore consumes them as source trees, like
`cna-extended` does: the top-level project adds `../cna` (and optionally
`../cna-extended`) unless a parent project already defined `CNA::Core`. The
installed `CnaEngineConfig.cmake` requires the consumer to define the CNA
module targets before `find_package(CnaEngine)` and fails with a clear message
otherwise.

## Addendum (2026-10-19) — `CnaEngine::EnhancedGraphics`

Enhanced rendering needs CNA's own graphics extensions (`PbrEffect`, alpha
modes, texture transforms) without needing cna-extended. A second enhanced
target is therefore added:

| CMake target | Alias | Depends on | Profile |
| --- | --- | --- | --- |
| `cna_engine_enhanced` | `CnaEngine::EnhancedGraphics` | `CnaEngine::Graphics`, CNA graphics extensions | enhanced |

- Include prefix `CnaEngine/Enhanced/`, namespace `CnaEngine::Enhanced`;
  built when `CNA_ENGINE_WITH_ENHANCED_GRAPHICS=ON` (default) and part of
  `CnaEngine::Engine`.
- It must not include `CNA/Extended/` headers; `ExtendedIntegration` stays
  the only cna-extended user.
- Strict code reaches enhanced rendering only through the strict
  `IMaterialBinder` interface (`Graphics/MaterialBinder.hpp`): the strict
  `ForwardRenderer` never names `PbrEffect`, and the strict boundary check is
  unchanged.

## Consequences

- Strict consumers link `CnaEngine::Core` and/or `CnaEngine::Graphics` only.
- Enhanced consumers link `CnaEngine::Engine`.
- New strict APIs must pass the boundary check; anything needing CNA
  extensions belongs in a separate, explicitly enhanced target.
- If car-simulator rejects the allowance, it stays a requirements oracle and
  validation project, as stated in the gap audit.
