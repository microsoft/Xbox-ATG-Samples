//--------------------------------------------------------------------------------------
// BC1CompressTailMips.hlsl
//
// Fast block compression ComputeShader for BC5 for the mipmap "tails"
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "BlockCompress.hlsli"

Texture2D g_texIn : register(t0);

RWTexture2D<uint4> g_tex16Out : register(u0);    // Output BC5 texture as R32G32B32A32_UINT
RWTexture2D<uint4> g_tex8Out : register(u1);
RWTexture2D<uint4> g_tex4Out : register(u2);
RWTexture2D<uint4> g_tex2Out : register(u3);
RWTexture2D<uint4> g_tex1Out : register(u4);

SamplerState g_samp : register(s0);

//--------------------------------------------------------------------------------------
// Compress the "tail" mips 16x16, 8x8, 4x4, 2x2, and 1x1
//--------------------------------------------------------------------------------------
[RootSignature(BlockCompressRS)]
[numthreads(COMPRESS_ONE_MIP_THREADGROUP_WIDTH, COMPRESS_ONE_MIP_THREADGROUP_WIDTH, 1)]
void main(
    uint2 threadIDWithinDispatch : SV_DispatchThreadID)
{
    float blockU[16], blockV[16];
    int mipBias = 0;
    float oneOverTextureSize = 1.0f;
    uint2 blockID = threadIDWithinDispatch;

    // Different threads in the threadgroup work on different mip levels
    CalcTailMipsParams(threadIDWithinDispatch, oneOverTextureSize, blockID, mipBias);
    LoadTexelsUVBias(g_texIn, g_samp, oneOverTextureSize, blockID, mipBias, blockU, blockV);
    uint4 compressed = CompressBC5Block(blockU, blockV, 1.0f);

    if (mipBias == 0)
    {
        g_tex16Out[blockID] = compressed;
    }
    else if (mipBias == 1)
    {
        g_tex8Out[blockID] = compressed;
    }
    else if (mipBias == 2)
    {
        g_tex4Out[blockID] = compressed;
    }
    else if (mipBias == 3)
    {
        g_tex2Out[blockID] = compressed;
    }
    else if (mipBias == 4)
    {
        g_tex1Out[blockID] = compressed;
    }
}
