# Frozen simulator oracles

The implementations car-, school-, train- and street-simulator used before
they moved onto the engine's shared facilities (SHARE-001). The equivalence
probes in `tests/pilots` compare the engine against these copies: once a game
delegates to the engine, compiling its current sources would only compare the
engine with itself.

| File | Original | Commit |
|---|---|---|
| `MipOracles.cpp` `Car*` | car-simulator `simulator/src/Render/Image.cpp` (`Image::Downsampled`, `UploadTexture`, `UpdateTexture`) | `64711d575ffcfb71b359a8f1ea0ea5f25c2f75d8` |
| `MipOracles.cpp` `School*` | school-simulator `src/Rendering/ImageUtil.cpp` (`BuildMipChain`, `CreateFromMipChain`, `CreateMipmapped`) | `21ab2f01df04b514bce3b7c2aa6bff16ebb97525` |
| `MipOracles.cpp` `Train*` | train-simulator `src/render/ProceduralMaterials.cpp` (`ProceduralImage::downsampled`, `downsampledForAlphaTest`, `uploadMipmappedTexture`) | `ea8941c7fcf3d943edd01377c2bb8acf85482806` |
| `TrainSettings.hpp/.cpp` | train-simulator `include/trainsim/config/Settings.hpp`, `src/config/Settings.cpp` | `ea8941c7fcf3d943edd01377c2bb8acf85482806` |
| `StreetNoise.hpp` | street-simulator `street/include/CnaStreet/Assets/Noise.hpp` | `36113486d433d4c04c640560cbefe8b8bcbe4972` |

The arithmetic is unchanged. Only the namespaces (`simulator_oracles`), the
containers of the mip code (`Pixels` for car's `Image` and train's
`ProceduralImage`) and the include paths differ from the originals.

Like `easy3d_legacy`, this code is private to the probe executables: it is not
installed and must not be used by production targets. room- and
house-simulator, which have not moved onto the engine, are still compiled
read-only from their checkouts.

The originals are MIT-licensed; see `LICENSE`.
