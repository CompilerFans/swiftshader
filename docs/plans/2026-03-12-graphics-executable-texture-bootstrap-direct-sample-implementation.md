# Graphics Executable Texture Bootstrap Direct-Sample Implementation

## TDD Steps
1. Add a Vulkan pipeline integration test whose fragment shader computes `texture(...) * 0.5` while keeping the existing narrow texcoord/layout shape.
2. Confirm the test fails because `GraphicsExecutable` still exposes texture-bootstrap binding metadata.
3. Tighten `GraphicsExecutable.cpp` so the texture-bootstrap path requires `location 0` to store the direct result of a supported image-sample instruction.
4. Re-run focused Vulkan, backend, and draw verification.

## Scope
- `tests/VulkanUnitTests/GraphicsBackendPipelineTests.cpp`
- `src/Backend/GraphicsExecutable.cpp`
- bring-up / tracking documents

## Non-Goals
- No draw-time texture descriptor/sampler materialization changes
- No richer fragment-expression lowering
- No expansion beyond the current single combined-image-sampler texture bootstrap path
