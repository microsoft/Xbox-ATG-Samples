//--------------------------------------------------------------------------------------
// QuadWithCamera.hlsl
//
// Draw a viewport-filling quad with zoom and offset
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "FullScreenQuad.hlsli"

cbuffer QuadWithCameraCB : register(b0)
{
    float g_oneOverZoom;
    float g_offsetX;
    float g_offsetY;
    float g_textureWidth;
    float g_textureHeight;
    uint g_mipLevel;
    bool g_highlightBlocks;
    bool g_colorDiffs;
    bool g_alphaDiffs;
    bool g_reconstructZ;
};

Texture2D       g_tex : register(t0);
Texture2D       g_originalTex : register(t1);

static const float g_diffsScale = 10.0f;

// Reconstruct the Z component of a normal map
float ReconstructZ(float2 v)
{
    v = v * 2 - 1;
    float z = sqrt(max(0, 1 - dot(v, v)));

    return 0.5f*(z + 1);
}

// Convert from UV to texel coordinates
uint2 UVToTexel(float2 uv)
{
    return int2(uv.x * g_textureWidth, uv.y * g_textureHeight);
}


//--------------------------------------------------------------------------------------
// Shader entry point
//--------------------------------------------------------------------------------------
[RootSignature(FullScreenQuadRS)]
float4 main(Interpolators In) : SV_Target
{
    float2 texCoord = In.TexCoord * g_oneOverZoom;
    texCoord += float2(g_offsetX, g_offsetY);

    // If desired, highlight block boundaries
    if (g_highlightBlocks)
    {
        uint2 prevBlock = UVToTexel(texCoord - float2(ddx(texCoord.x), ddy(texCoord.y))) / 4;
        uint2 currBlock = UVToTexel(texCoord) / 4;

        if (currBlock.x != prevBlock.x || currBlock.y != prevBlock.y)
        {
            return float4(1.0f, 0.0f, 1.0f, 1.0f);
        }
    }

    float4 texelColor = g_tex.SampleLevel(PointSampler, texCoord, g_mipLevel);
    if (g_reconstructZ) texelColor.z = ReconstructZ(texelColor.xy);

    if (g_colorDiffs)
    {
        float4 originalTexelColor = g_originalTex.SampleLevel(PointSampler, texCoord, g_mipLevel);
        float4 diff = g_diffsScale * abs(texelColor - originalTexelColor);

        return float4(diff.rgb, 1.0f);
    }
    else if (g_alphaDiffs)
    {
        float4 originalTexelColor = g_originalTex.SampleLevel(PointSampler, texCoord, g_mipLevel);
        float4 diff = g_diffsScale * abs(texelColor - originalTexelColor);

        return float4(diff.aaa, 1.0f);
    }

    // Draw a checkerboard in alpha areas
    const float numCheckers = 32;
    int2 checker = trunc(fmod(In.TexCoord * numCheckers, 2));
    float3 checkerColor = (checker.x ^ checker.y) * 1.f;

    return float4(lerp(checkerColor.rgb, texelColor.rgb, texelColor.a), 1.0f);
}
