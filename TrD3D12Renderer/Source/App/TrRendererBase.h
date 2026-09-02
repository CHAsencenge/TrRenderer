// todo: pure virtual interface 
#pragma once
#include "Utilities/TrUtil.h"

#include <utility>


class TrRendererBase
{

public:
	TrRendererBase(UINT width, UINT height, std::wstring title);
	virtual ~TrRendererBase();


#pragma region pipeline
	virtual void OnInitialize() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnRender() = 0;
	virtual void OnResize(UINT width, UINT height) = 0;
	virtual void OnDestroy() = 0;

	virtual void OnKeyDown(UINT8 wParam) = 0;
	virtual void OnKeyUp(UINT8 wParam) = 0;
	virtual void OnMouseMove(INT x, INT y) = 0;
	virtual void OnRightMouseButtonDown(INT x, INT y) = 0;
	virtual void OnRightMouseButtonUp() = 0;
	virtual void OnInputFocusLost() = 0;
#pragma endregion

	
public:
	std::wstring GetAssetFullPath(LPCWSTR assetName);
	void ParseCommandLineArgs(WCHAR* argv[], int argc);

	UINT GetWidth() { return mWidth; }
	UINT GetHeight() { return mHeight; }
	const WCHAR* GetTitle() const  { return mTitle.c_str(); }
	const std::wstring& GetScenePath() const { return mScenePath; }
	void SetScenePath(std::wstring scenePath) { mScenePath = std::move(scenePath); }

	UINT mWidth;
	UINT mHeight;
	float mAspectRatio;
	std::wstring mTitle;

	// settings
	// Warp: Windows Advanced Rasterization Platform, software-based rasterizer, rendering graphics using the CPU instead of a dedicated GPU
	BOOL mbUseWarpDevice = false;

private:
	/**note: 
	 * std::wstring : work with unicode or wide character strings
	 * LPCWSTR : windows specific type , stands for long pointer to constant wide string
	 * a pointer to a null-ternimated ('\0') array of wide characters (const wchar_t*)
	*/
	std::wstring mAssetsPath;
	std::wstring mScenePath;

	
};
