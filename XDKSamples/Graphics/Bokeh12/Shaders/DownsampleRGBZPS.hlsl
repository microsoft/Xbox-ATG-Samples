//--------------------------------------------------------------------------------------
// DownsampleRGBZPS.hlsl
//
// Demonstrates how to render depth of field using point sprites.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Bokeh.hlsli"

// downsample the RGBZ texture
[RootSignature(BokehRS)]
float4 main(in float2 dummy : TEXCOORD0, in float4 p : SV_Position) : SV_Target
{
    float3  color = t0.Sample(s0, dummy).xyz;
    float4  depths = t0.GatherAlpha(s0, dummy);
    float   depth = min(depths.x, min(depths.y, min(depths.z, depths.w)));

    return float4(color, depth);
}