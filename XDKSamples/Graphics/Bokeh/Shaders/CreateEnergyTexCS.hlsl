//--------------------------------------------------------------------------------------
// CreateEnergyTexCS.hlsl
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "..\shadersettings.h"

Texture2D<float4> g_irisTex : register(t0);
SamplerState g_bilinearSampler : register(s0);

RWTexture3D<float> g_deviceMem : register(u0); // 8x8xN float buffer
RWTexture1D<float> g_energyTex : register(u1); // N float buffer

groupshared float s_groupMem[64];

float Load(uint2 id) 
{
    int index = id.y * 8 + id.x;
    return s_groupMem[index]; 
}

void Store(uint2 id, float value)
{
    s_groupMem[id.y * 8 + id.x] = value;
}

void GroupReduceSum(uint3 GroupThreadId)
{
    for (int i = 1; all(GroupThreadId.xy < (0x8 >> i)); ++i)
    {
        GroupMemoryBarrier();

        uint offset = 1 << (i - 1);
        uint2 xy = GroupThreadId.xy * (1 << i);

        float v0 = Load(xy + uint2(0, 0) * offset);
        float v1 = Load(xy + uint2(1, 0) * offset);
        float v2 = Load(xy + uint2(0, 1) * offset);
        float v3 = Load(xy + uint2(1, 1) * offset);

        float fSum = v0 + v1 + v2 + v3;

        Store(xy, fSum);
    }
}

[numthreads(8, 8, 1)]
void main(  uint3 GroupId : SV_GroupID,
            uint GroupIndex : SV_GroupIndex,
            uint3 GroupThreadId : SV_GroupThreadID,
            uint3 DispatchThreadId : SV_DispatchThreadID)
{
    uint slice = GroupId.z;

    // Compute the sample coordinates. 
    // This positions the iris at the center of the dispatch block with a cocRadius size (based on the z-component.)
    float cocRadius = (slice + 1) * 0.5f;
    float cocDiam = cocRadius * 2.0f;
    float cocCenter = NUM_RADII_WEIGHTS / 2;
    float bl = cocCenter - cocRadius;

    float2 sampleUV = ((DispatchThreadId.xy + 0.5f) - bl) / cocDiam;
    float level = (1.0f - cocRadius / cocCenter) * 5.0f;

    // Read and store the iris texture value into LDS memory.
    float value = g_irisTex.SampleLevel(g_bilinearSampler, sampleUV, level).x;
    Store(GroupThreadId.xy, value);

    // Reduce the memory by summation.
    GroupReduceSum(GroupThreadId);

    // Group Thread (0, 0) responsible for saving this out to device memory.
    if (all(GroupThreadId.xy == 0))
    {
        g_deviceMem[GroupId] = Load(GroupThreadId.xy);
    }

    // This problem has been reduced to an 8x8 sum which one group can handle. 
    // Release all other groups. Left with 1x1x64 groups.
    if (all(GroupId.xy != 0))
    {
        return;
    }

    // Copy the inter-group memory to LDS memory.
    DeviceMemoryBarrierWithGroupSync();
    Store(GroupThreadId.xy, g_deviceMem[uint3(GroupThreadId.xy, slice)]);

    // Reduce the memory by summation - same way as above.
    GroupReduceSum(GroupThreadId);

    // Group Thread (0, 0) responsible for calculating and saving the final energy value.
    if (all(GroupThreadId.xy == 0))
    {
        float fSum = Load(GroupThreadId.xy);
        float area = cocDiam * cocDiam;

        g_energyTex[slice] = area / fSum;
    }
}
