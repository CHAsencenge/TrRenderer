#pragma once

#include "Backend/TrShaderCompiler.h"

#include <DirectXMath.h>
#include <stdexcept>
#include <vector>

#ifndef TR_REVERSED_Z
#define TR_REVERSED_Z 1
#endif

static_assert(TR_REVERSED_Z == 0 || TR_REVERSED_Z == 1);

enum class TrDepthConvention
{
    Forward,
    Reversed
};

namespace TrRenderConfig
{
    inline constexpr TrDepthConvention DepthConvention = TR_REVERSED_Z
        ? TrDepthConvention::Reversed
        : TrDepthConvention::Forward;
    inline constexpr bool UseReversedZ =
        DepthConvention == TrDepthConvention::Reversed;
    inline constexpr float DepthClearValue = UseReversedZ ? 0.0f : 1.0f;
    inline constexpr D3D12_COMPARISON_FUNC DepthComparison = UseReversedZ
        ? D3D12_COMPARISON_FUNC_GREATER_EQUAL
        : D3D12_COMPARISON_FUNC_LESS_EQUAL;

    inline DirectX::XMMATRIX CreatePerspectiveFovLH(
        float fovY,
        float aspectRatio,
        float nearPlane,
        float farPlane)
    {
        if(fovY <= 0.0f || aspectRatio <= 0.0f || nearPlane <= 0.0f ||
           farPlane <= nearPlane)
        {
            throw std::invalid_argument(
                "Perspective projection parameters are invalid.");
        }

        return DirectX::XMMatrixPerspectiveFovLH(
            fovY,
            aspectRatio,
            UseReversedZ ? farPlane : nearPlane,
            UseReversedZ ? nearPlane : farPlane);
    }

    inline std::vector<TrShaderDefine> GetDepthShaderDefines()
    {
        return
        {
            {L"TR_REVERSED_Z", UseReversedZ ? L"1" : L"0"}
        };
    }
}
