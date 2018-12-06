#define HLSL
#include "GlobalSharedHlslCompat.h"
#include "SSAOHlslCompat.h"

Texture2D<float> DS4x : register(t0);
Texture2D<float3> Normal4x : register(t1);

RWTexture2D<float> DS8x : register(u0);
RWTexture2DArray<float> DS8xAtlas : register(u1);
RWTexture2D<float> DS16x : register(u2);
RWTexture2DArray<float> DS16xAtlas : register(u3);
RWTexture2D<float3> Normals8x : register(u4);
RWTexture2DArray<float3> Normals8xAtlas : register(u5);
RWTexture2D<float3> Normals16x : register(u6);
RWTexture2DArray<float3> Normals16xAtlas : register(u7);

[numthreads(8, 8, 1)]
void main(uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    float m1 = DS4x[DTid.xy << 1];
    float3 n1 = Normal4x[DTid.xy << 1];

    uint2 st = DTid.xy;
    uint2 stAtlas = st >> 2;
    uint stSlice = (st.x & 3) | ((st.y & 3) << 2);
    DS8x[st] = m1;
    DS8xAtlas[uint3(stAtlas, stSlice)] = m1;
    Normals8x[st] = n1;
    Normals8xAtlas[uint3(stAtlas, stSlice)] = n1;

    if ((GI & 011) == 0)
    {
        uint2 st = DTid.xy >> 1;
        uint2 stAtlas = st >> 2;
        uint stSlice = (st.x & 3) | ((st.y & 3) << 2);
        DS16x[st] = m1;
        DS16xAtlas[uint3(stAtlas, stSlice)] = m1;
        Normals16x[st] = n1;
        Normals16xAtlas[uint3(stAtlas, stSlice)] = n1;
    }
}