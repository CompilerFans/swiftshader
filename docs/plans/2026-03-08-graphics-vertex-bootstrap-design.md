# Graphics Vertex Bootstrap Design

**Goal / 目标**

Replace the current placeholder graphics bootstrap kernel with a minimal CUDA kernel that actually looks like a vertex stage bootstrap: vertex input, vertex output, and `gl_Position`-style writeback semantics.  
把当前图形 bootstrap 占位 kernel 替换成一个最小但更接近真实 vertex stage 的 CUDA kernel：包含顶点输入、顶点输出，以及 `gl_Position` 风格的写回语义。

**Approaches / 方案**

- **Keep the current comment-only bootstrap**  
  Lowest effort, but the dump remains too far from the intended vertex path.
- **Generate a minimal vertex-style bootstrap source and launch it with dummy arguments** **(chosen)**  
  This keeps the current CPU fallback rendering path untouched, but makes the CUDA dump and launch contract much closer to the next real milestone.
- **Jump directly to full vertex shader lowering**  
  Too large for the next incremental step.

**Design / 设计**

- Introduce one reusable backend helper that owns:
  - the minimal CUDA source text for the graphics bootstrap kernel
  - the minimal runtime launch path for that kernel
- The kernel should include:
  - `struct VertexInput`
  - `struct VertexOutput`
  - a bounds check on `vertexCount`
  - `gl_Position`-style writeback with `w = 1.0f`
- `CustomExecutionBackend` should call this helper on the first graphics submit, then continue delegating real drawing to the CPU fallback.
- Disable warmup in the triangle tests that are meant to inspect draw-path dumps, so screen output reflects the graphics bootstrap rather than the runtime warmup kernel.

**Validation / 验证**

- Backend unit tests should verify the emitted CUDA source text contains the expected vertex structures and writeback code.
- Backend unit tests should verify the bootstrap launch uses three kernel arguments.
- Draw tests should stay green, and the simple triangle dump should show the new vertex-style bootstrap kernel.
