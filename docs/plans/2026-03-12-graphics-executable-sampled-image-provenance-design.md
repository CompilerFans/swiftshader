# Graphics Executable Sampled-Image Provenance Design

## Goal
Refine `GraphicsExecutable` texture-plan extraction so sampled-image resource classification is driven by the descriptors that actually feed texture sampling instructions, not by every descriptor decoration present in the fragment shader.

## Context
The previous sampled-image plan slice introduced `CombinedImageSampler`, `SeparateImageSampler`, and `Other`. That was the right shape, but the implementation still over-approximates descriptor usage by collecting all fragment-shader descriptor decorations.

That over-approximation is now the limiting factor:
- a `sampler2D` texture path plus an unrelated UBO gets downgraded to `Other`
- a separate image/sampler path plus an unrelated UBO also gets downgraded to `Other`

This is not a bootstrap-policy issue. It is a provenance issue.

## Chosen Approach
Build the texture plan from actual sample-use provenance:
- scan texture sample instructions
- resolve the sampled-image operand backward through trivial value forwarding
- recover the descriptor objects that feed that sample
- classify the texture plan from those sampled-image descriptors only

The existing output-path rule for bootstrap support stays unchanged:
- `location 0 == vec2`
- output resolves to a supported texture sample through trivial pass-through
- single combined image sampler with `descriptorCount == 1`

## Why This Approach
- It broadens supported shader shapes without relaxing bootstrap execution semantics.
- It keeps non-sampled resources out of the sampled-image plan instead of misclassifying them as texture complexity.
- It scales better toward future work where draw-time materialization may consume richer sampled-image plans.

## Scope
In scope:
- sample-use-based sampled-image descriptor collection
- combined and separate sampled-image classification that ignores unrelated descriptors
- Vulkan regression coverage for unrelated UBOs alongside texture sampling

Out of scope:
- draw-time support for separate image/sampler
- non-uniform indexing and broader sampled-resource graphs
- reworking bootstrap output-path eligibility rules
