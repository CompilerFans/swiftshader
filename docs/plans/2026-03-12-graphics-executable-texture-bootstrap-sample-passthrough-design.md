# Graphics Executable Texture Bootstrap Sample Passthrough Design

## Goal
Loosen the texture-bootstrap metadata rule just enough to accept fragment shaders that forward a sampled color through trivial pass-through values before writing `location 0`.

## Problem
The current direct-sample rule requires `location 0` to be written directly from the result of a supported image-sample instruction. That is safe, but likely too narrow for semantically equivalent shaders such as:

```glsl
vec4 sampledColor = texture(texSampler, inTexCoord);
outColor = sampledColor;
```

This shader still matches the current `Texture2DColor` bootstrap semantics, but the metadata extractor may reject it if the SPIR-V uses a trivial load/copy/store chain.

## Options
1. Keep the rule as-is.
   - Lowest risk.
   - Leaves a likely false negative in the supported narrow path.
2. Add general def-use tracing for arbitrary fragment expressions.
   - Too broad for this slice.
   - Risks accidentally re-accepting unsupported post-processing.
3. Accept only trivial pass-through chains from a supported image-sample instruction to the final `location 0` store.
   - Keeps the narrow bootstrap contract.
   - Fixes the likely false negative without widening semantics.

## Chosen Approach
Option 3.

Treat the final `location 0` value as supported when its def-use chain resolves to a supported image-sample instruction through a tiny set of pass-through instructions only. The pass-through set should stay intentionally small, such as:
- `OpLoad` from a function-local temporary
- `OpCopyObject`

Anything involving arithmetic, vector composition, blending, or multiple distinct stored values remains unsupported.
