# Graphics Executable Texture Bootstrap Separate-Image-Sampler Boundary Design

## Goal
Lock the current texture-bootstrap boundary so separate image/sampler descriptors remain explicitly unsupported by `GraphicsExecutable` texture metadata.

## Context
The current narrow texture-bootstrap path is intentionally modeled around a single `COMBINED_IMAGE_SAMPLER` binding. The draw-time bootstrap materialization path only knows how to read one combined sampled-image descriptor and turn it into `Texture2DColor` runtime state.

## Why Add Coverage Now
Separate image/sampler shaders are easy to accidentally re-admit while refining descriptor-binding extraction or sample-use analysis. The current behavior is correctly conservative, but it is only implicit.

## Chosen Approach
Add a Vulkan pipeline integration test that uses:
- `layout(binding = 0) uniform texture2D tex;`
- `layout(binding = 1) uniform sampler texSampler;`
- `texture(sampler2D(tex, texSampler), inTexCoord)`

Expected result: no texture-bootstrap binding metadata is exposed.

No production-code change is needed in this slice; the purpose is to lock the boundary with a real shader/pipeline test.
