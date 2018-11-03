//--------------------------------------------------------------------------------------
// CreateRGBZPS.hlsl
//
// Demonstrates how to render depth of field using point sprites.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Bokeh.hlsli"

// this creates a full screen version of the RGBZ texture from a source colour and depth texture
// note that we only consume a quarter res version of this texture in point sprites generation
// and we consume this in the last step.
// note -- if the soruce texture and destination texture aern't the same, we can skip this
// copy and downsample straight from source color and depth to save time
float4 main(in float2 dummy : TEXCOORD0, in float4 p : SV_Position) : SV_Target
{
    uint2   pos = uint2(p.xy);
    float3  color = t0.Load(uint3(pos, 0)).xyz;

    const BokehInfo    i = GetBokehInfo(pos);

    return float4(color, i.fDepth);
}