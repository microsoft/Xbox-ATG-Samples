//--------------------------------------------------------------------------------------
// FullScreenQuadPS.hlsl
//
// A simple pixel shader to render a texture 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "FullScreenQuad.hlsli"

float4 main11(Interpolators In) : SV_Target0
{
    return Texture.Sample(PointSampler, In.TexCoord);
}

[RootSignature(FullScreenQuadRS)]
float4 main(Interpolators In) : SV_Target0
{
    return main11(In);
}