#pragma once

#include "Utilities/TrUtil.h"

#include <dxcapi.h>

class TrShaderCompiler
{
public:
    static Microsoft::WRL::ComPtr<IDxcBlob> Compile(
        const std::wstring& filename,
        const wchar_t* entryPoint,
        const wchar_t* targetProfile);
};
