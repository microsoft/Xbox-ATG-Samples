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
VSOut_Velocity VSConstantVelocity(VSInputNmTxTangent vin)
{
    VSOut_Velocity vout;

    CommonVSOutputPixelLighting cout = ComputeCommonVSOutputPixelLighting(vin.Position, vin.Normal, PBR_World, PBR_WorldViewProj, PBR_WorldInverseTranspose);
    
    vout.current.PositionPS = cout.Pos_ps;
    vout.current.PositionWS = float4(cout.Pos_ws, 1);
    vout.current.NormalWS = cout.Normal_ws;
    vout.current.Diffuse = float4(PBR_ConstantAlbedo, 1);
    vout.current.TexCoord = vin.TexCoord;
    vout.current.TangentWS = normalize(mul(vin.Tangent.xyz, PBR_WorldInverseTranspose));
    vout.prevPosition = mul(vin.Position, PBR_PrevWorldViewProj);

    return vout;
}
