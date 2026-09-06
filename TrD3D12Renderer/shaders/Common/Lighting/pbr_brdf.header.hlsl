#ifndef TR_PBR_BRDF_HEADER_HLSL
#define TR_PBR_BRDF_HEADER_HLSL

static const float TR_PBR_PI = 3.14159265359f;
static const float TR_PBR_INV_PI = 0.31830988618f;

float TrPow5(float value)
{
    const float valueSquared = value * value;
    return valueSquared * valueSquared * value;
}

// Schlick Fresnel approximation:
// F(V, H) = F0 + (1 - F0) * (1 - saturate(V dot H))^5.
// F0 is the reflectance at normal incidence; grazing angles approach 1.
float3 TrFresnelSchlick(float3 reflectanceAtNormalIncidence, float viewDotHalf)
{
    return reflectanceAtNormalIncidence +
        (1.0f - reflectanceAtNormalIncidence) *
        TrPow5(1.0f - saturate(viewDotHalf));
}

// Trowbridge-Reitz GGX normal distribution. The material roughness is the
// perceptual glTF roughness; alpha = roughness^2 is the microfacet slope.
// D_GGX(N, H) = alpha^2 /
//     (PI * ((N dot H)^2 * (alpha^2 - 1) + 1)^2).
float TrDistributionGgx(float normalDotHalf, float perceptualRoughness)
{
    const float alpha = max(
        perceptualRoughness * perceptualRoughness,
        0.0025f);
    const float alphaSquared = alpha * alpha;
    const float denominator = normalDotHalf * normalDotHalf *
        (alphaSquared - 1.0f) + 1.0f;
    return alphaSquared /
        max(TR_PBR_PI * denominator * denominator, 1.0e-7f);
}

// Height-correlated Smith visibility. This is G / (4 * NoV * NoL), so the
// caller multiplies D * visibility * F directly to obtain the specular BRDF.
// V_GGX = 0.5 / (NoL * sqrt(NoV^2 * (1 - alpha^2) + alpha^2)
//                    + NoV * sqrt(NoL^2 * (1 - alpha^2) + alpha^2)).
float TrVisibilitySmithGgxCorrelated(
    float normalDotView,
    float normalDotLight,
    float perceptualRoughness)
{
    const float alpha = max(
        perceptualRoughness * perceptualRoughness,
        0.0025f);
    const float alphaSquared = alpha * alpha;
    const float viewLambda = normalDotLight * sqrt(
        normalDotView * normalDotView * (1.0f - alphaSquared) +
        alphaSquared);
    const float lightLambda = normalDotView * sqrt(
        normalDotLight * normalDotLight * (1.0f - alphaSquared) +
        alphaSquared);
    return 0.5f / max(viewLambda + lightLambda, 1.0e-6f);
}

struct TrPbrBrdfTerms
{
    float3 diffuse;
    float3 specular;
};

TrPbrBrdfTerms TrEvaluateMetallicRoughnessBrdfTerms(
    float3 baseColor,
    float metallic,
    float perceptualRoughness,
    float3 worldNormal,
    float3 directionToView,
    float3 directionToLight)
{
    TrPbrBrdfTerms result = (TrPbrBrdfTerms)0;
    const float3 normal = normalize(worldNormal);
    const float3 view = normalize(directionToView);
    const float3 light = normalize(directionToLight);
    const float normalDotView = saturate(dot(normal, view));
    const float normalDotLight = saturate(dot(normal, light));
    if(normalDotView <= 0.0f || normalDotLight <= 0.0f)
    {
        return result;
    }

    // Microfacet half vector: H = normalize(V + L). The BRDF below uses the
    // common abbreviations NoV=N dot V, NoL=N dot L, NoH=N dot H, VoH=V dot H.
    const float3 halfVector = normalize(view + light);
    const float normalDotHalf = saturate(dot(normal, halfVector));
    const float viewDotHalf = saturate(dot(view, halfVector));
    const float clampedMetallic = saturate(metallic);
    const float clampedRoughness = saturate(perceptualRoughness);

    // glTF's metallic-roughness model uses 4% reflectance for a dielectric and
    // the base color itself as F0 for a metal.
    const float3 reflectanceAtNormalIncidence = lerp(
        0.04f.xxx,
        saturate(baseColor),
        clampedMetallic);
    const float3 fresnel = TrFresnelSchlick(
        reflectanceAtNormalIncidence,
        viewDotHalf);
    const float distribution = TrDistributionGgx(
        normalDotHalf,
        clampedRoughness);
    const float visibility = TrVisibilitySmithGgxCorrelated(
        normalDotView,
        normalDotLight,
        clampedRoughness);
    // Cook-Torrance specular term: f_specular = D_GGX * V_GGX * F_Schlick.
    // V_GGX already contains the usual 1 / (4 * NoV * NoL) denominator.
    result.specular = distribution * visibility * fresnel;

    // Fresnel-reflected energy and metallic energy cannot also become diffuse.
    // f_diffuse = (1 - F) * (1 - metallic) * baseColor / PI.
    const float3 diffuseWeight = (1.0f - fresnel) *
        (1.0f - clampedMetallic);
    result.diffuse = diffuseWeight * saturate(baseColor) *
        TR_PBR_INV_PI;
    return result;
}

float3 TrEvaluateMetallicRoughnessBrdf(
    float3 baseColor,
    float metallic,
    float perceptualRoughness,
    float3 worldNormal,
    float3 directionToView,
    float3 directionToLight)
{
    const TrPbrBrdfTerms terms = TrEvaluateMetallicRoughnessBrdfTerms(
        baseColor,
        metallic,
        perceptualRoughness,
        worldNormal,
        directionToView,
        directionToLight);
    // Metallic-roughness BRDF: f_r(V, L) = f_diffuse + f_specular.
    return terms.diffuse + terms.specular;
}

#endif
