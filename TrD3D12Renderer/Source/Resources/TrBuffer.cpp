#include "TrBuffer.h"

#include "Backend/TrResourceBarrier.h"
#include "Backend/TrUploadContext.h"

#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    constexpr UINT64 ConstantBufferAlignment =
        D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    constexpr UINT64 MaxConstantBufferSize =
        D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16ull;

    UINT64 AlignUp(UINT64 value, UINT64 alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    UINT CheckedUint(UINT64 value, const char* message)
    {
        if(value > std::numeric_limits<UINT>::max())
        {
            throw std::overflow_error(message);
        }
        return static_cast<UINT>(value);
    }

    UINT ResolveElementCount(
        UINT64 bufferSize,
        const TrBufferViewDesc& viewDesc)
    {
        UINT64 elementSize = 0;
        switch(viewDesc.Type)
        {
        case TrBufferViewType::Structured:
            if(viewDesc.ElementStride == 0 || viewDesc.Format != DXGI_FORMAT_UNKNOWN)
            {
                throw std::invalid_argument(
                    "A structured buffer view requires a stride and an unknown format.");
            }
            elementSize = viewDesc.ElementStride;
            break;

        case TrBufferViewType::Raw:
            if(viewDesc.ElementStride != 0 || viewDesc.Format != DXGI_FORMAT_UNKNOWN)
            {
                throw std::invalid_argument(
                    "A raw buffer view does not use a stride or typed format.");
            }
            elementSize = sizeof(UINT32);
            break;

        case TrBufferViewType::Typed:
            if(viewDesc.ElementStride != 0 || viewDesc.Format == DXGI_FORMAT_UNKNOWN ||
               viewDesc.ElementCount == 0)
            {
                throw std::invalid_argument(
                    "A typed buffer view requires a format and explicit element count.");
            }
            return viewDesc.ElementCount;

        default:
            throw std::invalid_argument("Unknown buffer view type.");
        }

        if(viewDesc.FirstElement > bufferSize / elementSize)
        {
            throw std::out_of_range("Buffer view starts outside the resource.");
        }

        const UINT64 availableElementCount =
            bufferSize / elementSize - viewDesc.FirstElement;
        const UINT64 elementCount = viewDesc.ElementCount == 0
            ? availableElementCount
            : viewDesc.ElementCount;
        if(elementCount == 0 || elementCount > availableElementCount)
        {
            throw std::out_of_range("Buffer view exceeds the resource.");
        }
        return CheckedUint(elementCount, "Buffer view element count exceeds UINT range.");
    }
}

TrBuffer::~TrBuffer()
{
    Reset();
}

void TrBuffer::Initialize(ID3D12Device* device, const TrBufferDesc& desc)
{
    if(device == nullptr || desc.SizeInBytes == 0)
    {
        throw std::invalid_argument("Buffer requires a device and non-zero size.");
    }
    if(desc.HeapType != D3D12_HEAP_TYPE_DEFAULT &&
       desc.HeapType != D3D12_HEAP_TYPE_UPLOAD &&
       desc.HeapType != D3D12_HEAP_TYPE_READBACK)
    {
        throw std::invalid_argument("Buffer heap must be default, upload, or readback.");
    }
    if(desc.HeapType == D3D12_HEAP_TYPE_UPLOAD &&
       desc.InitialState != D3D12_RESOURCE_STATE_GENERIC_READ)
    {
        throw std::invalid_argument("Upload buffers must remain in GENERIC_READ state.");
    }
    if(desc.HeapType == D3D12_HEAP_TYPE_READBACK &&
       desc.InitialState != D3D12_RESOURCE_STATE_COPY_DEST)
    {
        throw std::invalid_argument("Readback buffers must remain in COPY_DEST state.");
    }
    if(desc.HeapType != D3D12_HEAP_TYPE_DEFAULT &&
       desc.Flags != D3D12_RESOURCE_FLAG_NONE)
    {
        throw std::invalid_argument("Upload and readback buffers do not support resource flags.");
    }

    Reset();
    const CD3DX12_HEAP_PROPERTIES heapProperties(desc.HeapType);
    const CD3DX12_RESOURCE_DESC resourceDesc =
        CD3DX12_RESOURCE_DESC::Buffer(desc.SizeInBytes, desc.Flags);
    ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        desc.InitialState,
        nullptr,
        IID_PPV_ARGS(&mResource)));

    mSizeInBytes = desc.SizeInBytes;
    mHeapType = desc.HeapType;
    mFlags = desc.Flags;
    mState = desc.InitialState;
    if(desc.DebugName != nullptr && desc.DebugName[0] != L'\0')
    {
        ThrowIfFailed(mResource->SetName(desc.DebugName));
    }

    if(mHeapType == D3D12_HEAP_TYPE_UPLOAD)
    {
        const D3D12_RANGE noCpuReads = {0, 0};
        Map(&noCpuReads);
    }
}

void TrBuffer::InitializeStatic(
    TrUploadContext& uploadContext,
    const void* sourceData,
    UINT64 byteSize,
    D3D12_RESOURCE_STATES finalState,
    D3D12_RESOURCE_FLAGS flags,
    const wchar_t* debugName)
{
    if(sourceData == nullptr || byteSize == 0)
    {
        throw std::invalid_argument("Static buffer requires non-empty source data.");
    }

    Reset();
    uploadContext.UploadStaticBuffer(
        sourceData,
        byteSize,
        finalState,
        mResource,
        flags);
    mSizeInBytes = byteSize;
    mHeapType = D3D12_HEAP_TYPE_DEFAULT;
    mFlags = flags;
    mState = finalState;
    if(debugName != nullptr && debugName[0] != L'\0')
    {
        ThrowIfFailed(mResource->SetName(debugName));
    }
}

void TrBuffer::Reset()
{
    if(mResource != nullptr && mMappedData != nullptr)
    {
        const D3D12_RANGE noCpuWrites = {0, 0};
        mResource->Unmap(
            0,
            mHeapType == D3D12_HEAP_TYPE_READBACK ? &noCpuWrites : nullptr);
    }

    mMappedData = nullptr;
    mResource.Reset();
    mSizeInBytes = 0;
    mHeapType = D3D12_HEAP_TYPE_DEFAULT;
    mFlags = D3D12_RESOURCE_FLAG_NONE;
    mState = D3D12_RESOURCE_STATE_COMMON;
}

void TrBuffer::Update(
    const void* data,
    UINT64 byteSize,
    UINT64 destinationOffset)
{
    ValidateResource();
    if(mHeapType != D3D12_HEAP_TYPE_UPLOAD || data == nullptr || byteSize == 0)
    {
        throw std::invalid_argument("Only upload buffers accept non-empty CPU updates.");
    }
    if(destinationOffset > mSizeInBytes || byteSize > mSizeInBytes - destinationOffset)
    {
        throw std::out_of_range("Buffer update exceeds the resource.");
    }

    if(mMappedData == nullptr)
    {
        const D3D12_RANGE noCpuReads = {0, 0};
        Map(&noCpuReads);
    }
    std::memcpy(mMappedData + destinationOffset, data, static_cast<size_t>(byteSize));
}

void* TrBuffer::Map(const D3D12_RANGE* readRange)
{
    ValidateResource();
    if(mHeapType == D3D12_HEAP_TYPE_DEFAULT)
    {
        throw std::logic_error("Default-heap buffers cannot be CPU mapped.");
    }
    if(mMappedData == nullptr)
    {
        ThrowIfFailed(mResource->Map(
            0,
            readRange,
            reinterpret_cast<void**>(&mMappedData)));
    }
    return mMappedData;
}

void TrBuffer::Unmap(const D3D12_RANGE* writtenRange)
{
    ValidateResource();
    if(mMappedData == nullptr)
    {
        return;
    }

    const D3D12_RANGE noCpuWrites = {0, 0};
    mResource->Unmap(
        0,
        writtenRange != nullptr
            ? writtenRange
            : (mHeapType == D3D12_HEAP_TYPE_READBACK ? &noCpuWrites : nullptr));
    mMappedData = nullptr;
}

D3D12_VERTEX_BUFFER_VIEW TrBuffer::CreateVertexBufferView(
    UINT strideInBytes,
    UINT64 byteOffset,
    UINT64 byteSize) const
{
    ValidateResource();
    if(strideInBytes == 0 || byteOffset >= mSizeInBytes)
    {
        throw std::invalid_argument("Vertex buffer view requires a valid stride and offset.");
    }

    const UINT64 resolvedSize = byteSize == 0 ? mSizeInBytes - byteOffset : byteSize;
    if(resolvedSize == 0 || resolvedSize > mSizeInBytes - byteOffset)
    {
        throw std::out_of_range("Vertex buffer view exceeds the resource.");
    }

    D3D12_VERTEX_BUFFER_VIEW view = {};
    view.BufferLocation = GetGpuVirtualAddress(byteOffset);
    view.SizeInBytes = CheckedUint(
        resolvedSize,
        "Vertex buffer view size exceeds UINT range.");
    view.StrideInBytes = strideInBytes;
    return view;
}

D3D12_INDEX_BUFFER_VIEW TrBuffer::CreateIndexBufferView(
    DXGI_FORMAT format,
    UINT64 byteOffset,
    UINT64 byteSize) const
{
    ValidateResource();
    if(format != DXGI_FORMAT_R16_UINT && format != DXGI_FORMAT_R32_UINT)
    {
        throw std::invalid_argument("Index buffer format must be R16_UINT or R32_UINT.");
    }
    if(byteOffset >= mSizeInBytes)
    {
        throw std::out_of_range("Index buffer view starts outside the resource.");
    }

    const UINT64 resolvedSize = byteSize == 0 ? mSizeInBytes - byteOffset : byteSize;
    const UINT indexStride = format == DXGI_FORMAT_R16_UINT
        ? sizeof(UINT16)
        : sizeof(UINT32);
    if(resolvedSize == 0 || resolvedSize > mSizeInBytes - byteOffset ||
       byteOffset % indexStride != 0 || resolvedSize % indexStride != 0)
    {
        throw std::out_of_range("Index buffer view is misaligned or exceeds the resource.");
    }

    D3D12_INDEX_BUFFER_VIEW view = {};
    view.BufferLocation = GetGpuVirtualAddress(byteOffset);
    view.SizeInBytes = CheckedUint(
        resolvedSize,
        "Index buffer view size exceeds UINT range.");
    view.Format = format;
    return view;
}

void TrBuffer::CreateConstantBufferView(
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE handle,
    UINT64 byteOffset,
    UINT64 byteSize) const
{
    ValidateResource();
    if(device == nullptr || byteOffset >= mSizeInBytes ||
       byteOffset % ConstantBufferAlignment != 0)
    {
        throw std::invalid_argument("Constant buffer view has an invalid device or offset.");
    }

    const UINT64 requestedSize = byteSize == 0 ? mSizeInBytes - byteOffset : byteSize;
    const UINT64 alignedSize = AlignUp(requestedSize, ConstantBufferAlignment);
    if(requestedSize == 0 || alignedSize > mSizeInBytes - byteOffset ||
       alignedSize > MaxConstantBufferSize)
    {
        throw std::out_of_range("Constant buffer view exceeds its resource or 64 KiB limit.");
    }

    D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
    viewDesc.BufferLocation = GetGpuVirtualAddress(byteOffset);
    viewDesc.SizeInBytes = static_cast<UINT>(alignedSize);
    device->CreateConstantBufferView(&viewDesc, handle);
}

void TrBuffer::CreateShaderResourceView(
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE handle,
    const TrBufferViewDesc& viewDesc) const
{
    ValidateResource();
    if(device == nullptr || (mFlags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
    {
        throw std::invalid_argument("Buffer cannot be exposed as an SRV.");
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC nativeDesc = {};
    nativeDesc.Format = viewDesc.Type == TrBufferViewType::Raw
        ? DXGI_FORMAT_R32_TYPELESS
        : viewDesc.Format;
    nativeDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    nativeDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    nativeDesc.Buffer.FirstElement = viewDesc.FirstElement;
    nativeDesc.Buffer.NumElements = ResolveElementCount(mSizeInBytes, viewDesc);
    nativeDesc.Buffer.StructureByteStride =
        viewDesc.Type == TrBufferViewType::Structured ? viewDesc.ElementStride : 0;
    nativeDesc.Buffer.Flags = viewDesc.Type == TrBufferViewType::Raw
        ? D3D12_BUFFER_SRV_FLAG_RAW
        : D3D12_BUFFER_SRV_FLAG_NONE;
    device->CreateShaderResourceView(mResource.Get(), &nativeDesc, handle);
}

void TrBuffer::CreateUnorderedAccessView(
    ID3D12Device* device,
    D3D12_CPU_DESCRIPTOR_HANDLE handle,
    const TrBufferViewDesc& viewDesc,
    ID3D12Resource* counterResource) const
{
    ValidateResource();
    if(device == nullptr || !(mFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
    {
        throw std::invalid_argument("Buffer is not an unordered-access resource.");
    }
    if(counterResource != nullptr && viewDesc.Type != TrBufferViewType::Structured)
    {
        throw std::invalid_argument("Only structured UAVs can use a counter resource.");
    }
    if(counterResource == nullptr && viewDesc.CounterOffsetInBytes != 0)
    {
        throw std::invalid_argument("A UAV counter offset requires a counter resource.");
    }
    if(counterResource != nullptr &&
       viewDesc.CounterOffsetInBytes % D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT != 0)
    {
        throw std::invalid_argument("UAV counter offset must be 4 KiB aligned.");
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC nativeDesc = {};
    nativeDesc.Format = viewDesc.Type == TrBufferViewType::Raw
        ? DXGI_FORMAT_R32_TYPELESS
        : viewDesc.Format;
    nativeDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    nativeDesc.Buffer.FirstElement = viewDesc.FirstElement;
    nativeDesc.Buffer.NumElements = ResolveElementCount(mSizeInBytes, viewDesc);
    nativeDesc.Buffer.StructureByteStride =
        viewDesc.Type == TrBufferViewType::Structured ? viewDesc.ElementStride : 0;
    nativeDesc.Buffer.CounterOffsetInBytes = viewDesc.CounterOffsetInBytes;
    nativeDesc.Buffer.Flags = viewDesc.Type == TrBufferViewType::Raw
        ? D3D12_BUFFER_UAV_FLAG_RAW
        : D3D12_BUFFER_UAV_FLAG_NONE;
    device->CreateUnorderedAccessView(
        mResource.Get(),
        counterResource,
        &nativeDesc,
        handle);
}

bool TrBuffer::Transition(
    ID3D12GraphicsCommandList* commandList,
    D3D12_RESOURCE_STATES newState)
{
    ValidateResource();
    if((mHeapType == D3D12_HEAP_TYPE_UPLOAD &&
        newState != D3D12_RESOURCE_STATE_GENERIC_READ) ||
       (mHeapType == D3D12_HEAP_TYPE_READBACK &&
        newState != D3D12_RESOURCE_STATE_COPY_DEST))
    {
        throw std::logic_error("Upload and readback buffer states are fixed.");
    }

    const bool recorded = TrResourceBarrier::Transition(
        commandList,
        mResource.Get(),
        mState,
        newState);
    mState = newState;
    return recorded;
}

void TrBuffer::UavBarrier(ID3D12GraphicsCommandList* commandList) const
{
    ValidateResource();
    if(!(mFlags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
    {
        throw std::logic_error("UAV barrier requires an unordered-access buffer.");
    }
    TrResourceBarrier::Uav(commandList, mResource.Get());
}

D3D12_GPU_VIRTUAL_ADDRESS TrBuffer::GetGpuVirtualAddress(UINT64 byteOffset) const
{
    ValidateResource();
    if(byteOffset >= mSizeInBytes)
    {
        throw std::out_of_range("GPU virtual address offset exceeds the buffer.");
    }
    return mResource->GetGPUVirtualAddress() + byteOffset;
}

void TrBuffer::ValidateResource() const
{
    if(mResource == nullptr || mSizeInBytes == 0)
    {
        throw std::logic_error("Buffer has not been initialized.");
    }
}
