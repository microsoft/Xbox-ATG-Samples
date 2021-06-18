//--------------------------------------------------------------------------------------
// SkyboxEffect_PS.hlsl
//
// A sky box rendering helper.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "SkyboxEffect_Common.hlsli"

TextureCube<float4> CubeMap : register(t0);
SamplerState Sampler        : register(s0);

[RootSignature(SkyboxRS)]
float4 main(PSInputPixelLightingTxTangent pin) : SV_TARGET0
{
    return CubeMap.Sample(Sampler, normalize(pin.TexCoord));
}