# DX12 最小 Lumen 实现基础建设规划

更新日期：2026-08-31

## 1. 目标与边界

目标是在当前 DX12 纹理三角形样例上，逐步实现一个轻量的、用于学习和验证核心思路的 Lumen-like 全局光照原型。

第一阶段最终效果定义为：

- Cornell Box 静态场景；
- 延迟渲染 GBuffer；
- 屏幕空间射线追踪；
- 屏幕外命中由 DXR Inline RayQuery 补充；
- 固定容量 Surface Cache；
- 低分辨率 Screen Probe；
- 一次漫反射间接光；
- 简单的空间滤波、上采样和历史累积。

这不是 UE5 Lumen 的完整复刻。首版明确不覆盖软件距离场光追、Nanite、世界分区、资源流送以及复杂动态场景。

## 2. 当前 DX12 基线

当前已经具备：

- Win32 窗口与消息循环；
- DX12 Debug Layer；
- 硬件适配器和 WARP 选择；
- Device、Direct Command Queue；
- 双缓冲 SwapChain；
- RTV、SRV Descriptor Heap；
- Root Signature 和 Graphics PSO；
- HLSL VS/PS 编译；
- Vertex Buffer；
- 默认堆纹理和 Upload Heap 上传；
- Command List、Bundle；
- Resource Barrier；
- 双缓冲 FrameContext 和按帧 Fence/Event 同步；
- 持续 Update/Render 主循环；
- D32_FLOAT Depth Buffer 和 DSV；
- Default Heap 静态 Vertex/Index Buffer；
- 每帧独立、256 字节对齐的 Camera Constant Buffer；
- 使用索引绘制的程序化 Cornell Box（路径追踪常见双球体变体）；
- 顶点法线/反照率和最小环境光 + Lambert 直接光照。

当前主要不足：

- 没有 Resize；
- 只有固定 View/Projection 和顶点材质，尚无可交互相机、场景对象系统和 GBuffer；
- 没有 Compute Shader 基础；
- 仍使用 `D3DCompileFromFile` 和 Shader Model 5.0；
- 没有 DXR 加速结构和 RayQuery；
- 键盘回调和逐帧更新为空。

`nvpro_core` 保持为可选外部依赖。DX12 基础建设不得依赖它；缺少该依赖时，DX12 和 Software 目标仍需独立配置、构建和运行。

## 3. 设计原则

1. 只抽象已经出现两次以上的重复逻辑。
2. 首版只使用一个 Graphics Queue。
3. 使用固定容量 Descriptor Heap，不做虚拟化。
4. 手动编排 Pass 和资源状态，不先实现 Render Graph。
5. 静态场景优先，动态对象后置。
6. 硬件光追优先，暂不实现 Mesh SDF 软件光追。
7. 每个阶段都必须有可视化结果和独立验收条件。
8. 调试能力与渲染功能同步建设，避免后期补诊断系统。

## 4. 推荐的最小代码结构

不建立复杂继承体系，仅拆分职责：

```text
TrD3D12Renderer/
  DxContext.*          Device、Queue、SwapChain、Fence
  FrameContext.*       每帧 CommandAllocator、FenceValue、上传偏移
  GpuResource.*        Buffer/Texture、资源状态和 Barrier 辅助
  DescriptorHeap.*     RTV、DSV、CBV/SRV/UAV 的简单线性分配
  UploadContext.*      静态资源上传
  Scene.*              Camera、Mesh、Material、Instance
  Renderer.*           帧流程和 Pass 调度
  Passes/
    GBufferPass.*
    LightingPass.*
    HzbPass.*
    ScreenTracePass.*
    RayQueryPass.*
    SurfaceCachePass.*
    ScreenProbePass.*
    CompositePass.*
```

Pass 初期可以只是普通函数或小类。不要建立通用节点系统、反射注册系统或插件框架。

## 5. 实施阶段

### 阶段 0：稳定的逐帧运行骨架

任务：

- 将 `Update/Render` 从单纯的 `WM_PAINT` 回调移到空闲消息循环；
- 处理 `WM_SIZE`，重建 Back Buffer 和 RTV；
- 建立两个 `FrameContext`，对应双缓冲 SwapChain；
- 每个 FrameContext 保存 CommandAllocator 和 FenceValue；
- 只在复用该帧资源时等待 Fence，不在每次 Present 后立即全局等待；
- 修正 Fence Event 创建失败判断；
- 启用 Debug Layer、GPU-Based Validation 和 DRED；
- 为关键 DX12 对象设置调试名称。

验收：

- 窗口可持续渲染并正确缩放；
- 连续运行至少 1000 帧；
- Debug Layer 无资源状态和生命周期错误；
- CPU 不再因为每帧 Fence 等待而完全串行。

### 阶段 1：最小资源与描述符管理

任务：

- 实现轻量 `GpuBuffer` 和 `GpuTexture`；
- 资源对象记录当前 `D3D12_RESOURCE_STATES`；
- 提供 Transition 和 UAV Barrier 辅助函数；
- 建立 CPU 可见 RTV、DSV Heap；
- 建立一个固定容量、Shader Visible 的 CBV/SRV/UAV Heap；
- 使用线性索引分配描述符，暂不回收；
- 建立一次性静态上传和每帧 Upload Ring Buffer；
- 静态 Vertex/Index Buffer 放入 Default Heap；
- 支持创建 Graphics PSO 与 Compute PSO；
- 接入 DXC，最低目标 Shader Model 6.5。

验收：

- Indexed Geometry 使用 Default Heap 正常绘制；
- Compute Shader 可以读 SRV、写 UAV；
- RenderDoc/PIX 中资源名称和状态清晰可辨。

### 阶段 2：场景与 GBuffer

先用程序生成的 Cornell Box，避免模型导入分散注意力。

任务：

- Camera、View、Projection；
- Mesh：Position、Normal、UV、32 位 Index；
- Instance Transform；
- Material：BaseColor、Roughness、Metallic、Emissive；
- Depth Buffer 和 DSV；
- GBuffer 建议：
  - `RGBA8_UNORM`：BaseColor + Roughness；
  - `RGBA16_FLOAT`：World Normal + Metallic；
  - `D32_FLOAT`：Depth；
  - `RGBA16_FLOAT`：HDR Lighting；
- 一个方向光和最简单的直接光照 Pass；
- 全屏三角形 Composite Pass；
- GBuffer 各通道调试视图。

验收：

- Cornell Box 正确显示；
- 可分别显示 BaseColor、Normal、Depth 和 Direct Lighting；
- 深度遮挡、相机移动和投影正确。

### 阶段 3：Compute 与屏幕空间工具

任务：

- 全屏/分块 Compute Dispatch；
- HZB 深度金字塔生成；
- 屏幕空间射线步进；
- Ping-Pong 历史纹理；
- Depth/Normal 双边上采样；
- GPU Timestamp；
- Hit Mask、Hit Distance、Ray Direction 等调试输出。

验收：

- HZB 每级可视化正确；
- 屏幕空间射线可以命中当前画面内几何；
- 能清楚显示命中、未命中和越界三种情况。

### 阶段 4：最小 DXR Inline RayQuery

使用 DXR 1.1 Inline RayQuery，避免首版引入完整 Ray Generation/Miss/Hit Shader Table 管线。

任务：

- 检测 `D3D12_FEATURE_D3D12_OPTIONS5` 和 Raytracing Tier；
- 为静态 Mesh 构建 BLAS；
- 构建或按需更新简单 TLAS；
- 在 Compute Shader 中执行 `RayQuery`；
- 输出 Instance ID、Primitive ID、命中距离和重心坐标；
- 屏幕空间追踪失败时才执行 RayQuery。

验收：

- 屏幕外几何仍可被射线命中；
- 可视化结果能区分 Screen Trace 命中与 RayQuery 命中；
- 静态场景下 BLAS 不重复构建。

### 阶段 5：固定 Surface Cache

首版不实现虚拟化和动态 Card 调度。

任务：

- 每个静态 Mesh 手工或按六个轴向方向生成固定 Card；
- 使用固定尺寸 Atlas；
- 缓存 BaseColor、Normal、Emissive 和 Direct Lighting；
- 建立 Ray Hit 到 Card UV 的映射；
- 提供 Card、Atlas 占用和 Lookup 结果调试视图。

验收：

- Cornell Box 各表面能稳定映射到 Surface Cache；
- 从屏幕外射线命中点能够查询到合理的表面光照；
- Atlas 满时明确报错，不实现复杂驱逐策略。

### 阶段 6：Screen Probe GI

任务：

- 以 8×8 或 16×16 像素为单位布置低分辨率 Screen Probe；
- 每个 Probe 发射固定数量方向射线；
- 优先 Screen Trace，失败后使用 RayQuery；
- 在命中点查询 Surface Cache；
- 将结果投影为低阶漫反射辐照度；
- 空间滤波和深度/法线引导上采样；
- 静态相机历史累积；
- 最后再加入 Motion Vector 和动态相机 History Rejection。

验收：

- 关闭直接光后仍能观察到一次漫反射间接照明；
- 墙面颜色能够产生基本 Color Bleeding；
- 静态画面随时间收敛且噪声下降；
- 遮挡边缘不过度漏光。

## 6. 首版明确不做

- Mesh SDF 和 Global Distance Field；
- Nanite 或虚拟几何；
- Render Graph；
- Async Compute 和多队列同步；
- Bindless Descriptor 虚拟化；
- 通用 GPU 内存分配器；
- Shader 反射驱动的自动绑定系统；
- 蒙皮和动态拓扑；
- 镜面 Lumen Reflections；
- 多次间接反弹；
- 自适应 Card 放置、Atlas 驱逐和资源流送；
- 完整材质图和复杂透明材质。

## 7. 推荐执行顺序

严格按以下顺序推进：

1. 持续帧循环、Resize、FrameContext、Fence；
2. Buffer/Texture、Descriptor、Upload、Barrier；
3. DXC、Compute PSO、调试输出；
4. Cornell Box、Camera、Depth、GBuffer；
5. HZB 和 Screen Trace；
6. BLAS/TLAS 和 Inline RayQuery；
7. 固定 Surface Cache；
8. Screen Probe 和一次漫反射 GI；
9. 空间滤波、上采样、历史累积；
10. 完成稳定性和性能测量后，再决定是否扩展动态场景。

不要在前四步完成前开始 Surface Cache，也不要在固定场景跑通前设计通用场景系统。

## 8. 第一批具体任务

- [x] 重写 Win32 主循环，使渲染持续运行；
- [ ] 添加 `WM_SIZE` 处理；
- [x] 修正 Fence Event 错误判断；
- [x] 增加双缓冲 `FrameContext`；
- [x] 增加 DSV 和 Depth Buffer；
- [x] 增加 Index Buffer；
- [x] 将静态几何从 Upload Heap 移入 Default Heap；
- [x] 增加每帧 Camera Constant Buffer；
- [ ] 用 DXC 替换 `D3DCompileFromFile`；
- [ ] 增加最小 Compute Shader Dispatch；
- [ ] 加入 DRED 和对象调试名称；
- [x] 建立 Cornell Box 程序化场景。

## 9. 参考资料

- Epic Games：[Lumen Technical Details](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine)
- Microsoft：[DirectX Raytracing Specification](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)
- Microsoft：[Use DRED to diagnose GPU faults](https://learn.microsoft.com/en-us/windows/win32/direct3d12/use-dred)
