# DX12 最小 Lumen 实现基础建设规划

更新日期：2026-09-01

## 1. 目标与边界

目标是在当前 DX12 Cornell Box 光栅样例上，逐步实现一个轻量的、用于学习和验证核心思路的 Lumen-like 全局光照原型。

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
- RTV、DSV Descriptor Heap；
- 独立封装的 Graphics/Compute Root Signature 和 PSO；
- DXC HLSL 编译、Shader Model 6.5 VS/PS/CS 和运行时能力检查；
- Default Heap Vertex/Index Buffer；
- 独立的一次性 Upload Context；
- 记录每级 Mip 状态的 Texture2D 封装，以及可指定 Mip 范围的 RTV、DSV、SRV、UAV View 创建；
- 固定容量、线性分配的 RTV、DSV、Shader-visible CBV/SRV/UAV Descriptor Heap；
- 可跳过无效重复转换的 Transition 和 UAV Barrier 辅助；
- 可复用描述符的双纹理 `TrHistoryTexture`，支持 Current/Previous、Advance 和 Invalidate；
- `WM_SIZE` 驱动的 SwapChain、GBuffer、Depth、HDR 与历史纹理重建；
- 离屏 Render Target 渲染并复制到 SwapChain 的可视化验证路径；
- Command List；
- Resource Barrier；
- 双缓冲 TrFrameContext 和按帧 Fence/Event 同步；
- 持续 Update/Render 主循环；
- `R32_TYPELESS` Depth Buffer，以及 `D32_FLOAT` DSV、`R32_FLOAT` SRV；
- Default Heap 静态 Vertex/Index Buffer；
- UE 风格六层常量域：Scene、View、Pass、Primitive、Material、Draw/Dispatch；
- 每帧独立、256 字节对齐的 Scene、View、逐 Instance Primitive 和强类型 Pass Constant Buffer；
- Draw/Dispatch 小常量通过 Root Constants 提交；
- 用于运行时场景验证的程序化 Cornell Box：一个六 Primitive 房间 Mesh、共享球/立方体 Mesh 的多实例、嵌套父子节点和多材质；
- 三目标 MRT GBuffer：BaseColor/Roughness、WorldNormal/Metallic 与 Emissive/Occlusion；
- 读取 GBuffer/Depth 的全屏 Deferred Lighting Pass；
- `RGBA16_FLOAT` HDR Lighting Target 和显示颜色转换 Composite Pass；
- 可注册任意 SRV 的 `TrGpuDebug` 中间结果查看器，支持 Final、BaseColor、Normal、Roughness、Metallic、Emissive、AO 和线性 Depth；
- 独立接入 Dear ImGui（不依赖 nvpro_core），GPU 调试面板通过按钮选择视图，并提供 Exposure、Depth Range 文本输入；面板同时显示 Runtime Scene 节点树和每个节点的 Instance/Mesh/Primitive/Material ID；
- Final Lighting 支持 Hierarchy 调试着色（父节点主色、节点辅色、深度亮度）和 Primitive Draw 调试着色（PrimitiveID + InstanceID），无需新增 ID Render Target；
- Debug 退出时检查 D3D12 InfoQueue 的 ERROR/CORRUPTION 消息。

当前资源创建已经拆成单向依赖的轻量流程：`TrScene` 保存 CPU 场景层级，`TrRuntimeScene` 为每个被引用的源 Mesh 创建一份 GPU 几何，并以 Node 作为 Instance；Mesh 只持有 GPU 几何资源，Upload Context 只负责上传与上传期同步，Graphics Pipeline 只负责 Root Signature、Shader 和 PSO，Renderer 只编排这些步骤。运行时不再把节点变换烘焙到一个展平 Mesh。MeshID、PrimitiveID、InstanceID、MaterialID 都有明确且与绘制顺序无关的契约；Runtime Mesh/Primitive 保存 Local AABB，Instance 保存 Current/Previous World Transform 和 World AABB。CPU 侧支持 `BeginFrame -> SetNodeLocalTransform -> UpdateWorldTransforms` 的层级更新，GPU Scene 暂不接入。

当前主要不足：

- 历史纹理生命周期已具备，但尚未接入实际时间累积、Motion Vector 和 History Rejection；
- 已有多 Mesh、多 Primitive、多 Instance、CPU 动态层级更新和时序 Transform，但尚无可交互相机、动画资产系统和 GPU Scene StructuredBuffer；
- Compute 基础已接入，尚未实现 HZB 和屏幕空间追踪；
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

## 4. 最小代码结构

当前已经落地的资源和管线组件：

命名约定：`TrD3D12Renderer` 只保留为后端目录和构建目标名，用于区分 D3D12、Vulkan 等平台模块；模块内自定义 C++ 类型不再重复携带 `D3D12`。主编排类由 `TrD3D12RendererRaster` 改为 `TrDeferredRenderer`，因为它已经负责 GBuffer、Deferred Lighting、Composite，并将继续接入 Compute 和 RayQuery；Raster 只是其中一个执行阶段，不足以描述整条管线。

```text
TrD3D12Renderer/
  Source/
    App/                Win32 入口、窗口和 Renderer 生命周期接口
    Renderer/           TrDeferredRenderer 与六层常量契约
    Passes/Raster/      GBuffer、Deferred Lighting、Composite
    Passes/Compute/     HZB、Screen Trace 等后续 Compute Pass
    Passes/RayTracing/  Inline RayQuery 等后续光追 Pass
    Resources/          Buffer、Texture、Mesh、Descriptor、Render Target、Material
    Backend/            Graphics/Compute PSO、DXC、Upload、Barrier
    Scene/              TrRuntimeScene 与程序化场景
    Debug/              GPU 中间结果和 ImGui 面板
    Utilities/          通用错误处理与图像解码
    Legacy/             已确认不参与编译的旧实验代码
  shaders/
    Raster/             当前光栅与全屏 Pass Shader
    Compute/            后续 HZB、Screen Trace Shader
    RayTracing/         后续 Inline RayQuery Shader
    Legacy/             已确认未使用的旧 Shader
  ThirdParty/           d3dx12.h；Dear ImGui 仍从 Includes 独立引用
```

磁盘目录、CMake `source_group(TREE ...)` 和 Visual Studio Filter 保持一致；不再手工维护生成的 `.vcxproj`。Visual Studio 解决方案将可执行程序放在 `Applications`，`TrSceneCore` 放在 `Libraries`，导入器放在 `Tools`。D3D12 使用独立的 `Source/App/Main.cpp`，不再通过 `Common/TrRenderer.cpp` 的条件宏提供入口。

初始化依赖保持单向，不让场景生成代码接触 DX12 对象：

```text
TrCornellBoxScene / TrSceneImporter -> TrScene Mesh + Node hierarchy
                                           |
TrUploadContext -> TrRuntimeScene -> TrMesh -> per-mesh GPU Vertex/Index Buffer
TrGraphicsPipeline -----------> Root Signature + PSO
TrBuffer -------> TrConstantBuffer -> 每个 TrFrameContext 一份
                                  |
TrDeferredRenderer --------------+-> 逐帧 Bind / Draw / Present
```

常量数据按 UE 风格的逻辑生命周期拆成六层，所有 Shader 使用同一寄存器约定：

```text
TrSceneConstants            b0  方向光、环境光等场景公共数据
TrViewConstants             b1  当前/上一帧矩阵、相机、渲染尺寸、Jitter、帧号
Tr*PassConstants            b2  GBuffer、DeferredLighting、Composite 各自的强类型参数
TrPrimitiveConstants        b3  World、PreviousWorld、法线矩阵、包围球、InstanceId、MeshId
TrMaterialConstants         b4  BaseColorFactor、Roughness、Metallic、EmissiveStrength
TrDrawConstants             b5  PrimitiveId、MaterialId、LocalPrimitiveIndex、Draw Flags
```

`b2` Pass Constants 是逻辑层，不使用一个通用的大结构。每个 Pass 定义自己的强类型结构，当前包括 `TrGBufferPassConstants`、`TrDeferredLightingPassConstants` 和 `TrCompositePassConstants`。Scene、View 和 Pass CBV 按 `TrFrameContext` 分配；Primitive CBV 按 Frame × Instance 分配；静态 Material CBV 由材质资源持有。CPU 只更新当前可安全复用的帧资源。Draw/Dispatch 数据只有 16 字节，使用映射到 `b5` 的 Root Constants，避免为每次 Draw 额外分配 256 字节 CBV。以后对象、材质或光源数量增大时，再把对应数组迁移到 StructuredBuffer/GPU Scene 风格索引访问。

延迟渲染迭代 1 已完成：Cornell Box 先渲染到带 RTV/SRV 的离屏纹理，再以 `COPY_SOURCE` 复制到 `COPY_DEST` 状态的 SwapChain Back Buffer。当前逐帧状态链为：

```text
Offscreen: PIXEL_SHADER_RESOURCE -> RENDER_TARGET -> COPY_SOURCE -> PIXEL_SHADER_RESOURCE
BackBuffer: PRESENT -> COPY_DEST -> PRESENT
```

延迟渲染迭代 2 已完成：几何 Pass 同时写入 BaseColor/Roughness 和 WorldNormal/Metallic，Depth 可同时作为 DSV 与 SRV。后续已经接入最小 Deferred Lighting 和 Composite，当前窗口不再直接复制 GBuffer0。

```text
GBuffer0/1: PIXEL_SHADER_RESOURCE -> RENDER_TARGET -> PIXEL_SHADER_RESOURCE
Depth:      PIXEL_SHADER_RESOURCE -> DEPTH_WRITE -> PIXEL_SHADER_RESOURCE
HDR:        PIXEL_SHADER_RESOURCE -> RENDER_TARGET -> PIXEL_SHADER_RESOURCE
BackBuffer: PRESENT -> RENDER_TARGET -> PRESENT
```

后续只在需求出现时继续拆分，不建立复杂继承体系：

```text
TrD3D12Renderer/
  TrDeviceContext.*    Device、Queue、SwapChain、Fence
  TrFrameContext.*     每帧 CommandAllocator、FenceValue、上传偏移
  TrBuffer.*           通用 Buffer、各种 View 和资源状态
  TrTexture.*          Texture2D、各种 View 和资源状态
  TrDescriptorHeap.*   RTV、DSV、CBV/SRV/UAV 的简单线性分配
  TrUploadContext.*    静态资源上传（已完成 Buffer 路径）
  TrScene.*            Camera、Mesh、Material、Instance
  TrDeferredRenderer.* 帧流程和 Pass 调度
  Passes/
    TrGBufferPass.*
    TrDeferredLightingPass.*
    TrHzbPass.*
    TrScreenTracePass.*
    TrRayQueryPass.*
    TrSurfaceCachePass.*
    TrScreenProbePass.*
    TrCompositePass.*
```

Pass 初期可以只是普通函数或小类。不要建立通用节点系统、反射注册系统或插件框架。

## 5. 实施阶段

### 阶段 0：稳定的逐帧运行骨架

任务：

- 将 `Update/Render` 从单纯的 `WM_PAINT` 回调移到空闲消息循环；
- 处理 `WM_SIZE`，重建 Back Buffer 和 RTV；
- 建立两个 `TrFrameContext`，对应双缓冲 SwapChain；
- 每个 TrFrameContext 保存 CommandAllocator 和 FenceValue；
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

- 实现轻量 `TrBuffer` 和 `TrTexture`（已完成）；
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
- GBuffer 各通道调试视图（已完成）。

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

1. 持续帧循环、Resize、TrFrameContext、Fence；
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
- [x] 添加 `WM_SIZE` 处理；
- [x] 修正 Fence Event 错误判断；
- [x] 增加双缓冲 `TrFrameContext`；
- [x] 增加 DSV 和 Depth Buffer；
- [x] 增加 Index Buffer；
- [x] 将静态几何从 Upload Heap 移入 Default Heap；
- [x] 增加每帧 Camera Constant Buffer；
- [x] 将常量拆分为 Scene、View、Pass、Primitive、Material、Draw/Dispatch 六个逻辑层；
- [x] 将 Constant Buffer、静态上传、Mesh、Graphics PSO 拆成独立组件；
- [x] 将 Cornell Box CPU 几何生成移出 Renderer；
- [x] 用 DXC 替换 `D3DCompileFromFile`，并检查 Shader Model 6.5 支持；
- [ ] 在首个实际 Compute Pass 中验证 SRV 读取、UAV 写入和 Dispatch；
- [ ] 加入 DRED 和对象调试名称；
- [x] 建立 Cornell Box 程序化场景。
- [x] 建立可扩展的 GPU 中间结果调试视图。
- [x] 建立保留 Mesh、Primitive、Node 层级和实例变换的 `TrRuntimeScene`，运行时不再展平场景。
- [x] 补齐稳定 Mesh/Primitive/Instance/Material ID、Mesh/Primitive/Instance AABB 和 Current/Previous Transform。
- [x] 建立 CPU 层级变换传播，并用共享 Mesh、多实例、多 Primitive、多材质的程序化场景验证。

## 9. 参考资料

- Epic Games：[Lumen Technical Details](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine)
- Microsoft：[DirectX Raytracing Specification](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)
- Microsoft：[Use DRED to diagnose GPU faults](https://learn.microsoft.com/en-us/windows/win32/direct3d12/use-dred)
