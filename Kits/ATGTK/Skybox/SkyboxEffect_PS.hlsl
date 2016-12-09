//--------------------------------------------------------------------------------------
// SkyboxEffect_PS.hlsl
//
// A simple flimic tonemapping effect for DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "SkyboxEffect_Common.hlsli"

TextureCube<float4> CubeMap : register(t0);
SamplerState Sampler        : register(s0);

[RootSignature(SkyboxRS)]
float4 main(PSInputPixelLightingTxTangent pin) : SV_TARGET0
{
    return CubeMap.Sample(Sampler, normalize(pin.TexCoord));
}