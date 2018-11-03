//--------------------------------------------------------------------------------------
// RecombineDebugPS.hlsl
//
// Demonstrates how to render depth of field using point sprites.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Bokeh.hlsli"

// Debug function that. Modify this to show various data.
[RootSignature(BokehRS)]
float4 main(PSSceneIn input) : SV_Target
{
    float2  texUvNear = ((input.tex * g_viewports[0].zw) + g_viewports[0].xy) / g_dofTexSize;
    float2  texUvFar = ((input.tex * g_viewports[1].zw) + g_viewports[1].xy) / g_dofTexSize;
    float2  texUvNear2 = ((input.tex * g_viewports[2].zw) + g_viewports[2].xy) / g_dofTexSize;
    float2  texUvFar2 = ((input.tex * g_viewports[3].zw) + g_viewports[3].xy) / g_dofTexSize;
    float2  texUvNear4 = ((input.tex * g_viewports[4].zw) + g_viewports[4].xy) / g_dofTexSize;
    float2  texUvFar4 = ((input.tex * g_viewports[5].zw) + g_viewports[5].xy) / g_dofTexSize;

    const BokehInfo    i = GetBokehInfo(input.tex * g_screenSize);

    float4    clrNear = t0.Sample(s0, texUvNear);
    float4    clrNear2 = t0.Sample(s0, texUvNear2);
    float4    clrNear4 = t0.Sample(s0, texUvNear4);
    float4    clrFar = t0.Sample(s0, texUvFar);
    float4    clrFar2 = t0.Sample(s0, texUvFar2);
    float4    clrFar4 = t0.Sample(s0, texUvFar4);

    float4 c = clrNear + clrNear2 + clrNear4;
    float alpha = min(c.w, 1);

    return lerp(float4(0, 1, 0, 1), c / c.w, alpha);
}