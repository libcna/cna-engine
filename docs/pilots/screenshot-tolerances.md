# Screenshot tolerances (proposal for MILESTONE-2)

Status: **proposed, enforced, awaiting the owner's agreement.** MILESTONE-2
asks for "screenshot comparisons with agreed tolerances". The numbers below
are the engine's proposal. The scene comparisons already enforce them, so that
a rendering regression fails a test instead of only changing a printed number.
If the owner prefers other limits, `tests/pilots/ImageTolerance.hpp` is the one
place to change.

## The rule

The pilot renders a view, the engine renders the same view over the same
buffers, textures and materials, and the two images (480×270, 8-bit sRGB) are
compared pixel by pixel:

| Measure | Proposed limit | Why |
| --- | --- | --- |
| Mean absolute difference per channel | **≤ 0.5 / 255** | catches any global change: lighting, exposure, fog, blending, color space (the Street comparison before its lighting was carried over measured 19 / 255) |
| Pixels with a channel difference above 3 / 255 | **≤ 1 % of the image** (1,296 pixels) | catches local changes a viewer can see; one- to three-level differences are rounding (car-simulator's lit facades: 13,891 pixels differ by exactly one level) |

Visibility is checked separately and exactly: the engine must reach the same
visible sets (items, instances, figures) as the pilot, so a missing object
fails even when it covers few pixels.

## Measured on 2026-10-21 (baseline build, software renderer, Debug)

| Comparison | Mean | Pixels differing | Visibly (> 3 / 255) | Share | Within |
| --- | --- | --- | --- | --- | --- |
| car-simulator, day | 0.05 | 14,030 | 39 | 0.03 % | yes |
| car-simulator, night | 0.00 | 800 | 37 | 0.03 % | yes |
| car-simulator, night with headlights | 0.01 | 809 | 47 | 0.04 % | yes |
| Street, first viewpoint (traffic, pedestrians) | 0.01 | 68 | 56 | 0.04 % | yes |
| Street, walk driven by engine input | 0.21 | 884 | 620 | 0.48 % | yes |
| Street, pedestrian close-up | 0.13 | 781 | 775 | 0.60 % | yes |

The largest shares come from the two Street views whose camera orientation
passes through both programs' yaw handling (a 1.7e-6 view-matrix difference)
and from Street's coplanar ground meshes, whose depth ties such a difference
flips (`docs/pilots/street-simulator.md`). The limit leaves room of about 1.7×
over the worst measured view; a regression of the kinds seen during the work
(a missing lighting term, wrong blend state, inherited sampler) exceeds it by
far.

## Not covered by this rule

- Renderers other than the software renderer: GPU rasterizers round
  differently; a GPU run would need its own measurement before the same
  limits are applied.
- Frames the pilots draw with passes the engine does not take over
  (car-simulator's mirrors, rain and snow; Street's reflection-probe
  bake, which the probes mode measures by hand: 75 differing pixels).
- Performance: CPU frame times are reported next to the images but have no
  budget yet (see `docs/verification-matrix.md`, "Performance").

## Decision needed

Accept the two limits as MILESTONE-2's agreed tolerances, or name others.
