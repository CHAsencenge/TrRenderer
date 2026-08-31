#pragma once
#include "TrUtil.h"

class TrColorVertex
{
public:
    TrColorVertex(DirectX::XMFLOAT3 position, DirectX::XMFLOAT4 color);
    DirectX::XMFLOAT3 mPosition;
    DirectX::XMFLOAT4 mColor;
};

// need uv
class TrTextureVertex
{
public:
    TrTextureVertex(DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 uv);
    DirectX::XMFLOAT3 mPosition;
    DirectX::XMFLOAT2 mUv;
};


