# FragCoord Quadrant Draw Test Design

**Goal:** Add a lightweight draw test that verifies fragment shading based on `gl_FragCoord`, saves a frame artifact, and makes the result easy to inspect visually.

**Approach:** Reuse the existing `DrawTester` path and `saveFrame()` artifact support. Render a fullscreen triangle, use `gl_FragCoord` in the fragment shader to split the image into four quadrant colors, then assert representative pixels from each quadrant.

**Why this shape:** A quadrant-color image is easier to debug than a gradient, and its assertions are more stable than tolerance-based continuous color checks.
