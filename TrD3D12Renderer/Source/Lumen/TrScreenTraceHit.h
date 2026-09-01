#pragma once

#include <cstdint>
#include <limits>

// Shared semantic contract for one Screen Trace ray. The GPU representation
// is a R32G32B32A32_UINT texel so every probe ray can address its result by
// trace-atlas coordinate without an additional indirection buffer.
enum class TrScreenTraceStatus : std::uint32_t
{
    InvalidProbe = 0,
    Hit = 1,
    MissDistance = 2,
    MissScreen = 3,
    MissIterations = 4
};

enum class TrTraceHitSource : std::uint32_t
{
    None = 0,
    Screen = 1,
    RayQuery = 2,
    SoftwareRayTracing = 3
};

namespace TrScreenTraceHitPacking
{
    constexpr std::uint32_t InvalidHitPixel =
        std::numeric_limits<std::uint32_t>::max();

    constexpr std::uint32_t StatusShift = 0;
    constexpr std::uint32_t StatusMask = 0x7u;
    constexpr std::uint32_t SourceShift = 3;
    constexpr std::uint32_t SourceMask = 0x3u;
    constexpr std::uint32_t FrontFaceShift = 5;
    constexpr std::uint32_t LastMipShift = 6;
    constexpr std::uint32_t LastMipMask = 0x1fu;
    constexpr std::uint32_t IterationShift = 11;
    constexpr std::uint32_t IterationMask = 0xffu;

    constexpr std::uint32_t PackHitPixel(
        std::uint32_t x,
        std::uint32_t y)
    {
        return (x & 0xffffu) | ((y & 0xffffu) << 16u);
    }

    constexpr std::uint32_t UnpackHitPixelX(std::uint32_t packedPixel)
    {
        return packedPixel & 0xffffu;
    }

    constexpr std::uint32_t UnpackHitPixelY(std::uint32_t packedPixel)
    {
        return packedPixel >> 16u;
    }

    constexpr std::uint32_t PackFlags(
        TrScreenTraceStatus status,
        TrTraceHitSource source,
        std::uint32_t lastMip,
        std::uint32_t iterationCount,
        bool frontFace = false)
    {
        return
            ((static_cast<std::uint32_t>(status) & StatusMask) << StatusShift) |
            ((static_cast<std::uint32_t>(source) & SourceMask) << SourceShift) |
            ((frontFace ? 1u : 0u) << FrontFaceShift) |
            ((lastMip & LastMipMask) << LastMipShift) |
            ((iterationCount & IterationMask) << IterationShift);
    }

    constexpr TrScreenTraceStatus GetStatus(std::uint32_t packedFlags)
    {
        return static_cast<TrScreenTraceStatus>(
            (packedFlags >> StatusShift) & StatusMask);
    }

    constexpr TrTraceHitSource GetSource(std::uint32_t packedFlags)
    {
        return static_cast<TrTraceHitSource>(
            (packedFlags >> SourceShift) & SourceMask);
    }

    constexpr std::uint32_t GetLastMip(std::uint32_t packedFlags)
    {
        return (packedFlags >> LastMipShift) & LastMipMask;
    }

    constexpr std::uint32_t GetIterationCount(std::uint32_t packedFlags)
    {
        return (packedFlags >> IterationShift) & IterationMask;
    }
}

struct alignas(16) TrScreenTraceHit
{
    // R: low 16 bits = pixel X, high 16 bits = pixel Y.
    std::uint32_t PackedHitPixel =
        TrScreenTraceHitPacking::InvalidHitPixel;
    // G: stored through asuint on GPU; ray direction is normalized.
    float HitT = 0.0f;
    // B: status, source, last HZB mip and iteration count.
    std::uint32_t PackedFlags = TrScreenTraceHitPacking::PackFlags(
        TrScreenTraceStatus::InvalidProbe,
        TrTraceHitSource::None,
        0,
        0);
    // A: stored through asuint on GPU.
    float Confidence = 0.0f;
};

static_assert(sizeof(TrScreenTraceHit) == 16);
