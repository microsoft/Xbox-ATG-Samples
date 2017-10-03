//--------------------------------------------------------------------------------------
// FullScreenQuadVS.hlsl
//
// Simple vertex shader to draw a full-screen quad
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "FullScreenQuad.hlsli"

Interpolators main11(uint vI : SV_VertexId)
{
    Interpolators output;

    float2 texcoord = float2(vI & 1, vI >> 1);
    output.TexCoord = texcoord;
    output.Position = float4((texcoord.x - 0.5f) * 2.0f, -(texcoord.y - 0.5f) * 2.0f, 0.0f, 1.0f);

    return output;
}

[RootSignature(FullScreenQuadRS)]
Interpolators main(uint vI : SV_VertexId)
{
    return main11(vI);
}