//--------------------------------------------------------------------------------------
// PBREffect_VSConstant.hslsi
//
// A physically based shader for forward rendering on DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "PBREffect_Common.hlsli"

// Shared vertex shader for constant (debugging) and textured variants
[RootSignature(PBREffectRS)]
VSOutputPixelLightingTxTangent VSConstant(VSInputNmTxTangent vin)
{
    VSOutputPixelLightingTxTangent vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal, PBR_World, PBR_WorldViewProj, PBR_WorldInverseTranspose);
    
    vout.PositionPS = cout.Pos_ps;
    vout.PositionWS = float4(cout.Pos_ws, 1);
    vout.NormalWS = cout.Normal_ws;
    vout.Diffuse = float4(PBR_ConstantAlbedo, 1);
    vout.TexCoord = vin.TexCoord;
    vout.TangentWS = normalize(mul(vin.Tangent.xyz, PBR_WorldInverseTranspose));

    return vout;
}
