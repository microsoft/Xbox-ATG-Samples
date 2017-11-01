//--------------------------------------------------------------------------------------
// PrepareSwapChainBuffersPS.hlsl
//
// Takes the final HDR back buffer with linear values using Rec.709 color primaries and
// outputs the HDR signal. The HDR signal uses Rec.2020 color primaries
// with the ST.2084 curve.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "HDRCommon.hlsli"
#include "FullScreenQuad.hlsli"

float4 PaperWhiteNits           : register(b0);     // Defines how bright white is (in nits), which controls how bright the SDR range in the image will be, e.g. 200 nits

// Prepare the HDR swapchain buffer as HDR10. This means the buffer has to contain data which uses
//  - Rec.2020 color primaries
//  - Quantized using ST.2084 curve
//  - 10-bit per channel
float4 main(Interpolators In) : SV_Target0
{
    // Input is linear values using sRGB / Rec.709 color primaries
    float4 hdrSceneValues = Texture.Sample(PointSampler, In.TexCoord);

    return ConvertToHDR10(hdrSceneValues, PaperWhiteNits.x);
}