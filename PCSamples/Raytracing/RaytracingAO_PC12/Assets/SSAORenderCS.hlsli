#define HLSL

#include "GlobalSharedHlslCompat.h"
#include "SSAOHlslCompat.h"

#ifndef INTERLEAVE_RESULT
#define WIDE_SAMPLING 1
#endif

#ifdef INTERLEAVE_RESULT
Texture2DArray<float> DepthTex : register(t0);
Texture2DArray<float3> NormalTex : register(t1);
#else
Texture2D<float> DepthTex : register(t0);
Texture2D<float3> NormalTex : register(t1);
#endif

RWTexture2D<float> Occlusion : register(u0);
SamplerState PointSamplerClamp : register(s3);

ConstantBuffer<SceneConstantBuffer> SceneCB : register(b0);
ConstantBuffer<SSAORenderConstantBuffer> RenderCB : register(b1);

#if WIDE_SAMPLING
// 32x32 cache size:  the 16x16 in the center forms the area of focus with the 8-pixel perimeter used for wide gathering.
#define TILE_DIM 32
#define THREAD_COUNT_X 16
#define THREAD_COUNT_Y 16
#else
// 16x16 cache size:  the 8x8 in the center forms the area of focus with the 4-pixel perimeter used for gathering.
#define TILE_DIM 16
#define THREAD_COUNT_X 8
#define THREAD_COUNT_Y 8
#endif

groupshared float PositionsX[TILE_DIM * TILE_DIM];
groupshared float PositionsY[TILE_DIM * TILE_DIM];
groupshared float PositionsZ[TILE_DIM * TILE_DIM];

float3 GetPosLDS(uint index)
{
    return float3(
        PositionsX[index],
        PositionsY[index],
        PositionsZ[index]
        );
}

void StorePosLDS(uint index, float3 pos)
{
    PositionsX[index] = pos.x;
    PositionsY[index] = pos.y;
    PositionsZ[index] = pos.z;
}


float3 ComputePosition(float2 uv, float depth)
{
    float3 pos = SceneCB.frustumPoint.xyz;
    pos += uv.x * SceneCB.frustumHDelta.xyz;
    pos += uv.y * SceneCB.frustumVDelta.xyz;
    return (depth * pos) + SceneCB.cameraPosition.xyz;
}


float rsqrtFast(float x)
{
    return asfloat(0x5f375a84 - (asint(x) >> 1));
}


float ComputeNormalAO(float3 normal, float3 pos, float3 pos2)
{
    float3 dir = pos2 - pos;
    //	float invD	 = 1.0f / length(dir);
    float invD = rsqrtFast(dot(dir, dir));
    float ao = dot(normal, dir * invD);
#ifdef USE_NORMAL_HEMISPHERE
# ifdef ALLOW_BACK_PROJECTION
    ao += gNormalBackprojectionToleranceAdd;
    ao *= gNormalBackprojectionToleranceMul;
# endif
    ao = max(0.0, ao * invD);
#else
    ao *= invD;
#endif
    return ao;
}


float TestSamplePair(float3 normal, float3 pos, uint base, int offset)
{
    float normalAo1 = ComputeNormalAO(normal, pos, GetPosLDS(base + offset));
    float normalAo2 = ComputeNormalAO(normal, pos, GetPosLDS(base - offset));
    return 0.5f * (normalAo1 + normalAo2);
}


float TestSamples(float3 normal, float3 pos, uint centerIdx, uint x, uint y)
{
#if WIDE_SAMPLING
    x <<= 1;
    y <<= 1;
#endif

    if (y == 0)
    {
        // Axial
        float o1 = TestSamplePair(normal, pos, centerIdx, x);
        float o2 = TestSamplePair(normal, pos, centerIdx, x * TILE_DIM);

        return 0.5f * (o1 + o2);
    }
    else if (x == y)
    {
        // Diagonal
        float o1 = TestSamplePair(normal, pos, centerIdx, x * TILE_DIM - x);
        float o2 = TestSamplePair(normal, pos, centerIdx, x * TILE_DIM + x);

        return 0.5f * (o1 + o2);
    }
    else
    {
        // L-Shaped
        float o1 = TestSamplePair(normal, pos, centerIdx, y * TILE_DIM + x);
        float o2 = TestSamplePair(normal, pos, centerIdx, y * TILE_DIM - x);
        float o3 = TestSamplePair(normal, pos, centerIdx, x * TILE_DIM + y);
        float o4 = TestSamplePair(normal, pos, centerIdx, x * TILE_DIM - y);

        return 0.25f * (o1 + o2 + o3 + o4);
    }
}

#if WIDE_SAMPLING
[numthreads(16, 16, 1)]
#else
[numthreads(8, 8, 1)]
#endif
void main(uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
#ifdef INTERLEAVE_RESULT
    float3 normal = NormalTex[DTid.xyz];
    float depth = DepthTex[DTid.xyz];
#else
    float3 normal = NormalTex[DTid.xy];
    float depth = DepthTex[DTid.xy];
#endif
    // Fetch four depths
#ifdef INTERLEAVE_RESULT
    float2 QuadCenterUV = (DTid.xy + GTid.xy - 3) * RenderCB.invSliceDimension;
    float4 depths = DepthTex.Gather(PointSamplerClamp, float3(QuadCenterUV, DTid.z));
#else
    float2 QuadCenterUV = (DTid.xy + GTid.xy - 7) * RenderCB.invSliceDimension;
    float4 depths = DepthTex.Gather(PointSamplerClamp, QuadCenterUV);
#endif
    // populate LDS
    // optimise position computation, would need to add on camera position to get real world space coordinates however 
    // we don't need world space this instead uses just world orientated, localised around the camera, slightly cheaper.
    float3 dx1 = SceneCB.frustumPoint.xyz + (QuadCenterUV.x * SceneCB.frustumHDelta.xyz);
    float3 dx2 = dx1 + (RenderCB.invSliceDimension.x * SceneCB.frustumHDelta.xyz);
    float3 dy1 = QuadCenterUV.y * SceneCB.frustumVDelta.xyz;
    float3 dy2 = dy1 + (RenderCB.invSliceDimension.y * SceneCB.frustumVDelta.xyz);

    int destIdx = GTid.x * 2 + GTid.y * 2 * TILE_DIM;

    StorePosLDS(destIdx, (dx1 + dy1) * depths.w);
    StorePosLDS(destIdx + 1, (dx2 + dy1) * depths.z);
    StorePosLDS(destIdx + TILE_DIM, (dx1 + dy2) * depths.x);
    StorePosLDS(destIdx + TILE_DIM + 1, (dx2 + dy2) * depths.y);

#if WIDE_SAMPLING
    uint thisIdx = GTid.x + GTid.y * TILE_DIM + 8 * TILE_DIM + 8;
#else
    uint thisIdx = GTid.x + GTid.y * TILE_DIM + 4 * TILE_DIM + 4;
#endif
    normal = (2.0f * normal) - 1.0f;
    float ao = 1.0f;

    GroupMemoryBarrierWithGroupSync();

    [branch] if (depth < 1.0)
    {
        float3 pos = GetPosLDS(thisIdx);

#ifdef SAMPLE_EXHAUSTIVELY
        // 68 sample all cells in *within* a circular radius of 5
        ao = gSampleWeightTable[0].x * TestSamples(normal, pos, thisIdx, 1, 0);
        ao += gSampleWeightTable[0].y * TestSamples(normal, pos, thisIdx, 2, 0);
        ao += gSampleWeightTable[0].z * TestSamples(normal, pos, thisIdx, 3, 0);
        ao += gSampleWeightTable[0].w * TestSamples(normal, pos, thisIdx, 4, 0);
        ao += gSampleWeightTable[1].x * TestSamples(normal, pos, thisIdx, 1, 1);
        ao += gSampleWeightTable[2].x * TestSamples(normal, pos, thisIdx, 2, 2);
        ao += gSampleWeightTable[2].w * TestSamples(normal, pos, thisIdx, 3, 3);
        ao += gSampleWeightTable[1].y * TestSamples(normal, pos, thisIdx, 1, 2);
        ao += gSampleWeightTable[1].z * TestSamples(normal, pos, thisIdx, 1, 3);
        ao += gSampleWeightTable[1].w * TestSamples(normal, pos, thisIdx, 1, 4);
        ao += gSampleWeightTable[2].y * TestSamples(normal, pos, thisIdx, 2, 3);
        ao += gSampleWeightTable[2].z * TestSamples(normal, pos, thisIdx, 2, 4);
#else
        // 36 sample every-other cell in a checker board pattern
        ao = RenderCB.sampleWeightTable[0].y * TestSamples(normal, pos, thisIdx, 2, 0);
        ao += RenderCB.sampleWeightTable[0].w * TestSamples(normal, pos, thisIdx, 4, 0);
        ao += RenderCB.sampleWeightTable[1].x * TestSamples(normal, pos, thisIdx, 1, 1);
        ao += RenderCB.sampleWeightTable[2].x * TestSamples(normal, pos, thisIdx, 2, 2);
        ao += RenderCB.sampleWeightTable[2].w * TestSamples(normal, pos, thisIdx, 3, 3);
        ao += RenderCB.sampleWeightTable[1].z * TestSamples(normal, pos, thisIdx, 1, 3);
        ao += RenderCB.sampleWeightTable[2].z * TestSamples(normal, pos, thisIdx, 2, 4);
#endif
#ifdef INTERLEAVE_RESULT
        ao *= 2.0f;
#endif
        ao = 1.0f - (RenderCB.normalMultiply * ao * depth);
        ao *= ao;
    }
#ifdef INTERLEAVE_RESULT
    uint2 OutPixel = DTid.xy << 2 | uint2(DTid.z & 3, DTid.z >> 2);
#else
    uint2 OutPixel = DTid.xy;
#endif
    Occlusion[OutPixel] = ao;
}