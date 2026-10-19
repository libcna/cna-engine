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
