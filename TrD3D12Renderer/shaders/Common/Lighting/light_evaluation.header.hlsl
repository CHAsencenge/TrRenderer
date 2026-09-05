#ifndef TR_LIGHT_EVALUATION_HEADER_HLSL
#define TR_LIGHT_EVALUATION_HEADER_HLSL

#include "../ABI/light_types.header.hlsl"

float TrEvaluateDistanceAttenuation(float distanceSquared, float range)
{
    float attenuation = rcp(max(distanceSquared, 1.0e-4f));
    if(range > 0.0f)
    {
        const float normalizedDistanceSquared =
            distanceSquared / (range * range);
        attenuation *= saturate(
            1.0f - normalizedDistanceSquared * normalizedDistanceSquared);
    }
    return attenuation;
}

float TrEvaluateSpotAttenuation(
    TrGpuLight light,
    float3 surfaceToLight)
{
    // light.direction points from the light toward the scene, whereas
    // surfaceToLight points in the opposite direction at the shaded point.
    const float actualCos = dot(
        normalize(light.direction),
        -surfaceToLight);
    return saturate(
        (actualCos - light.outerConeCos) /
        max(light.innerConeCos - light.outerConeCos, 1.0e-5f));
}

float3 TrEvaluateLightIrradiance(
    TrGpuLight light,
    float3 worldPosition,
    float3 worldNormal)
{
    float3 surfaceToLight;
    float attenuation = 1.0f;
    if(light.type == TR_LIGHT_TYPE_DIRECTIONAL)
    {
        surfaceToLight = -normalize(light.direction);
    }
    else
    {
        const float3 toLight = light.position - worldPosition;
        const float distanceSquared = dot(toLight, toLight);
        surfaceToLight = toLight * rsqrt(max(distanceSquared, 1.0e-8f));
        attenuation = TrEvaluateDistanceAttenuation(
            distanceSquared,
            light.range);
        if(light.type == TR_LIGHT_TYPE_SPOT)
        {
            attenuation *= TrEvaluateSpotAttenuation(light, surfaceToLight);
        }
    }

    return max(light.color, 0.0f) * max(light.intensity, 0.0f) *
        attenuation * saturate(dot(worldNormal, surfaceToLight));
}

float3 TrEvaluateDirectIrradiance(
    StructuredBuffer<TrGpuLight> lights,
    uint lightCount,
    float3 worldPosition,
    float3 worldNormal)
{
    float3 irradiance = 0.0f;
    [loop]
    for(uint lightIndex = 0u; lightIndex < lightCount; ++lightIndex)
    {
        irradiance += TrEvaluateLightIrradiance(
            lights[lightIndex],
            worldPosition,
            worldNormal);
    }
    return irradiance;
}

#endif
