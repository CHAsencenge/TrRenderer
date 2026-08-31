#pragma once

#include "TrD3D12Util.h"

#include <dxcapi.h>

class TrD3D12ShaderCompiler
{
public:
    static Microsoft::WRL::ComPtr<IDxcBlob> Compile(
        const std::wstring& filename,
        const wchar_t* entryPoint,
        const wchar_t* targetProfile);
};
