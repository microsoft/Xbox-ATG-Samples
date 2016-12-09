//--------------------------------------------------------------------------------------
// SkyboxEffect_VS.hlsl
//
// A simple flimic tonemapping effect for DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
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