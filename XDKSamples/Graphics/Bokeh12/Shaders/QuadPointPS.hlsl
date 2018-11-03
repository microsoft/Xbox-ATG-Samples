//--------------------------------------------------------------------------------------
// QuadPointPS.hlsl
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Bokeh.hlsli"

// the last step -- output alpha premultiplied colour using this trivial pixel shader
[RootSignature(BokehRS)]
float4 main(PSSceneInPoint input) : SV_Target
{
    float iris = t1.Sample(s0, input.uv).x;

    float w = input.weight * iris;
    if (w <= 0)
    {
        discard;
    }

    return float4(input.clr.xyz, 1) * w;
}
