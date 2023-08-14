#pragma once

#include <windows.h>
#include <wrl.h>
#include "minwindef.h"
#include <DirectXMath.h>
#include <string>
#include <d3d12.h>
#include "d3dx12.h"
#include <wrl/client.h>
#include "dxgi1_4.h"
#include "d3d12sdklayers.h"
#include <winerror.h>
#include <exception>



inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    // string:c_str()返回const char*
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    // wstring 使用wchar_t(宽字符)，用于满足非ASCII字符的要求，例如Unicode编码，中文，日文，韩文
    return std::wstring(buffer);
}

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString()const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif