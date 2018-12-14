//--------------------------------------------------------------------------------------
// RMSReduce.hlsl
//
// Reduce the result of an RMS Error calculation
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RMS.hlsli"

#define TEXELS_PER_GROUP (RMS_THREADGROUP_WIDTH * 2)
#define REDUCTION_FACTOR 4

RWStructuredBuffer<float2> g_buffInput : register(u0);
RWStructuredBuffer<float2> g_buffOutput : register(u1);

// Squared error from each thread
groupshared float2 gs_sqError[64];

//--------------------------------------------------------------------------------------
// Compute shader entry point. Each thread reads two pixels, so a single 
// threadgroup reads 128 buffer elements along X
//--------------------------------------------------------------------------------------
[RootSignature(RMSRS)]
[numthreads(RMS_THREADGROUP_WIDTH, 1, 1)]
void main(
    uint threadIDWithinDispatch : SV_DispatchThreadID,
    uint groupIDWithinDispatch : SV_GroupID,
    uint threadIndexWithinGroup : SV_GroupIndex)
{
    // Combine the values from two adjacent buffer elements
    uint location = threadIDWithinDispatch * 2;
    float2 sqError = g_buffInput[location] + g_buffInput[location + 1];
    gs_sqError[threadIndexWithinGroup] = sqError;

    // No synchronization needed because our threadgroup is the size of a wavefront

    // Each even thread combines two elements from local store and writes them to the output buffer
    if (!(threadIndexWithinGroup & 1))
    {
        sqError += gs_sqError[threadIndexWithinGroup + 1];

        uint outputIndex = groupIDWithinDispatch * (TEXELS_PER_GROUP / REDUCTION_FACTOR) + threadIndexWithinGroup / 2;
        g_buffOutput[outputIndex] = sqError;
    }
}
