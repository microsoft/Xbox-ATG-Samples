//--------------------------------------------------------------------------------------
// PBREffect_PSConstant.hslsi
//
// A physically based shader for forward rendering on DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "PBREffect_Common.hlsli"

// Pixel shader: pixel lighting + texture.
[RootSignature(PBREffectRS)]
float4 PSConstant(PSInputPixelLightingTxTangent pin) : SV_Target0
{
    // vectors
    const float3 V = normalize(PBR_EyePosition - pin.PositionWS.xyz); // view vector
    const float3 N = normalize(pin.NormalWS);                         // surface normal
    const float AO = 1;                                               // ambient term

    float3 output = PBR_LightSurface(V, N, 3, PBR_LightColor, PBR_LightDirection,
                                PBR_ConstantAlbedo, PBR_ConstantRoughness, PBR_ConstantMetallic, AO);
    
    return float4(output, 1);
}
