struct FullscreenVertex
{
    float4 position : SV_POSITION;
};

cbuffer SceneConstants : register(b0)
{
    float3 g_lightDirection;
    float g_lightIntensity;
    float3 g_lightColor;
    float g_ambientStrength;
};

cbuffer ViewConstants : register(b1)
{
    float4x4 g_view;
    float4x4 g_projection;
    float4x4 g_viewProjection;
    float4x4 g_inverseViewProjection;
    float4x4 g_previousViewProjection;
    float3 g_cameraPosition;
    float g_nearPlane;
    float2 g_renderSize;
    float2 g_inverseRenderSize;
    float2 g_temporalJitter;
    float2 g_previousTemporalJitter;
    uint g_frameNumber;
    float g_farPlane;
    float2 g_viewPadding;
};

cbuffer DeferredLightingPassConstants : register(b2)
{
    float g_directLightingScale;
    float g_ambientLightingScale;
    uint g_lightingDebugView;
    float g_lightingPadding;
};

Texture2D<float4> g_baseColorRoughness : register(t0);
Texture2D<float4> g_normalMetallic : register(t1);
Texture2D<float> g_depth : register(t2);

FullscreenVertex VSMain(uint vertexId : SV_VertexID)
{
    const float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    FullscreenVertex result;
    result.position = float4(positions[vertexId], 0.0f, 1.0f);
    return result;
}

float4 PSMain(FullscreenVertex input) : SV_Target
{
    const int2 pixel = min(
        int2(input.position.xy),
        int2(g_renderSize) - 1);
    const float depth = g_depth.Load(int3(pixel, 0));
    if(depth >= 1.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    const float3 baseColor = g_baseColorRoughness.Load(int3(pixel, 0)).rgb;
    const float3 worldNormal = normalize(g_normalMetallic.Load(int3(pixel, 0)).xyz);
    if(g_lightingDebugView == 1u)
    {
        return float4(worldNormal * 0.5f + 0.5f, 1.0f);
    }

    const float diffuse = saturate(dot(worldNormal, normalize(g_lightDirection)));
    const float lighting = g_ambientStrength * g_ambientLightingScale +
        (1.0f - g_ambientStrength) * g_lightIntensity *
        g_directLightingScale * diffuse;
    return float4(baseColor * g_lightColor * lighting, 1.0f);
}
