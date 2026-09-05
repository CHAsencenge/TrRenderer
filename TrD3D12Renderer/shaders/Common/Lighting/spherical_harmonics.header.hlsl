#ifndef TR_SPHERICAL_HARMONICS_HEADER_HLSL
#define TR_SPHERICAL_HARMONICS_HEADER_HLSL

#define TR_SH_L2_COEFFICIENT_COUNT 9
#define TR_SH_L2_COEFFICIENT_GRID_DIMENSION 3

static const float TR_SH_PI = 3.14159265359f;

// Real, orthonormal spherical-harmonic basis through band 2. Coefficients are
// stored in world space so they remain meaningful across camera motion.
void TrEvaluateShL2Basis(
    float3 direction,
    out float basis[TR_SH_L2_COEFFICIENT_COUNT])
{
    const float3 d = normalize(direction);
    basis[0] = 0.2820947918f;
    basis[1] = 0.4886025119f * d.y;
    basis[2] = 0.4886025119f * d.z;
    basis[3] = 0.4886025119f * d.x;
    basis[4] = 1.0925484306f * d.x * d.y;
    basis[5] = 1.0925484306f * d.y * d.z;
    basis[6] = 0.3153915653f * (3.0f * d.z * d.z - 1.0f);
    basis[7] = 1.0925484306f * d.x * d.z;
    basis[8] = 0.5462742153f * (d.x * d.x - d.y * d.y);
}

// Zonal coefficients of the clamped-cosine kernel. Multiplying radiance SH
// coefficients by these factors converts them into diffuse irradiance SH.
float TrShDiffuseConvolutionFactor(uint coefficientIndex)
{
    if(coefficientIndex == 0u)
    {
        return TR_SH_PI;
    }
    if(coefficientIndex <= 3u)
    {
        return 2.0f * TR_SH_PI / 3.0f;
    }
    return TR_SH_PI / 4.0f;
}

uint2 TrShL2AtlasCoordinate(uint2 probeCoordinate, uint coefficientIndex)
{
    return probeCoordinate * TR_SH_L2_COEFFICIENT_GRID_DIMENSION + uint2(
        coefficientIndex % TR_SH_L2_COEFFICIENT_GRID_DIMENSION,
        coefficientIndex / TR_SH_L2_COEFFICIENT_GRID_DIMENSION);
}

#endif
