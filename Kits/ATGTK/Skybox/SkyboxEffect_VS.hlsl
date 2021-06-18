//--------------------------------------------------------------------------------------
// SkyboxEffect_VS.hlsl
//
// A sky box rendering helper.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "SkyboxEffect_Common.hlsli"

[RootSignature(SkyboxRS)]
VSOutputPixelLightingTxTangent main(VSInputNmTxTangent vin)
{
    VSOutputPixelLightingTxTangent vout;

    vout.PositionPS = mul(vin.Position, Skybox_WorldViewProj); 
    vout.PositionPS.z = vout.PositionPS.w; // Draw on far plane
    vout.TexCoord = vin.Position.xyz;

    return vout;
}