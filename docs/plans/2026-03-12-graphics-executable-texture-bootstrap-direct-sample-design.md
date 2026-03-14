# Graphics Executable Texture Bootstrap Direct-Sample Design

## Goal
Further narrow `GraphicsExecutable` texture-bootstrap metadata so it only marks fragment shaders that the current `FragmentBootstrapShaderKind::Texture2DColor` path can actually represent.

## Problem
The current texture-bootstrap metadata accepts any fragment shader that:
- reads `location 0` as `vec2`
- contains texture sampling
- converges to one descriptor set / binding
- uses a layout binding that is a single `COMBINED_IMAGE_SAMPLER`

That is still too broad. A shader such as `outColor = texture(texSampler, inTexCoord) * 0.5;` is currently marked as supported even though the bootstrap path only performs a direct texture sample and ignores the post-processing multiply.

## Narrowing Rule
For the current bootstrap path, require that `location 0` output is written directly from the result of a supported texture-sample instruction.

This deliberately rejects:
- texture sample followed by arithmetic or blending
- sample result routed through other value-producing instructions
- more complex fragment expressions that the current bootstrap path cannot reproduce

## Why This Shape
- It matches the semantics of the existing `Texture2DColor` bootstrap implementation.
- It avoids widening the runtime-side fragment bootstrap ABI in this slice.
- It is conservative and reversible: once `GraphicsExecutable` owns a richer fragment/bootstrap plan, this rule can be relaxed with evidence.
