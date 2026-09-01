
#include "TrVertex.h"

TrColorVertex::TrColorVertex(DirectX::XMFLOAT3 position, DirectX::XMFLOAT4 color)
{
    mPosition = position;
    mColor = color;
}

TrTextureVertex::TrTextureVertex(DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 uv)
{
    mPosition = position;
    mUv = uv;
}
