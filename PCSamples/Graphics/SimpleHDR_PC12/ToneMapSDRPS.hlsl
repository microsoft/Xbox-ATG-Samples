//--------------------------------------------------------------------------------------
// ToneMapSDRPS.hlsl
//
// Takes the final HDR back buffer with linear values using Rec.709 color primaries and
// outputs the SDR signal. The SDR signal uses Rec.709 color primaries which the
// hardware will apply the Rec.709 gamma curve.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "FullScreenQuad.hlsli"

// Very simple HDR to SDR tonemapping, which simply clips values above 1.0f from the HDR scene.
[RootSignature(FullScreenQuadRS)]
float4 main(Interpolators In) : SV_Target0
{
    // Input is linear values using sRGB / Rec.709 color primaries
    float4 hdrSceneValues = Texture.Sample(PointSampler, In.TexCoord);

    return saturate(hdrSceneValues);
}