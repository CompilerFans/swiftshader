# FragCoord Quadrant Triangle Draw Test Design

**Goal:** Add a draw test where a normal triangle is rasterized by the vertex shader and the fragment shader uses `gl_FragCoord` to color different screen quadrants differently, while exporting a BMP artifact for inspection.

**Approach:** Reuse the existing `DrawTester` harness and BMP export flow. Keep the fragment shader screen-space based, but reduce the geometry from a fullscreen triangle to a normal triangle that spans the center of the frame, then assert colors only at sample points that lie inside the triangle and in different screen quadrants.

**Why this shape:** This preserves the `gl_FragCoord`-based fragment validation while making the visible result match the requested “ordinary triangle only” shape. It also keeps assertions stable because the sampled pixels are fixed screen coordinates.
