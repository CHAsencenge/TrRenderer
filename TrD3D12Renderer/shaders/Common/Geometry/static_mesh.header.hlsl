#ifndef TR_STATIC_MESH_HEADER_HLSL
#define TR_STATIC_MESH_HEADER_HLSL

struct TrStaticMeshVertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
    float2 texCoord0 : TEXCOORD0;
    float2 texCoord1 : TEXCOORD1;
    float2 texCoord2 : TEXCOORD2;
};

float4 TrTransformLocalPosition(float3 position, float4x4 world)
{
    return mul(float4(position, 1.0f), world);
}

float3 TrTransformLocalNormal(
    float3 normal,
    float4x4 worldInverseTranspose,
    bool reverseOrientation)
{
    float3 worldNormal = mul(
        float4(normal, 0.0f),
        worldInverseTranspose).xyz;
    return reverseOrientation ? -worldNormal : worldNormal;
}

#endif
