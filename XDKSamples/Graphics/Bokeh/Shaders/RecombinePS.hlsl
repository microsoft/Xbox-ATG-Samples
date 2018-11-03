//--------------------------------------------------------------------------------------
// RecombinePS.hlsl
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Bokeh.hlsli"

// after all the layers are generated, this step combines them with the in-focus image for the final image
// in-focus image occludes far layer and the near layer occludes both in-focus and far layers
float4 main(PSSceneIn input) : SV_Target
{
    // full resolution in-focus colour and linear depth
    float4 rgbz = t3.Load(uint3(input.tex * g_screenSize, 0));

    BokehInfo    i;
    i.fCoCRadius = CalculateCoC(rgbz.w, i.fDepthDifference);

    float4 c = float4(rgbz.xyz, 1);

    // in focus area totally obscures far range and is always weight = 1
    // this doesn't have to be exclusive, it's possible to blend one into another over a range of CoC values
    if (i.fCoCRadius > 1.0f &&
        i.fDepthDifference > 0)
    {
        float2  texUvFar = ((input.tex * g_viewports[1].zw) + g_viewports[1].xy) / g_dofTexSize;
        float2  texUvFar2 = ((input.tex * g_viewports[3].zw) + g_viewports[3].xy) / g_dofTexSize;
        float2  texUvFar4 = ((input.tex * g_viewports[5].zw) + g_viewports[5].xy) / g_dofTexSize;

        float4  clrFar = t0.Sample(s0, texUvFar);
        float4  clrFar2 = t0.Sample(s0, texUvFar2);
        float4  clrFar4 = t0.Sample(s0, texUvFar4);

        float4 far = clrFar + clrFar2 + clrFar4;

        c = lerp(c, far, min(1, i.fCoCRadius - 1));   // a small blend over to cover the transition

                                                      // normalize so the image doesn't get brighter or dimmer
        if (c.w > 0.00001f)
            c.xyz /= c.w;

        c.w = 1;
    }

    // blend in the near layer
    float2  texUvNear = ((input.tex * g_viewports[0].zw) + g_viewports[0].xy) / g_dofTexSize;
    float2  texUvNear2 = ((input.tex * g_viewports[2].zw) + g_viewports[2].xy) / g_dofTexSize;
    float2  texUvNear4 = ((input.tex * g_viewports[4].zw) + g_viewports[4].xy) / g_dofTexSize;

    float4  clrNear = t0.Sample(s0, texUvNear);
    float4  clrNear2 = t0.Sample(s0, texUvNear2);
    float4  clrNear4 = t0.Sample(s0, texUvNear4);

    float4 near = clrNear + clrNear2 + clrNear4;

    if (near.w > 0)
    {
        float occlusion = 0;

        if (i.fCoCRadius >= 1.0f &&
            i.fDepthDifference < 0)
        {
            float nearToInFocusTransition = min(1, (i.fCoCRadius - 1));
            occlusion = max(occlusion, nearToInFocusTransition);   // a small blend over to cover the transition
        }

        // Occlusion can be greater than one due to unnormalised energy at this point
        // so we only interpolate when occlusion is less than one, otherwise we assume
        // near range fully obscures far range
        if (occlusion < 1)
            c = c * (1 - min(1, near.w)) + near;
        else
            c = near;

        // normalize so the image doesn't get brighter or dimmer
        if (c.w > 0.00001f)
            c.xyz /= c.w;
    }

    return float4(c.xyz, 1);
}