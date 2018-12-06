#define HLSL

#include "GlobalSharedHlslCompat.h"
#include "GBufferH.hlsl"

GSInput main(VSInput input)
{
    GSInput result;
    result.position = mul(float4(input.position, 1.f), g_sceneCB.worldViewProjection);
    result.position.z = result.position.z;

    result.normal = input.normal;
    result.texcoord = input.texcoord;
    result.tangent = input.tangent;
    result.instance = input.instance;

    return result;
}