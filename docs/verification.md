# Verification record

Newest first. Each entry states the dependency revisions and worktree state,
the configuration, the commands, and the result. Failures and skips are
listed, never folded into a pass.

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
