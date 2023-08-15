#include "pch.h"
#include "TrD3D12RendererBase.h"


std::wstring TrD3D12RendererBase::GetAssetFullPath(LPCWSTR assetName)
{
	return mAssetsPath + assetName;
}
