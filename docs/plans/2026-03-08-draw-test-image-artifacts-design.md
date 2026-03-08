# Draw Test Image Artifacts Design

**Goal / 目标**

Save rendered triangle results as directly viewable image files during draw tests so the output can be inspected outside the test log.  
在 draw 测试期间把渲染结果保存成可直接查看的图片文件，方便脱离测试日志观察输出。

**Approach / 方案**

- Add one test-side API to `DrawTester` that saves the current swapchain image to disk after rendering.  
  在 `DrawTester` 增加一个测试侧 API，用于在渲染后把当前 swapchain 图像保存到磁盘。
- Write a simple `BMP` file instead of introducing a third-party image library.  
  直接写一个简单的 `BMP` 文件，不引入第三方图片库。
- Save artifacts under `draw-test-artifacts/` relative to the build directory, with stable filenames per test.  
  将产物保存在 build 目录下相对路径 `draw-test-artifacts/`，每个测试使用稳定文件名。

**Validation / 验证**

- `DrawTest.SolidColorTriangle` should save one image artifact.  
  `DrawTest.SolidColorTriangle` 应保存一张图片产物。
- `DrawTest.MultipleSolidColorTriangles` should save one image artifact.  
  `DrawTest.MultipleSolidColorTriangles` 应保存一张图片产物。
- Existing pixel assertions must keep passing.  
  现有像素断言必须继续通过。
