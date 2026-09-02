#pragma once

#include <cstdint>

// Runtime switches shared by pipeline scheduling and shader evaluation.
// Keep the bit values synchronized with the shader-side TR_FEATURE_* values.
enum class TrPipelineFeature : std::uint32_t
{
    IndirectLighting = 1u << 0u
};

class TrPipelineFeatures
{
public:
    bool IsEnabled(TrPipelineFeature feature) const
    {
        return (mEnabledMask & ToMask(feature)) != 0u;
    }

    bool SetEnabled(TrPipelineFeature feature, bool enabled)
    {
        const std::uint32_t previousMask = mEnabledMask;
        if(enabled)
        {
            mEnabledMask |= ToMask(feature);
        }
        else
        {
            mEnabledMask &= ~ToMask(feature);
        }
        return previousMask != mEnabledMask;
    }

    std::uint32_t GetEnabledMask() const { return mEnabledMask; }

private:
    static constexpr std::uint32_t ToMask(TrPipelineFeature feature)
    {
        return static_cast<std::uint32_t>(feature);
    }

    std::uint32_t mEnabledMask =
        static_cast<std::uint32_t>(TrPipelineFeature::IndirectLighting);
};
