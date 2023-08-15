// todo: pure virtual interface 
#pragma once

#include "TrD3D12Util.h"


class TrD3D12RendererBase
{

public:
	TrD3D12RendererBase(UINT width, UINT height);
	virtual ~TrD3D12RendererBase();


#pragma region pipeline
	virtual void Initialize() = 0;
	virtual void Update() = 0;
	virtual void Render() = 0;
	virtual void Destroy() = 0;
#pragma endregion

	
public:
	std::wstring GetAssetFullPath(LPCWSTR assetName);
	void ParseCommandLineArgs(WCHAR* argv[], int argc);

private:
	/**note: 
	 * std::wstring : work with unicode or wide character strings
	 * LPCWSTR : windows specific type , stands for long pointer to constant wide string
	 * a pointer to a null-ternimated ('\0') array of wide characters (const wchar_t*)
	*/
	std::wstring mAssetsPath;

	UINT mWidth;
	UINT mHeight;
	float mAspectRatio;

};