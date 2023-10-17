#pragma once
#include "TrD3D12Util.h"

class VertexBase
{
public:
    VertexBase(DirectX::XMFLOAT3 position, DirectX::XMFLOAT4 color);
    DirectX::XMFLOAT3 mPosition;
    DirectX::XMFLOAT4 mColor;
};

// need uv
class TextureVertexBase
{
public:
    TextureVertexBase(DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 uv);
    DirectX::XMFLOAT3 mPosition;
    DirectX::XMFLOAT2 mUv;
};


