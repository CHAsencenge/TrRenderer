#include "TrUtil.h"
#include "TrRendererBase.h"


TrRendererBase::TrRendererBase(UINT width, UINT height, std::wstring title)
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

TrRendererBase::~TrRendererBase()
{
}

std::wstring TrRendererBase::GetAssetFullPath(LPCWSTR assetName)
{
	return assetName;
}

// todo: cmd line parse
_Use_decl_annotations_
void TrRendererBase::ParseCommandLineArgs(WCHAR* argv[], int argc)
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

