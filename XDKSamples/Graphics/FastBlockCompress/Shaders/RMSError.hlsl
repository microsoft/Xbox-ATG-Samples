//--------------------------------------------------------------------------------------
// RMSError.hlsl
//
// Compute the RMS error between two textures and start the reduction
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RMS.hlsli"

#define TEXELS_PER_GROUP (RMS_THREADGROUP_WIDTH * 2)
#define REDUCTION_FACTOR 4

Texture2D g_texA : register(t0);
Texture2D g_texB : register(t1);
RWStructuredBuffer<float2> g_buffReduce : register(u0);

// Squared error from each thread: (RGB, A)
groupshared float2 gs_sqError[64];

// Reconstruct the Z component of a normal map
float ReconstructZ(float2 v)
{
    v = v * 2 - 1;
    float z = sqrt(max(0, 1 - dot(v, v)));

    return 0.5f*(z + 1);
}

//--------------------------------------------------------------------------------------
// Compute shader entry point. Each thread reads two pixels, so a single 
// threadgroup reads 128 texels along X
//--------------------------------------------------------------------------------------
[RootSignature(RMSRS)]
[numthreads(RMS_THREADGROUP_WIDTH, 1, 1)]
void main(
    uint2 threadIDWithinDispatch : SV_DispatchThreadID,
    uint2 groupIDWithinDispatch : SV_GroupID,
    uint threadIndexWithinGroup : SV_GroupIndex)
{
    // Load the values from two adjacent texels
    uint3 location = int3(threadIDWithinDispatch.x * 2, threadIDWithinDispatch.y, g_mipLevel);
    float4 error[2];

    float4 a = g_texA.Load(location);
    float4 b = g_texB.Load(location);
    if (g_reconstructZA) a.z = ReconstructZ(a.xy);
    if (g_reconstructZB) b.z = ReconstructZ(b.xy);
    error[0] = 255.0f * (a - b);

    a = g_texA.Load(location, int2(1, 0));
    b = g_texB.Load(location, int2(1, 0));
    if (g_reconstructZA) a.z = ReconstructZ(a.xy);
    if (g_reconstructZB) b.z = ReconstructZ(b.xy);
    error[1] = 255.0f * (a - b);

    float rgbError = dot(error[0].rgb, error[0].rgb) + dot(error[1].rgb, error[1].rgb);
    float alphaError = error[0].a*error[0].a + error[1].a*error[1].a;
    float2 sqError = float2(rgbError, alphaError);
    gs_sqError[threadIndexWithinGroup] = sqError;

    // No synchronization needed because our threadgroup is the size of a wavefront

    // Now we take the computed error values in the local store and downsample them, converting
    //  from a 2D texture into a flat list that will be fed into the reduction stage.
    if (!(threadIndexWithinGroup & 1)         // Each even thread (threads 0, 2, 4, ...) combines two values from local store
        && location.x < g_textureWidth)      // We need to bounds check because we are writing into a 1D buffer
    {
        sqError += gs_sqError[threadIndexWithinGroup + 1];

        uint outputIndex = groupIDWithinDispatch.y * (g_textureWidth / REDUCTION_FACTOR)
            + groupIDWithinDispatch.x*(TEXELS_PER_GROUP / REDUCTION_FACTOR)
            + threadIndexWithinGroup / 2;
        g_buffReduce[outputIndex] = sqError;
    }
}
