//--------------------------------------------------------------------------------------
// PrepareSwapChainBuffersPS.hlsl
//
// Takes the final HDR back buffer with linear values using Rec.709 color primaries and
// outputs two signals, an HDR and SDR signal. The HDR siganl uses Rec.2020 color primaries
// with the ST.2084 curve, whereas the SDR signal uses Rec.709 color primaries which the
// hardware will apply the Rec.709 gamma curve.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "HDRCommon.hlsli"
#include "FullScreenQuad.hlsli"


cbuffer HDR10Data : register(b0)
{
    float PaperWhiteNits;                           // Defines how bright white is (in nits), which controls how bright the SDR range in the image will be, e.g. 200 nits
};



// Very simple HDR to SDR tonemapping, which simply clips values above 1.0f from the HDR scene.
float4 TonemapHDR2SDR(float4 hdrSceneValue)
{
    return saturate(hdrSceneValue);
}

struct Interpolants
{
    float4 color    : COLOR;
    float2 texCoord : TEXCOORD;
};

struct PSOut
{
    float4 HDR10    : SV_Target0;       // HDR10 buffer using Rec.2020 color primaries with ST.2084 curve
    float4 GameDVR  : SV_Target1;       // GameDVR buffer using Rec.709 color primaries with sRGB gamma curve
};

// Prepare the HDR swapchain buffer as HDR10. This means the buffer has to contain data which uses
//  - Rec.2020 color primaries
//  - Quantized using ST.2084 curve
//  - 10-bit per channel
[RootSignature(FullScreenQuadRS)]
PSOut main(Interpolators In)
{
    PSOut output;

    // Input is linear values using sRGB / Rec.709 color primaries
    float4 hdrSceneValues = Texture.Sample(PointSampler, In.TexCoord);

    output.HDR10 = ConvertToHDR10(hdrSceneValues, PaperWhiteNits.x);
    output.GameDVR = TonemapHDR2SDR(hdrSceneValues);

    return output;
}