#include "TrD3D12Util.h"
#include "TrD3D12RendererBase.h"


TrD3D12RendererBase::TrD3D12RendererBase(UINT width, UINT height, std::wstring title)
{
	mWidth = width;
	mHeight = height;
	mTitle = title;
	mbUseWarpDevice = false;
	mAspectRatio = static_cast<float>(width) / static_cast<float>(height);

	WCHAR assetsPath[512];
	// exe location
	GetAssetsPath(assetsPath, _countof(assetsPath));
	mAssetsPath = assetsPath;
}

TrD3D12RendererBase::~TrD3D12RendererBase()
{
}

std::wstring TrD3D12RendererBase::GetAssetFullPath(LPCWSTR assetName)
{
	return mAssetsPath + assetName;
}

// todo: cmd line parse
_Use_decl_annotations_
void TrD3D12RendererBase::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
	for (int i = 1; i < argc; ++i)
	{
		if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 || 
			_wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
		{
			mbUseWarpDevice = true;
			mTitle = mTitle + L" (WARP)";
		}
	}
}

