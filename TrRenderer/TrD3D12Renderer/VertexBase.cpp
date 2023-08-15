#include "pch.h"
#include "VertexBase.h"

VertexBase::VertexBase(DirectX::XMFLOAT3 position, DirectX::XMFLOAT4 color)
{
    mPosition = position;
    mColor = color;
}