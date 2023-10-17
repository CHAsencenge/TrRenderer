
#include "VertexBase.h"

VertexBase::VertexBase(DirectX::XMFLOAT3 position, DirectX::XMFLOAT4 color)
{
    mPosition = position;
    mColor = color;
}

TextureVertexBase::TextureVertexBase(DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 uv)
{
    mPosition = position;
    mUv = uv;
}
