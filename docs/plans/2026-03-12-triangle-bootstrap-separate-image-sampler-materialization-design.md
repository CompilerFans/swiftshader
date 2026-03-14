# Triangle Bootstrap Separate-Image-Sampler Materialization Design

## Goal
Teach `TriangleBootstrapDraw` to materialize texture bootstrap state from a `SeparateImageSampler` texture plan, not just from a single combined image-sampler binding.

## Context
After the sampled-image plan work, `GraphicsExecutable` can already classify separate image/sampler shaders correctly. But strict GPU triangle bootstrap still only consumes the legacy combined-binding compatibility accessor. That means separate image/sampler pipelines still fall off the GPU bootstrap path even when their fragment shader is a narrow direct-sample passthrough.

## Chosen Approach
Keep the compatibility accessor for existing combined-only users, but let `TriangleBootstrapDraw` consume the richer texture plan directly:
- `CombinedImageSampler` materializes image + sampler from one binding
- `SeparateImageSampler` materializes image from the sampled-image binding and sampler state from the sampler binding
- `Other` remains unsupported

Pipeline-time support stays encoded in `texturePlan.bootstrapSupported`, so draw-time code only attempts materialization when the fragment path is bootstrap-compatible.

## Why This Shape
- It broadens actual GPU draw support without forcing every caller to stop using the compatibility accessor immediately.
- It keeps the plan model useful: separate image/sampler is no longer just metadata classification, it now drives real draw behavior.
- It avoids over-expanding scope into arbitrary sampled-resource graphs; only the already-classified one-image + one-sampler case is added.

## Scope
In scope:
- mark narrow direct-sample separate image/sampler plans as bootstrap-supported
- materialize fragment bootstrap config from separate image + sampler descriptors
- verify with the existing strict GPU draw regression case

Out of scope:
- arrays, multiple sampled-image resources, or broader `Other` handling
- changing the legacy `hasBootstrapTextureBinding()` compatibility accessor
