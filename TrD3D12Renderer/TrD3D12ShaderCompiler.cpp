#include "TrD3D12ShaderCompiler.h"

#include <filesystem>
#include <stdexcept>
#include <vector>

Microsoft::WRL::ComPtr<IDxcBlob> TrD3D12ShaderCompiler::Compile(
    const std::wstring& filename,
    const wchar_t* entryPoint,
    const wchar_t* targetProfile)
{
    if(filename.empty() || entryPoint == nullptr || entryPoint[0] == L'\0' ||
       targetProfile == nullptr || targetProfile[0] == L'\0')
    {
        throw std::invalid_argument("DXC shader description is incomplete.");
    }

    Microsoft::WRL::ComPtr<IDxcUtils> utils;
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));
    ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;
    ThrowIfFailed(utils->LoadFile(filename.c_str(), nullptr, &source));

    const DxcBuffer sourceBuffer =
    {
        source->GetBufferPointer(),
        source->GetBufferSize(),
        DXC_CP_ACP
    };

    const std::wstring includeDirectory =
        std::filesystem::path(filename).parent_path().wstring();
    std::vector<LPCWSTR> arguments =
    {
        filename.c_str(),
        L"-E", entryPoint,
        L"-T", targetProfile,
        L"-HV", L"2021",
        L"-Ges",
        L"-I", includeDirectory.c_str()
    };

#if defined(_DEBUG)
    arguments.push_back(L"-Zi");
    arguments.push_back(L"-Qembed_debug");
    arguments.push_back(L"-Od");
#else
    arguments.push_back(L"-O3");
#endif

    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
    ThrowIfFailed(utils->CreateDefaultIncludeHandler(&includeHandler));

    Microsoft::WRL::ComPtr<IDxcResult> result;
    ThrowIfFailed(compiler->Compile(
        &sourceBuffer,
        arguments.data(),
        static_cast<UINT32>(arguments.size()),
        includeHandler.Get(),
        IID_PPV_ARGS(&result)));

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> diagnostics;
    if(SUCCEEDED(result->GetOutput(
           DXC_OUT_ERRORS,
           IID_PPV_ARGS(&diagnostics),
           nullptr)) &&
       diagnostics != nullptr && diagnostics->GetStringLength() > 0)
    {
        OutputDebugStringA(diagnostics->GetStringPointer());
    }

    HRESULT compilationStatus = E_FAIL;
    ThrowIfFailed(result->GetStatus(&compilationStatus));
    if(FAILED(compilationStatus))
    {
        throw DxException(
            compilationStatus,
            L"DXC shader compilation",
            filename,
            0);
    }

    Microsoft::WRL::ComPtr<IDxcBlob> shader;
    ThrowIfFailed(result->GetOutput(
        DXC_OUT_OBJECT,
        IID_PPV_ARGS(&shader),
        nullptr));
    if(shader == nullptr || shader->GetBufferSize() == 0)
    {
        throw std::runtime_error("DXC produced no shader object.");
    }
    return shader;
}
