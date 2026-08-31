#pragma once
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <winnt.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <string>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include <exception>
#include <vector>
#include <memory>
#include <comdef.h>
#include <iostream>
#include <stdexcept>
#include <shellapi.h>



inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    // string:c_str()返回const char*
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    // wstring 使用wchar_t(宽字符)，用于满足非ASCII字符的要求，例如Unicode编码，中文，日文，韩文
    return std::wstring(buffer);
}

#ifdef IS_TR_D3D_RENDERER
class TrGraphicsException
{
public:
    TrGraphicsException() = default;
    TrGraphicsException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

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
    if(FAILED(hr__)) { throw TrGraphicsException(hr__, L#x, wfn, __LINE__); } \
}
#endif

inline void GetAssetsPath(_Out_writes_(pathSize) WCHAR* path, UINT pathSize)
// inline void GetAssetsPath(_Out_writes_(pathSize) LPSTR path, UINT pathSize)
{
    if (path == nullptr)
    {
        throw std::exception();
    }

    DWORD size = GetModuleFileName(nullptr, path, pathSize);
    if (size == 0 || size == pathSize)
    {
        throw std::exception();
    }

    WCHAR* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash)
    {
        *(lastSlash + 1) = L'\0';
    }
}
#endif

