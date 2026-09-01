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
- `R32G8X24_TYPELESS` Depth-Stencil Buffer、`D32_FLOAT_S8X24_UINT` DSV，以及独立的 Depth/Stencil SRV；
- Default Heap 静态 Vertex/Index Buffer；
- UE 风格六层常量域：Scene、View、Pass、Primitive、Material、Draw/Dispatch；
- 每帧独立、256 字节对齐的 Scene、View、逐 Instance Primitive 和强类型 Pass Constant Buffer；
- Draw/Dispatch 小常量通过 Root Constants 提交；
- 用于运行时场景验证的程序化 Cornell Box：一个六 Primitive 房间 Mesh、共享球/立方体 Mesh 的多实例、嵌套父子节点和多材质；
- 三目标 MRT GBuffer：BaseColor/Roughness、WorldNormal/Metallic 与 Emissive/Occlusion；
- 独立可配置的 Depth/Normal Prepass；默认绘制 Opaque 和 Alpha Mask，写 Device Depth 和世界空间 Shading Normal；
- C++/HLSL 对齐的具名 Material Flag：Unlit、DoubleSided、AlphaMask、AlphaBlend；
- AlphaMask 在 GBuffer Pixel Shader 中按 `AlphaCutoff` 执行覆盖率裁剪；
- 读取 GBuffer/Depth 的全屏 Deferred Lighting Pass；
- Deferred Lighting 后执行的 Forward Transparent Pass：透明 Primitive 不写 GBuffer，CPU 按世界包围盒中心由远到近排序，使用只读 Depth-Stencil 和标准非预乘 Alpha Blend 写入 HDR Lighting；
- `RGBA16_FLOAT` HDR Lighting Target 和显示颜色转换 Composite Pass；
- 可注册任意 SRV 的 `TrGpuDebug` 中间结果查看器，支持 Final、BaseColor、Normal、Roughness、Metallic、Emissive、AO 和线性 Depth；
- Screen Trace 命中可在 Deferred Lighting 前从命中点 GBuffer 解析为逐射线 Radiance，并以余弦加权 Monte Carlo 估计积分为逐 Probe Irradiance；Deferred Lighting 使用深度/法线引导上采样，并统一评估直接光、环境光和接收表面的间接漫反射；Radiance 和 Irradiance 均可在 GPU Visualization 中查看；
- 独立接入 Dear ImGui（不依赖 nvpro_core）；右侧工具面板使用一级分类按钮和二级下拉功能菜单，当前包含 Rendering/Geometry View、Rendering/Display Settings、Scene/Runtime Hierarchy，选中功能显示在功能子窗口；GPU Visualization 作为右侧常驻子窗口，保留所有中间结果按钮和 Screen Trace 状态说明；
- Final Lighting 支持 Hierarchy 调试着色（父节点主色、节点辅色、深度亮度）和 Primitive Draw 调试着色（PrimitiveID + InstanceID），无需新增 ID Render Target；
- Debug 退出时检查 D3D12 InfoQueue 的 ERROR/CORRUPTION 消息。

当前资源创建已经拆成单向依赖的轻量流程：`TrScene` 保存 CPU 场景层级，`TrRuntimeScene` 为每个被引用的源 Mesh 创建一份 GPU 几何，并以 Node 作为 Instance；Mesh 只持有 GPU 几何资源，Upload Context 只负责上传与上传期同步，Graphics Pipeline 只负责 Root Signature、Shader 和 PSO，Renderer 只编排这些步骤。运行时不再把节点变换烘焙到一个展平 Mesh。MeshID、PrimitiveID、InstanceID、MaterialID 都有明确且与绘制顺序无关的契约；Runtime Mesh/Primitive 保存 Local AABB，Instance 保存 Current/Previous World Transform 和 World AABB。CPU 侧支持 `BeginFrame -> SetNodeLocalTransform -> UpdateWorldTransforms` 的层级更新，GPU Scene 暂不接入。

当前主要不足：

- 历史纹理生命周期已具备，但尚未接入实际时间累积、Motion Vector 和 History Rejection；
- 已有多 Mesh、多 Primitive、多 Instance、CPU 动态层级更新和时序 Transform，但尚无可交互相机、动画资产系统和 GPU Scene StructuredBuffer；
- Compute、HZB Build、固定 Screen Probe、HZB Screen Trace、Radiance Resolve、Irradiance Integrate 和首版间接光合成已接入；尚无 RayQuery 补洞、时间积累和完整的去噪流程；
- 没有 DXR 加速结构和 RayQuery；
- 键盘回调当前保留为空，逐帧更新已负责场景层级动画、常量更新和透明排序。

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
    Passes/Raster/      Depth/Normal、GBuffer、Deferred Lighting、Forward Transparent、Composite
    Passes/Compute/     通用 Compute Pass，目前包含 HZB Build
    Lumen/              Screen Probe、Screen Trace、Radiance、Irradiance 及其专用资源
    Passes/RayTracing/  Inline RayQuery 等后续光追 Pass
    Resources/          Buffer、Texture、Mesh、Descriptor、Render Target、Material
    Backend/            Graphics/Compute PSO、DXC、Upload、Barrier
    Scene/              TrRuntimeScene 与程序化场景
    Debug/              GPU 中间结果和 ImGui 面板
    Utilities/          通用错误处理与图像解码
    Legacy/             已确认不参与编译的旧实验代码
  shaders/
    Raster/             当前光栅与全屏 Pass Shader
    Compute/            通用 Compute Shader，目前包含 HZB Build
    Lumen/              Screen Probe 布置、HZB Screen Trace、Radiance 与 Irradiance Shader
    RayTracing/         后续 Inline RayQuery Shader
    Legacy/             已确认未使用的旧 Shader
  ThirdParty/           d3dx12.h；Dear ImGui 仍从 Includes 独立引用
```

磁盘目录、CMake `source_group(TREE ...)` 和 Visual Studio Filter 保持一致；不再手工维护生成的 `.vcxproj`。Visual Studio 解决方案将可执行程序放在 `Applications`，`TrSceneCore` 放在 `Libraries`，导入器放在 `Tools`。D3D12 使用独立的 `Source/App/Main.cpp`，不再通过 `Common/TrRenderer.cpp` 的条件宏提供入口。

HZB Build、Screen Probe 布置和首版 Screen Trace 已接入 Renderer。当前固定每 `16x16` 个屏幕像素布置一个 Probe，每个 Probe 以 `4x4` 图集块保存 16 条确定性余弦半球射线。Probe 默认选择 Tile 中心的可见表面；中心为背景时，在 Tile 内选择离中心最近的有效 Depth/Normal 样本。这样布局稳定、成本固定，同时能覆盖轮廓附近的部分背景 Tile。

Screen Trace 从较粗 HZB Mip 开始。射线采样点位于场景深度之前时，按当前 Mip 对应的步长前进并逐步提升 Mip；检测到潜在穿越后逐级下降，在 Mip0 上做二分细化和 View Depth 厚度测试。正式命中图集使用 `R32G32B32A32_UINT`，每条射线保存 16 字节 `TrScreenTraceHit`：打包后的精确命中像素、未归一化世界空间 `HitT`、状态/来源/最后 Mip/迭代次数 Flag，以及厚度和屏幕边缘共同评估的置信度。未命中像素统一为 `0xffffffff`，状态区分无效 Probe、屏幕命中、超过距离、离开屏幕和迭代耗尽。原 `RGBA16_FLOAT` 图集已降为独立的 GPU 调试输出，只编码 Hit UV、归一化距离和状态。正式命中数据已连接屏幕 Radiance 采样和 Probe Irradiance 积分，但尚无 RayQuery fallback。

Radiance 和 Irradiance 保持为两个轻量 Compute Pass，不直接合并。Radiance Resolve 在 Deferred Lighting 之前根据精确命中像素读取 GBuffer，使用与 Deferred Lighting 共享的光照函数评估命中表面的直接光和自发光，输出与 Trace Atlas 同尺寸的 `RGBA16_FLOAT` 逐射线 Radiance，其中 Alpha 保存命中置信度。它刻意不采样 Probe GI，避免同帧递归反馈。该资源每帧完全覆盖，不作为历史保存；保留它是为了给后续 RayQuery fallback 提供统一的逐射线汇合点，并支持逐射线调试。Irradiance Integrate 随后按 Probe 的 `4x4` 射线块积分，以 `PI / RayCount * sum(Li * Confidence)` 估计余弦半球漫反射辐照度，并在 Alpha 中保存有效覆盖率。输出是每 Probe 一个 `RGBA16_FLOAT` Texel。

最终 Deferred Lighting 同时完成直接光和间接光材质评估。它对每个全分辨率不透明像素读取邻近 `2x2` Probe，以屏幕双线性权重、世界法线相似度、线性深度差和 Probe 覆盖率联合过滤，降低跨轮廓和跨表面漏光；随后计算 `BaseColor * (1 - Metallic) * Irradiance / PI * AO` 并与直接光、环境光和 Emissive 一次写入 HDR。当前仍没有 RayQuery fallback、时间积累、更宽范围空间滤波和 History Rejection；常量 Ambient 属于过渡照明，正式调节 GI 时应降低或关闭，避免掩盖真实间接光。

HZB 使用“最近深度金字塔”：Mip0 从场景 Device Depth 构建，Mip N 从 Mip N-1 保守归约。Reversed-Z 中较大的值离相机更近，因此使用 `max`；Forward-Z 使用 `min`。保留非线性的 Device Depth 可以直接配合硬件深度约定，也避免每级重复线性化。每个低分辨率 Texel 表示其覆盖区域内最近的潜在遮挡面，后续 Screen Trace 可以先在粗 Mip 快速跳过空区域，再逐级下降确认命中。

当前 HZB Build 的具体实现：

- `R32_FLOAT`、完整 Mip 链、`ALLOW_UNORDERED_ACCESS`；
- 一份全链 SRV，以及每级独立 SRV/UAV；描述符槽预留 15 级并在 Resize 时原位重建，避免线性 Descriptor Heap 泄漏；
- Mip0 读取 Depth SRV，后续每级读取前一级 SRV并写当前级 UAV；
- 每级使用一个 `8x8` Compute Dispatch，并做线程越界判断；
- 非二次幂尺寸按源/目标尺寸比例计算覆盖区间，而不是固定读取 2x2，保证奇数边长末端 Texel 不丢失；
- 每次 Dispatch 后执行 UAV Barrier，再将目标 Mip 从 UAV 转为 `ALL_SHADER_RESOURCE`，供下一级 Compute 和 Pixel Debug 使用；
- Resize 重建纹理和有效 Mip 视图，并重新注册调试项；
- ImGui 面板可查看从 Mip0 到 1x1 的所有层级，Composite 会将任意尺寸 Mip 拉伸到窗口显示。

当前采用“一次 Dispatch 生成一级”的方案，意图是让资源状态和归约语义保持直观。暂不使用 Group Shared Memory 一次生成多级、Wave Intrinsic、SPD 或异步 Compute；等 Screen Trace 接入并测得 HZB Build 成为瓶颈后再优化。

深度约定由 CMake 开关 `TR_USE_REVERSED_Z` 统一控制，默认开启。C++ 侧据此选择投影矩阵、Depth Clear 和 PSO 比较函数；DXC 通过通用 Shader Define 接口收到同值的 `TR_REVERSED_Z=0/1`。当前深度可视化和背景判断均已兼容两种模式，后续 HZB 与 Screen Trace Shader 继续复用该 Define。

Depth/Normal 已拆为独立 Prepass：它先写硬件 Device Depth 和应用法线贴图后的世界空间 Shading Normal。后续 GBuffer 复用相同目标；已进入 Prepass 的 Draw 使用 `EQUAL + DepthWriteMask=ZERO`，未进入 Prepass 的 Draw 继续使用当前 Forward/Reversed-Z 比较函数并写深度。GBuffer 结束后，`TrDepthNormalView` 将最终 Depth/Normal 及其 SRV 组合成非持有型输出契约，并验证尺寸、Mip、描述符和 Compute 可读状态，HZB 与 Screen Trace 直接消费该契约。

PC 渲染的 Prepass 材质策略保持显式且可裁剪：

| 材质类型 | 默认进入 Prepass | 原因 |
| --- | --- | --- |
| Opaque（包括 Unlit、双面不透明） | 是 | 不依赖像素透明度，深度稳定，最适合 Early-Z |
| Alpha Mask / Alpha Test | 是 | Prepass 和 GBuffer 使用相同 AlphaCutoff；会重复采样 Alpha，但能覆盖植被等高 Overdraw 场景 |
| Alpha Blend / Transparent | 否 | 不应写最近表面深度，走 Forward Transparent |
| 会改变深度或轮廓的材质 | 否，除非两 Pass 完全复现 | Pixel Depth Offset、位移或不一致的顶点动画会破坏 GBuffer `EQUAL` |

CMake 缓存项 `TR_PREPASS_MODE` 提供三档：`Disabled`、`OpaqueOnly`、`OpaqueAndMasked`（默认）。例如 `cmake -S . -B build -DTR_PREPASS_MODE=OpaqueOnly` 可只预绘制不透明材质。Mask 模式在 Depth/Normal Shader 中复用 BaseColor UV Transform、纹理 Alpha 和 `AlphaCutoff`，确保与 GBuffer 的覆盖率判定一致；Blend 永远不会进入 Prepass。

Depth 资源现使用 `R32G8X24_TYPELESS`，DSV 为 `D32_FLOAT_S8X24_UINT`，在保留 Reversed-Z 所需 32 位浮点深度精度的同时提供 8 位 Stencil。Depth 和 Stencil 分别通过 `R32_FLOAT_X8X24_TYPELESS`、`X32_TYPELESS_G8X24_UINT` SRV 暴露；每帧开始同时清理 Depth 与 Stencil，当前 Stencil 保留为后续分类用途，尚未定义写入位布局。

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
Tr*PassConstants            b2  GBuffer、DeferredLighting、ForwardTransparent、Composite 各自的强类型参数
TrPrimitiveConstants        b3  World、PreviousWorld、法线矩阵、包围球、InstanceId、MeshId
TrMaterialConstants         b4  BaseColorFactor、Roughness、Metallic、EmissiveStrength
TrDrawConstants             b5  PrimitiveId、MaterialId、LocalPrimitiveIndex、Draw Flags
```

`b2` Pass Constants 是逻辑层，不使用一个通用的大结构。每个 Pass 定义自己的强类型结构，当前包括 `TrGBufferPassConstants`、`TrDeferredLightingPassConstants`、`TrForwardTransparentPassConstants` 和 `TrCompositePassConstants`。Scene、View 和 Pass CBV 按 `TrFrameContext` 分配；Primitive CBV 按 Frame × Instance 分配；静态 Material CBV 由材质资源持有。CPU 只更新当前可安全复用的帧资源。Draw/Dispatch 数据只有 16 字节，使用映射到 `b5` 的 Root Constants，避免为每次 Draw 额外分配 256 字节 CBV。以后对象、材质或光源数量增大时，再把对应数组迁移到 StructuredBuffer/GPU Scene 风格索引访问。

延迟渲染迭代 1 已完成：Cornell Box 先渲染到带 RTV/SRV 的离屏纹理，再以 `COPY_SOURCE` 复制到 `COPY_DEST` 状态的 SwapChain Back Buffer。当前逐帧状态链为：

```text
Offscreen: PIXEL_SHADER_RESOURCE -> RENDER_TARGET -> COPY_SOURCE -> PIXEL_SHADER_RESOURCE
BackBuffer: PRESENT -> COPY_DEST -> PRESENT
```

延迟渲染迭代 2 已完成：几何 Pass 同时写入 BaseColor/Roughness 和 WorldNormal/Metallic，Depth 可同时作为 DSV 与 SRV。后续已经接入最小 Deferred Lighting 和 Composite，当前窗口不再直接复制 GBuffer0。

```text
Normal:     ALL_SHADER_RESOURCE -> RENDER_TARGET (Depth/Normal + GBuffer)
            -> ALL_SHADER_RESOURCE
Depth:      ALL_SHADER_RESOURCE -> DEPTH_WRITE (Depth/Normal + GBuffer)
            -> ALL_SHADER_RESOURCE -> DEPTH_READ (Forward Transparent)
            -> ALL_SHADER_RESOURCE
HDR:        PIXEL_SHADER_RESOURCE -> RENDER_TARGET (Deferred Lighting)
            -> PIXEL_SHADER_RESOURCE -> RENDER_TARGET (Forward Transparent)
            -> PIXEL_SHADER_RESOURCE
BackBuffer: PRESENT -> RENDER_TARGET -> PRESENT
```

材质阶段由 `TrSceneAlphaMode` 明确分类：Opaque 和 Mask 进入 GBuffer，Blend 只进入 Forward Transparent。GBuffer Shader 对 Blend Flag 额外执行防御性丢弃；透明 Shader 只接受 AlphaBlend Flag。当前透明方案是轻量的传统有序混合，排序粒度为 Primitive，不处理相交透明几何、每三角形排序或 OIT。

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
  Source/Passes/
    Raster/             GBuffer、Lighting、Transparent、Composite
    Compute/            HZB 等通用 Compute Pass
  Source/Lumen/
    TrScreenProbeResources.*
    TrScreenProbePass.*
    TrScreenTracePass.*
    TrRayQueryPass.*    后续
    TrSurfaceCachePass.* 后续
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
  - `D32_FLOAT_S8X24_UINT`：Depth + Stencil；
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
- [x] 通过 HZB Build 验证 Compute SRV 读取、逐 Mip UAV 写入、Dispatch 和资源屏障；
- [x] 以固定 `16x16` Tile 布置 Screen Probe，并输出世界位置、法线和有效性资源；
- [x] 每 Probe 生成 16 条确定性余弦半球射线，完成 HZB 粗到细 Screen Trace、Mip0 二分细化、正式命中载荷和独立命中状态可视化；
- [x] 将 Screen Trace 命中解析为逐射线 Radiance，并积分为逐 Probe 漫反射 Irradiance；Radiance 仅作为本帧工作集和调试结果；
- [x] 将 Radiance Resolve 和 Irradiance Integrate 放在 Deferred Lighting 前；Deferred Lighting 使用深度/法线/覆盖率引导的 `2x2` Probe 上采样，统一评估直接光和接收表面间接光；
- [ ] 加入 DRED 和对象调试名称；
- [x] 建立 Cornell Box 程序化场景。
- [x] 建立可扩展的 GPU 中间结果调试视图。
- [x] 建立保留 Mesh、Primitive、Node 层级和实例变换的 `TrRuntimeScene`，运行时不再展平场景。
- [x] 补齐稳定 Mesh/Primitive/Instance/Material ID、Mesh/Primitive/Instance AABB 和 Current/Previous Transform。
- [x] 建立 CPU 层级变换传播，并用共享 Mesh、多实例、多 Primitive、多材质的程序化场景验证。
- [x] 建立具名 Material Flag、Alpha Test 和按 Primitive 排序的 Forward Transparent Pass，并用程序化透明面板验证混合与深度遮挡。
- [x] 建立可配置 Depth/Normal Prepass；Opaque 和 Mask 默认进入，Blend 排除；GBuffer 对已预写深度使用 `EQUAL` 且关闭深度写入。

## 9. 参考资料

- Epic Games：[Lumen Technical Details](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine)
- Microsoft：[DirectX Raytracing Specification](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)
- Microsoft：[Use DRED to diagnose GPU faults](https://learn.microsoft.com/en-us/windows/win32/direct3d12/use-dred)
