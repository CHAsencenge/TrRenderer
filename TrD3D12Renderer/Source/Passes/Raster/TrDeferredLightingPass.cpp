#include "TrDeferredLightingPass.h"
#include "Renderer/TrRenderConfig.h"

#include <stdexcept>

void TrDeferredLightingPass::Initialize(
    ID3D12Device* device,
    const std::wstring& shaderPath)
{
    CD3DX12_ROOT_PARAMETER rootParameters[6];
    rootParameters[0].InitAsConstantBufferView(
        TrConstantRegister::Scene,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[1].InitAsConstantBufferView(
        TrConstantRegister::View,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[2].InitAsConstantBufferView(
        TrConstantRegister::Pass,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_DESCRIPTOR_RANGE gBufferRange;
    gBufferRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);
    rootParameters[3].InitAsDescriptorTable(
        1,
        &gBufferRange,
        D3D12_SHADER_VISIBILITY_PIXEL);
    CD3DX12_DESCRIPTOR_RANGE probeNormalDepthRange;
    probeNormalDepthRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);
    rootParameters[4].InitAsDescriptorTable(
        1,
        &probeNormalDepthRange,
        D3D12_SHADER_VISIBILITY_PIXEL);
    CD3DX12_DESCRIPTOR_RANGE irradianceRange;
    irradianceRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);
    rootParameters[5].InitAsDescriptorTable(
        1,
        &irradianceRange,
        D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        _countof(rootParameters),
        rootParameters,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    TrGraphicsPipelineDesc pipelineDesc;
    pipelineDesc.ShaderPath = shaderPath;
    pipelineDesc.RootSignatureDesc = &rootSignatureDesc;
    pipelineDesc.RenderTargetFormats[0] =
        TrDeferredRenderTargets::HdrLightingFormat;
    pipelineDesc.RenderTargetCount = 1;
    pipelineDesc.DepthStencilFormat = DXGI_FORMAT_UNKNOWN;
    pipelineDesc.DepthEnabled = false;
    pipelineDesc.CullMode = D3D12_CULL_MODE_NONE;
    pipelineDesc.ShaderDefines = TrRenderConfig::GetDepthShaderDefines();
    mPipeline.Initialize(device, pipelineDesc);
}

void TrDeferredLightingPass::Render(
    ID3D12GraphicsCommandList* commandList,
    TrDeferredRenderTargets& renderTargets,
    TrDescriptorHeap& resourceHeap,
    D3D12_GPU_VIRTUAL_ADDRESS sceneConstants,
    D3D12_GPU_VIRTUAL_ADDRESS viewConstants,
    D3D12_GPU_VIRTUAL_ADDRESS passConstants,
    TrScreenProbeResources& screenProbes,
    TrTexture& probeIrradiance,
    D3D12_GPU_DESCRIPTOR_HANDLE probeIrradianceSrv)
{
    if(commandList == nullptr || resourceHeap.Get() == nullptr ||
       !resourceHeap.IsShaderVisible() || sceneConstants == 0 ||
       viewConstants == 0 || passConstants == 0 ||
       probeIrradianceSrv.ptr == 0)
    {
        throw std::invalid_argument("Deferred lighting pass inputs are invalid.");
    }

    const TrScreenProbeLayout& layout = screenProbes.GetLayout();
    const D3D12_RESOURCE_DESC& hdrDescription =
        renderTargets.GetHdrLighting().GetDescription();
    const D3D12_RESOURCE_DESC& irradianceDescription =
        probeIrradiance.GetDescription();
    if(hdrDescription.Width != layout.RenderWidth ||
       hdrDescription.Height != layout.RenderHeight ||
       irradianceDescription.Width != layout.IrradianceAtlasWidth ||
       irradianceDescription.Height != layout.IrradianceAtlasHeight)
    {
        throw std::logic_error(
            "Deferred lighting Screen Probe resources have incompatible dimensions.");
    }

    screenProbes.GetNormalDepth().Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    probeIrradiance.Transition(
        commandList,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    ID3D12DescriptorHeap* descriptorHeaps[] = {resourceHeap.Get()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetPipelineState(mPipeline.GetPipelineState());
    commandList->SetGraphicsRootSignature(mPipeline.GetRootSignature());
    commandList->SetGraphicsRootConstantBufferView(0, sceneConstants);
    commandList->SetGraphicsRootConstantBufferView(1, viewConstants);
    commandList->SetGraphicsRootConstantBufferView(2, passConstants);
    commandList->SetGraphicsRootDescriptorTable(
        3,
        renderTargets.GetBaseColorSrv().GpuHandle);
    commandList->SetGraphicsRootDescriptorTable(
        4,
        screenProbes.GetNormalDepthSrv().GpuHandle);
    commandList->SetGraphicsRootDescriptorTable(
        5,
        probeIrradianceSrv);

    renderTargets.BeginDeferredLightingPass(commandList);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
    renderTargets.EndDeferredLightingPass(commandList);
}
