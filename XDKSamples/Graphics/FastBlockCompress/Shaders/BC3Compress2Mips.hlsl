//--------------------------------------------------------------------------------------
// BC3Compress2Mips.hlsl
//
// Fast block compression ComputeShader for BC3 (two mips at once via LDS)
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "BlockCompress.hlsli"

Texture2D g_texIn : register(t0);

RWTexture2D<uint4> g_texMip0Out : register(u0);    // Output BC3 texture as R32G32B32A32_UINT

SamplerState g_samp : register(s0);

RWTexture2D<uint4> g_texMip1Out : register(u1); 

groupshared float3 gs_mip1BlocksRGB[ MIP1_BLOCKS_PER_ROW * MIP1_BLOCKS_PER_ROW ][16];
groupshared float gs_mip1BlocksA[ MIP1_BLOCKS_PER_ROW * MIP1_BLOCKS_PER_ROW ][16];

//--------------------------------------------------------------------------------------
// Calculate the texels for mip level n+1
//--------------------------------------------------------------------------------------
void DownsampleMip(uint2 threadIDWithinGroup, float3 blockRGB[16], float blockA[16])
{
    // Find the block and texel index for this thread within the group
    uint2 blockID = threadIDWithinGroup.xy / 2;
    uint2 texelID = 2 * (threadIDWithinGroup.xy - 2 * blockID);
    uint blockIndex = blockID.y * MIP1_BLOCKS_PER_ROW + blockID.x;
    uint texelIndex = texelID.y * 4 + texelID.x;  // A block is 4x4 texels

    // We average the colors later by passing a scale value into CompressBC3Block. This allows
    //  us to avoid scaling all 16 colors in the block: we really only need to scale the min
    //  and max values.
    gs_mip1BlocksRGB[blockIndex][texelIndex] = blockRGB[0] + blockRGB[1] + blockRGB[4] + blockRGB[5];
    gs_mip1BlocksRGB[blockIndex][texelIndex + 1] = blockRGB[2] + blockRGB[3] + blockRGB[6] + blockRGB[7];
    gs_mip1BlocksRGB[blockIndex][texelIndex + 4] = blockRGB[8] + blockRGB[9] + blockRGB[12] + blockRGB[13];
    gs_mip1BlocksRGB[blockIndex][texelIndex + 5] = blockRGB[10] + blockRGB[11] + blockRGB[14] + blockRGB[15];
    gs_mip1BlocksA[blockIndex][texelIndex] = blockA[0] + blockA[1] + blockA[4] + blockA[5];
    gs_mip1BlocksA[blockIndex][texelIndex + 1] = blockA[2] + blockA[3] + blockA[6] + blockA[7];
    gs_mip1BlocksA[blockIndex][texelIndex + 4] = blockA[8] + blockA[9] + blockA[12] + blockA[13];
    gs_mip1BlocksA[blockIndex][texelIndex + 5] = blockA[10] + blockA[11] + blockA[14] + blockA[15];
}


//--------------------------------------------------------------------------------------
// Compress two mip levels at once by downsampling into LDS
//--------------------------------------------------------------------------------------
[RootSignature(BlockCompressRS)]
[numthreads(COMPRESS_TWO_MIPS_THREADGROUP_WIDTH, COMPRESS_TWO_MIPS_THREADGROUP_WIDTH, 1)]
void main(
    uint2 threadIDWithinDispatch : SV_DispatchThreadID,
    uint2 threadIDWithinGroup : SV_GroupThreadID,
    uint threadIndexWithinGroup : SV_GroupIndex,
    uint2 groupIDWithinDispatch : SV_GroupID)
{
    // Load the texels in our block
    float3 blockRGB[16];
    float blockA[16];
    LoadTexelsRGBA(g_texIn, threadIDWithinDispatch, blockRGB, blockA);

    // Downsample from mip 0 to mip 1
    DownsampleMip(threadIDWithinGroup, blockRGB, blockA);
    GroupMemoryBarrierWithGroupSync();

    g_texMip0Out[threadIDWithinDispatch.xy] = CompressBC3Block(blockRGB, blockA);

    // When compressing two mips at a time, we use a group size of 16x16. This produces four 64-thread shader vectors.
    // The first shader vector will execute the code below and the other three will retire.
    if (threadIndexWithinGroup < MIP1_BLOCKS_PER_ROW * MIP1_BLOCKS_PER_ROW)
    {
        uint2 texelID = uint2(threadIndexWithinGroup % MIP1_BLOCKS_PER_ROW, threadIndexWithinGroup / MIP1_BLOCKS_PER_ROW);

        // Pass a scale value of 0.25 to CompressBC3 block to average the four source values contributing to each pixel
        //  in the block. See the comment in DownsampleMip, above.
        uint4 compressed = CompressBC3Block(gs_mip1BlocksRGB[threadIndexWithinGroup], gs_mip1BlocksA[threadIndexWithinGroup], 0.25f);
        g_texMip1Out[groupIDWithinDispatch.xy * MIP1_BLOCKS_PER_ROW + texelID] = compressed;
    }
}
