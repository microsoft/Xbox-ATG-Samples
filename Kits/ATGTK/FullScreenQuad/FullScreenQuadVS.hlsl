//--------------------------------------------------------------------------------------
// FullScreenQuadVS.hlsl
//
// Simple vertex shader to draw a full-screen quad
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "FullScreenQuad.hlsli"

Interpolators main11(uint vI : SV_VertexId)
{
    Interpolators output;

    // We use the 'big triangle' optimization so you only Draw 3 verticies instead of 4.
    float2 texcoord = float2((vI << 1) & 2, vI & 2);
    output.TexCoord = texcoord;
    output.Position = float4(texcoord.x * 2 - 1, -texcoord.y * 2 + 1, 0, 1);

    return output;
}

[RootSignature(FullScreenQuadRS)]
Interpolators main(uint vI : SV_VertexId)
{
    return main11(vI);
}