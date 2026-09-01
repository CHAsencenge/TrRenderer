#pragma once

#include "Utilities/TrUtil.h"

#include <dxcapi.h>
#include <string>
#include <vector>

struct TrShaderDefine
{
    std::wstring Name;
    std::wstring Value = L"1";
};

class TrShaderCompiler
{
public:
    static Microsoft::WRL::ComPtr<IDxcBlob> Compile(
        const std::wstring& filename,
        const wchar_t* entryPoint,
        const wchar_t* targetProfile,
        const std::vector<TrShaderDefine>& defines = {});
};
