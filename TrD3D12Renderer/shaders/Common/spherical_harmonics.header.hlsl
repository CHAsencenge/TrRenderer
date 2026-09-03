#ifndef TR_SPHERICAL_HARMONICS_HEADER_HLSL
#define TR_SPHERICAL_HARMONICS_HEADER_HLSL

#define TR_SH_L2_COEFFICIENT_COUNT 9
#define TR_SH_L2_COEFFICIENT_GRID_DIMENSION 3 // SH 系数在纹理 Atlas 中的排布尺寸，九个系数排成一个 3×3 小块

static const float TR_SH_PI = 3.14159265359f;

// Real, orthonormal spherical-harmonic basis through band 2. Coefficients are
// stored in world space so they remain meaningful across camera motion.
// Y_{lm}(N_pixel)
void TrEvaluateShL2Basis(float3 direction, out float basis[TR_SH_L2_COEFFICIENT_COUNT])
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
// E_lm = A_l * L_lm，同一个 band 内的所有 m 使用相同系数
// A0 = pi, A1 = 2pi/3, A2 = pi/4
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

// 例如 Probe 坐标为 (5, 2)，该 Probe 的 Atlas 左上角 = (15, 6)
uint2 TrShL2AtlasCoordinate(uint2 probeCoordinate, uint coefficientIndex)
{
    return probeCoordinate * TR_SH_L2_COEFFICIENT_GRID_DIMENSION + uint2(
        coefficientIndex % TR_SH_L2_COEFFICIENT_GRID_DIMENSION,
        coefficientIndex / TR_SH_L2_COEFFICIENT_GRID_DIMENSION);
}

#endif
