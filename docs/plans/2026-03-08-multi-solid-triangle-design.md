# Multi Solid Triangle Validation Design

**Goal / 目标**

Validate that the current graphics path can render multiple independent solid-color triangles in one frame, while keeping the existing single-triangle test and the current custom CUDA bootstrap unchanged.  
验证当前图形路径能在同一帧中渲染多个独立的纯色三角形，同时保持现有单三角形测试和当前 custom CUDA bootstrap 不变。

**Approach / 方案**

- Add one lightweight draw-record hook to `DrawTester` after pipeline and vertex-buffer binding.  
  在 `DrawTester` 中增加一个轻量 draw-record hook，插在 pipeline 和 vertex buffer 绑定之后。
- Preserve the current default behavior: if no hook is provided, record exactly one `draw(vertices.numVertices, 1, 0, 0)`.  
  保持当前默认行为：若未提供 hook，则仍然只录制一次 `draw(vertices.numVertices, 1, 0, 0)`。
- Add a new Vulkan draw test that uploads vertex data for three separated triangles and records three draw calls with different `firstVertex` offsets.  
  增加一个新的 Vulkan draw 测试，上传三组分离三角形顶点数据，并通过不同 `firstVertex` 偏移录制三次 draw。

**Validation / 验证**

- Assert three center pixels inside the triangles are red.  
  断言三个三角形内部中心像素为红色。
- Do not assert untouched background pixels in the current harness, because the single-sampled color attachment uses `eDontCare` load behavior.  
  当前 harness 的单采样颜色附件使用 `eDontCare` load 行为，因此不对未触及的背景像素做断言。
- Re-run the existing single-triangle test to guarantee no regression.  
  重跑现有单三角形测试，保证没有回归。
