//--------------------------------------------------------------------------------------
// ToneMapEffect_PS.hlsl
//
// A simple flimic tonemapping effect for DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "ToneMapEffect_Common.hlsli"

Texture2D<float4> HDRTexture : register(t0);
SamplerState PointSampler   : register(s0);             // Point sample the HDR scene

// HDR to SDR tone mapping using a simple Reinhard operator
float3 TonemapReinhard(float3 HDRSceneValue)
{
    return HDRSceneValue / (1.0f + HDRSceneValue);
}

// HDR to SDR tone mapping using a simple Filmic operator
float3 TonemapFilmic(float3 HDRSceneValue)
{
    float3 x = max(0.0f, HDRSceneValue - 0.004f);
    return pow((x * (6.2f * x + 0.5f)) / (x * (6.2f * x + 1.7f) + 0.06f), 2.2f);
}

[RootSignature(ToneMapRS)]
float4 main(float2 texCoord : TEXCOORD0) : SV_TARGET0
{
    float4 hdr = HDRTexture.Sample(PointSampler, texCoord);
    float4 sdr = float4(TonemapReinhard(hdr.xyz),1);

    return sdr;
}