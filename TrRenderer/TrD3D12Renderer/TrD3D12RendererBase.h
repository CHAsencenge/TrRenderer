// todo: pure virtual interface 
#pragma once
#include "TrD3D12Util.h"


class TrD3D12RendererBase
{

public:
	TrD3D12RendererBase(UINT width, UINT height, std::wstring title);
	virtual ~TrD3D12RendererBase();


#pragma region pipeline
	virtual void OnInitialize() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnRender() = 0;
	virtual void OnDestroy() = 0;

	virtual void OnKeyDown(UINT8 wParam) = 0;
	virtual void OnKeyUp(UINT8 wParam) = 0;
#pragma endregion

	
public:
	std::wstring GetAssetFullPath(LPCWSTR assetName);
	void ParseCommandLineArgs(WCHAR* argv[], int argc);

	UINT GetWidth() { return mWidth; }
	UINT GetHeight() { return mHeight; }
	const WCHAR* GetTitle() const  { return mTitle.c_str(); }

	UINT mWidth;
	UINT mHeight;
	float mAspectRatio;
	std::wstring mTitle;

private:
	/**note: 
	 * std::wstring : work with unicode or wide character strings
	 * LPCWSTR : windows specific type , stands for long pointer to constant wide string
	 * a pointer to a null-ternimated ('\0') array of wide characters (const wchar_t*)
	*/
	std::wstring mAssetsPath;


};