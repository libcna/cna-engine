# Notices and license inventory

cna-engine itself is MIT-licensed (`LICENSE`). Every source file carries an
`SPDX-License-Identifier: MIT` line.

## Dependencies (not redistributed here)

| Dependency | Used by | License | How it is used |
| --- | --- | --- | --- |
| CNA (`libcna/cna`) | all targets | Microsoft Public License (Ms-PL) | linked from a source tree; headers included |
| Sharp Runtime (`libcna/sharp-runtime`) | all targets (through CNA) | MIT | linked from a source tree |
| cna-extended (`libcna/cna-extended`) | `CnaEngine::ExtendedIntegration` only | MIT | linked from a source tree |
| GoogleTest (vendored in Sharp Runtime) | tests only | BSD-3-Clause | test framework, never installed |

A binary distribution that links CNA must carry CNA's Ms-PL terms for the CNA
portions; the engine adds no further obligations beyond MIT.

## Material copied into this repository

| File | Origin | License |
| --- | --- | --- |
| `tools/xna4_types.txt` | car-simulator `scripts/xna4_types.txt` at `cd6d948e` (generated from `libcna/xna4-spec`) | MIT |
| Rules of `tools/check_strict_boundary.py` | car-simulator `scripts/check_xna_only.py` (reimplemented, same rules) | MIT |
| `include/CnaEngine/Core/Noise.hpp` (the functions' arithmetic) | street-simulator `street/include/CnaStreet/Assets/Noise.hpp` at `fe0de621b9a51e9b02279778498cd729c9259551`; copyright Robert Vokáč | MIT |
| sRGB tables in `src/Graphics/Image.cpp` | room-simulator `src/Assets/Image.cpp` at `56e349c32c1d68522bbf082f588a1ca89726e714` (the transfer tables of `Image::halved`); copyright Robert Vokáč | MIT |
| `tests/fixtures/easy3d_legacy/` | Minimal equivalence oracle extracted from `openeggbert/easy-3d` at `ad63bf9c176af5a798ad05e75c21e8802dda0f2f`; copyright Robert Vokáč | MIT (license retained beside the fixture) |

No other code was copied from the games or from CNA's retired engine layer.
Engine APIs were designed from the pilots' implementations as evidence
(`docs/api-status.md`) and written for this repository.

## Read-only use of pilot sources

`tests/pilots/` compiles selected sources of car-simulator (MIT) and
street-simulator (MIT) from their own checkouts into test executables to
compare behaviour. Those sources are neither copied nor installed.

The retired easy-3d repository is not a build dependency. Its small frozen
oracle is compiled only into `Easy3DEquivalenceTests`, is never installed and
is retained solely to verify the completed migration.
