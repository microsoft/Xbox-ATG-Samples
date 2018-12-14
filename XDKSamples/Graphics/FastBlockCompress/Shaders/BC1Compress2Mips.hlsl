//--------------------------------------------------------------------------------------
// BC1Compress2Mips.hlsl
//
// Fast block compression ComputeShader for BC1 (two mips at once via LDS)
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "BlockCompress.hlsli"

Texture2D g_texIn : register(t0);

RWTexture2D<uint2> g_texMip0Out : register(u0);    // Output BC1 texture as R32G32_UINT 

SamplerState g_samp : register(s0);

RWTexture2D<uint2> g_texMip1Out : register(u1);   

groupshared float3 gs_mip1Blocks[MIP1_BLOCKS_PER_ROW * MIP1_BLOCKS_PER_ROW][16];

//--------------------------------------------------------------------------------------
// Calculate the texels for mip level n+1
//--------------------------------------------------------------------------------------
void DownsampleMip(uint2 threadIDWithinGroup, float3 block[16])
{
    // Find the block and texel index for this thread within the group
    uint2 blockID = threadIDWithinGroup / 2;
    uint2 texelID = 2 * (threadIDWithinGroup - 2 * blockID);
    uint blockIndex = blockID.y * MIP1_BLOCKS_PER_ROW + blockID.x;
    uint texelIndex = texelID.y * 4 + texelID.x;  // A block is 4x4 texels

    // We average the colors later by passing a scale value into CompressBC1Block. This allows
    //  us to avoid scaling all 16 colors in the block: we really only need to scale the min
    //  and max values.
    gs_mip1Blocks[blockIndex][texelIndex] = block[0] + block[1] + block[4] + block[5];
    gs_mip1Blocks[blockIndex][texelIndex + 1] = block[2] + block[3] + block[6] + block[7];
    gs_mip1Blocks[blockIndex][texelIndex + 4] = block[8] + block[9] + block[12] + block[13];
    gs_mip1Blocks[blockIndex][texelIndex + 5] = block[10] + block[11] + block[14] + block[15];
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
    // Load the texels in our mip 0 block
    float3 block[16];
    LoadTexelsRGB(g_texIn, g_samp, g_oneOverTextureWidth, threadIDWithinDispatch, block);

    // Downsample from mip 0 to mip 1
    DownsampleMip(threadIDWithinGroup, block);
    GroupMemoryBarrierWithGroupSync();

    g_texMip0Out[threadIDWithinDispatch] = CompressBC1Block(block, 1.0f);

    // When compressing two mips at a time, we use a group size of 16x16. This produces four 64-thread wavefronts.
    // The first wavefronts will execute the code below and the other three will retire.
    if (threadIndexWithinGroup < MIP1_BLOCKS_PER_ROW * MIP1_BLOCKS_PER_ROW)
    {
        uint2 texelID = uint2(threadIndexWithinGroup % MIP1_BLOCKS_PER_ROW, threadIndexWithinGroup / MIP1_BLOCKS_PER_ROW);

        // Pass a scale value of 0.25 to CompressBC1 block to average the four source values contributing to each pixel
        //  in the block. See the comment in DownsampleMip, above.
        g_texMip1Out[groupIDWithinDispatch * MIP1_BLOCKS_PER_ROW + texelID] = CompressBC1Block(gs_mip1Blocks[threadIndexWithinGroup], 0.25f);
    }
}
