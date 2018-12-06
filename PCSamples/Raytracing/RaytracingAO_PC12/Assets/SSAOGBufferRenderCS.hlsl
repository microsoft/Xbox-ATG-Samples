#define HLSL

#include "GlobalSharedHlslCompat.h"
#include "SSAOHlslCompat.h"

Texture2D<float> SsaoBuffer : register(t0);
Texture2DArray<float4> GBuffer : register(t1);
Texture2D<float> Depth : register(t2);

RWTexture2D<float3> OutColor : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (Depth[DTid.xy] != 1.0)
        OutColor[DTid.xy] = GBuffer[uint3(DTid.xy, 1)].xyz * SsaoBuffer[DTid.xy];
    else
        OutColor[DTid.xy] = BACKGROUND.xyz;
}