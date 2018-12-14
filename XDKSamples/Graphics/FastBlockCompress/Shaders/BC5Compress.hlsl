//--------------------------------------------------------------------------------------
// BC5Compress.hlsl
//
// Fast block compression ComputeShader for BC5
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "BlockCompress.hlsli"

Texture2D g_texIn : register(t0);

RWTexture2D<uint4> g_texMip0Out : register(u0);    // Output BC5 texture as R32G32B32A32_UINT

SamplerState g_samp : register(s0);

//--------------------------------------------------------------------------------------
// Compress one mip level
//--------------------------------------------------------------------------------------
[RootSignature(BlockCompressRS)]
[numthreads(COMPRESS_ONE_MIP_THREADGROUP_WIDTH, COMPRESS_ONE_MIP_THREADGROUP_WIDTH, 1)]
void main(
    uint2 threadIDWithinDispatch : SV_DispatchThreadID)
{
    float blockU[16], blockV[16];
    LoadTexelsUV(g_texIn, g_samp, g_oneOverTextureWidth, threadIDWithinDispatch, blockU, blockV);

    g_texMip0Out[threadIDWithinDispatch] = CompressBC5Block(blockU, blockV);
}
