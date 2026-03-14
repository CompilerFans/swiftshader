# Graphics Executable Sampled-Image Plan Design

## Goal
Stop treating texture-bootstrap metadata as a single support boolean and instead model the sampled-image resource shape explicitly inside `GraphicsExecutable`.

## Context
The previous slices had already tightened the current bootstrap path around a single `COMBINED_IMAGE_SAMPLER` binding with direct sample passthrough. That kept false positives under control, but it also started to blur two separate questions:
- what sampled-image resource shape does this fragment shader use?
- is that shape currently materializable by the narrow bootstrap path?

If those questions stay collapsed into `hasBootstrapTextureBinding()`, every new boundary case turns into another one-off rejection rule.

## Chosen Approach
Introduce a texture plan in `GraphicsExecutable` with two layers:
- resource shape:
  - `CombinedImageSampler`
  - `SeparateImageSampler`
  - `Other`
- current bootstrap support:
  - only the narrow `CombinedImageSampler` case remains bootstrap-compatible for now

The compatibility accessor `hasBootstrapTextureBinding()` stays in place, but it becomes a derived view over the richer plan rather than the primary model.

## Why This Shape
- It gives `GraphicsExecutable` a stable place to express sampled-image layout knowledge without forcing draw-time bootstrap code to understand every resource form immediately.
- It lets tests assert meaningful distinctions between separate image/sampler and multiple combined samplers instead of flattening both into the same unsupported boolean.
- It prepares the next step, where draw-time materialization can grow from “single combined sampler only” to wider sampled-image plans without redoing the pipeline-time metadata API again.

## Scope of This Slice
In scope:
- add explicit sampled-image resource classification to `GraphicsExecutable`
- keep current bootstrap execution support unchanged
- update Vulkan pipeline introspection/tests to assert plan shape as well as bootstrap support

Out of scope:
- draw-time support for separate image/sampler
- broader descriptor/resource graph modeling
- expanding bootstrap execution beyond the current direct-sample combined-image-sampler path
