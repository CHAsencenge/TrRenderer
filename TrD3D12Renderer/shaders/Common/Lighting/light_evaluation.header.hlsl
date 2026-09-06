#ifndef TR_LIGHT_EVALUATION_HEADER_HLSL
#define TR_LIGHT_EVALUATION_HEADER_HLSL

#include "../ABI/light_types.header.hlsl"

struct TrLightSample
{
    float3 directionToLight;
    float3 radiance;
};

float TrEvaluateDistanceAttenuation(float distanceSquared, float range)
{
    // Inverse-square falloff with a finite-range window:
    // A_distance(d) = 1 / max(d^2, epsilon) * saturate(1 - (d / range)^4).
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

    float spot_atten = saturate((actualCos - light.outerConeCos) /
        max(light.innerConeCos - light.outerConeCos, 1.0e-5f));
    
    return spot_atten * spot_atten;
}

TrLightSample TrEvaluateLightSample(
    TrGpuLight light,
    float3 worldPosition)
{
    TrLightSample result;
    float attenuation = 1.0f;
    if(light.type == TR_LIGHT_TYPE_DIRECTIONAL)
    {
        result.directionToLight = -normalize(light.direction);
    }
    else
    {
        const float3 toLight = light.position - worldPosition;
        const float distanceSquared = dot(toLight, toLight);
        result.directionToLight = toLight * rsqrt(max(distanceSquared, 1.0e-8f));
        attenuation = TrEvaluateDistanceAttenuation(
            distanceSquared,
            light.range);
        if(light.type == TR_LIGHT_TYPE_SPOT)
        {
            attenuation *= TrEvaluateSpotAttenuation(
                light,
                result.directionToLight);
        }
    }

    // Incident radiance before the surface cosine term:
    // L_i = lightColor * intensity * A_distance * A_spot.
    result.radiance = max(light.color, 0.0f) *
        max(light.intensity, 0.0f) * attenuation;
    return result;
}

float3 TrEvaluateLightIrradiance(
    TrGpuLight light,
    float3 worldPosition,
    float3 worldNormal)
{
    const TrLightSample lightSample = TrEvaluateLightSample(
        light,
        worldPosition);
    // Irradiance from a punctual/directional light:
    // E = L_i * max(N dot L, 0).
    return lightSample.radiance * saturate(dot(
        worldNormal,
        lightSample.directionToLight));
}

float3 TrEvaluateDirectIrradiance(
    StructuredBuffer<TrGpuLight> lights,
    uint lightCount,
    float3 worldPosition,
    float3 worldNormal)
{
    // Discrete-light integral: E_direct = sum_i L_i * max(N dot L_i, 0).
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
