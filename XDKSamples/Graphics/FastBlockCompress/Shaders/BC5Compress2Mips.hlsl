//--------------------------------------------------------------------------------------
// BC5Compress2Mips.hlsl
//
// Fast block compression ComputeShader for BC5 (two mips at once via LDS)
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "BlockCompress.hlsli"

Texture2D g_texIn : register(t0);

RWTexture2D<uint4> g_texMip0Out : register(u0);    // Output BC5 texture as R32G32B32A32_UINT

SamplerState g_samp : register(s0);

RWTexture2D<uint4> g_texMip1Out : register(u1);

groupshared float gs_mip1BlocksU[ MIP1_BLOCKS_PER_ROW * MIP1_BLOCKS_PER_ROW ][16];
groupshared float gs_mip1BlocksV[ MIP1_BLOCKS_PER_ROW * MIP1_BLOCKS_PER_ROW ][16];

//--------------------------------------------------------------------------------------
// Calculate the texels for mip level n+1
//--------------------------------------------------------------------------------------
void DownsampleMip(uint2 threadIDWithinGroup, float blockU[16], float blockV[16])
{
    // Find the block and texel index for this thread within the group
    uint2 blockID = threadIDWithinGroup / 2;
    uint2 texelID = 2 * (threadIDWithinGroup - 2 * blockID);
    uint blockIndex = blockID.y * MIP1_BLOCKS_PER_ROW + blockID.x;
    uint texelIndex = texelID.y * 4 + texelID.x;  // A block is 4x4 texels

    // We average the colors later by passing a scale value into CompressBC1Block. This allows
    //  us to avoid scaling all 16 colors in the block: we really only need to scale the min
    //  and max values.
    gs_mip1BlocksU[blockIndex][texelIndex] = blockU[0] + blockU[1] + blockU[4] + blockU[5];
    gs_mip1BlocksU[blockIndex][texelIndex + 1] = blockU[2] + blockU[3] + blockU[6] + blockU[7];
    gs_mip1BlocksU[blockIndex][texelIndex + 4] = blockU[8] + blockU[9] + blockU[12] + blockU[13];
    gs_mip1BlocksU[blockIndex][texelIndex + 5] = blockU[10] + blockU[11] + blockU[14] + blockU[15];
    gs_mip1BlocksV[blockIndex][texelIndex] = blockV[0] + blockV[1] + blockV[4] + blockV[5];
    gs_mip1BlocksV[blockIndex][texelIndex + 1] = blockV[2] + blockV[3] + blockV[6] + blockV[7];
    gs_mip1BlocksV[blockIndex][texelIndex + 4] = blockV[8] + blockV[9] + blockV[12] + blockV[13];
    gs_mip1BlocksV[blockIndex][texelIndex + 5] = blockV[10] + blockV[11] + blockV[14] + blockV[15];
}


//--------------------------------------------------------------------------------------
// Compress two mip levels at once by downsampling into LDS
//--------------------------------------------------------------------------------------
[RootSignature(BlockCompressRS)]
[numthreads( COMPRESS_TWO_MIPS_THREADGROUP_WIDTH, COMPRESS_TWO_MIPS_THREADGROUP_WIDTH, 1)]
void main(
    uint2 threadIDWithinDispatch : SV_DispatchThreadID,
    uint2 threadIDWithinGroup : SV_GroupThreadID,
    uint threadIndexWithinGroup : SV_GroupIndex,
    uint2 groupIDWithinDispatch : SV_GroupID)
{
    // Load the texels in our block
    float blockU[16], blockV[16];
    LoadTexelsUV(g_texIn, g_samp, g_oneOverTextureWidth, threadIDWithinDispatch, blockU, blockV);

    // Downsample from mip 0 to mip 1
    DownsampleMip(threadIDWithinGroup, blockU, blockV);
    GroupMemoryBarrierWithGroupSync();

    g_texMip0Out[threadIDWithinDispatch] = CompressBC5Block(blockU, blockV, 1.0f);

    // When compressing two mips at a time, we use a group size of 16x16. This produces four 64-thread shader vectors.
    // The first shader vector will execute the code below and the other three will retire.
    if (threadIndexWithinGroup < MIP1_BLOCKS_PER_ROW*MIP1_BLOCKS_PER_ROW)
    {
        uint2 texelID = uint2(threadIndexWithinGroup % MIP1_BLOCKS_PER_ROW, threadIndexWithinGroup / MIP1_BLOCKS_PER_ROW);

        // Pass a scale value of 0.25 to CompressBC5 block to average the four source values contributing to each pixel
        //  in the block. See the comment in DownsampleMip, above.
        uint4 compressed = CompressBC5Block(gs_mip1BlocksU[threadIndexWithinGroup], gs_mip1BlocksV[threadIndexWithinGroup], 0.25f);
        g_texMip1Out[groupIDWithinDispatch * MIP1_BLOCKS_PER_ROW + texelID] = compressed;
    }
}
