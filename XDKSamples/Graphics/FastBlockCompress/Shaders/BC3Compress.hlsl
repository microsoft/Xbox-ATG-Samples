//--------------------------------------------------------------------------------------
// BC3Compress.hlsl
//
// Fast block compression ComputeShader for BC3
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "BlockCompress.hlsli"

Texture2D g_texIn : register(t0);

RWTexture2D<uint4> g_texMip0Out : register(u0);    // Output BC3 texture as R32G32B32A32_UINT

SamplerState g_samp : register(s0);

//--------------------------------------------------------------------------------------
// Compress one mip level
//--------------------------------------------------------------------------------------
[RootSignature(BlockCompressRS)]
[numthreads(COMPRESS_ONE_MIP_THREADGROUP_WIDTH, COMPRESS_ONE_MIP_THREADGROUP_WIDTH, 1)]
void main(
    uint2 threadIDWithinDispatch : SV_DispatchThreadID)
{
    float3 blockRGB[16];
    float blockA[16];
    LoadTexelsRGBA(g_texIn, threadIDWithinDispatch, blockRGB, blockA);

    g_texMip0Out[threadIDWithinDispatch.xy] = CompressBC3Block(blockRGB, blockA, 1.0f);
}
