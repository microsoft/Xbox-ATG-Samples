//--------------------------------------------------------------------------------------
// BC1Compress.hlsl
//
// Fast block compression ComputeShader for BC1
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//--------------------------------------------------------------------------------------

#include "BlockCompress.hlsli"

Texture2D g_texIn : register(t0);

RWTexture2D<uint2> g_texMip0Out : register(u0);    // Output BC1 texture as R32G32_UINT 

SamplerState g_samp : register(s0);

//--------------------------------------------------------------------------------------
// Compress one mip level
//--------------------------------------------------------------------------------------
[RootSignature(BlockCompressRS)]
[numthreads(COMPRESS_ONE_MIP_THREADGROUP_WIDTH, COMPRESS_ONE_MIP_THREADGROUP_WIDTH, 1)]
void main(
    uint2 threadIDWithinDispatch : SV_DispatchThreadID)
{
    float3 block[16];
    LoadTexelsRGB(g_texIn, g_samp, g_oneOverTextureWidth, threadIDWithinDispatch, block);

    g_texMip0Out[threadIDWithinDispatch] = CompressBC1Block(block);
}
