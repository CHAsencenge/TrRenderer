#include "Utilities/TrUtil.h"
#include "TrRendererBase.h"

#include <stdexcept>


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

_Use_decl_annotations_
void TrRendererBase::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
	for (int i = 1; i < argc; ++i)
	{
		if (_wcsicmp(argv[i], L"-warp") == 0 ||
			_wcsicmp(argv[i], L"/warp") == 0)
		{
			mbUseWarpDevice = true;
			mTitle = mTitle + L" (WARP)";
		}
		else if(_wcsicmp(argv[i], L"-scene") == 0 ||
			_wcsicmp(argv[i], L"/scene") == 0)
		{
			if(i + 1 >= argc)
			{
				throw std::invalid_argument("-scene requires a .glb or .trscene path.");
			}
			mScenePath = argv[++i];
		}
		else if(_wcsnicmp(argv[i], L"-scene=", 7) == 0 ||
			_wcsnicmp(argv[i], L"/scene=", 7) == 0)
		{
			if(argv[i][7] == L'\0')
			{
				throw std::invalid_argument("-scene requires a .glb or .trscene path.");
			}
			mScenePath = argv[i] + 7;
		}
	}
}
