#ifndef GBUFFER_H
#define GBUFFER_H

struct VSInput
{
    float3 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float3 tangent : TANGENT;
    uint instance : SV_InstanceID;
};

struct GSInput
{
    float4 position : SV_POSITION;
    float3 normal : TEXTURE_1;
    float2 texcoord : TEXCOORD_2;
    float3 tangent : TEXCOORD_3;
    uint instance : SV_InstanceID;
};

struct GSOutput
{
    float4 position : SV_POSITION;
    float3 normal : TEXTURE_1;
    float2 texcoord : TEXCOORD_2;
    float3 tangent : TEXCOORD_3;
    uint instance : SV_RenderTargetArrayIndex;
};

Texture2D<float4> g_diffuse : register(t0);
Texture2D<float4> g_ambient : register(t1);
Texture2D<float4> g_normal : register(t2);

SamplerState samplerLinearCLamp : register(s2);

ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<MaterialConstantBuffer> g_material : register(b2);

#endif