#pragma once
#include "TrD3D12Util.h"

class VertexBase
{
public:
    VertexBase(DirectX::XMFLOAT3 position, DirectX::XMFLOAT4 color);
    DirectX::XMFLOAT3 mPosition;
    DirectX::XMFLOAT4 mColor;
};


