#include "TrD3D12Util.h"
#include "TrD3D12RendererBase.h"


TrD3D12RendererBase::TrD3D12RendererBase(UINT width, UINT height, std::wstring title)
{
	mWidth = width;
	mHeight = height;
	mTitle = title;
	mAspectRatio = static_cast<float>(width) / static_cast<float>(height);

	WCHAR assetsPath[512];
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
void TrD3D12RendererBase::ParseCommandLineArgs(WCHAR* argv[], int argc)
{
}
