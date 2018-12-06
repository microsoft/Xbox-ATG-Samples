#define HLSL

#include "GlobalSharedHlslCompat.h"
#include "GBufferH.hlsl"

float4 main(GSOutput input) : SV_TARGET
{
    switch (input.instance)
    {
    case 0:

        float3 normal;

        if (g_material.isNormalTexture)
        {
            normal = g_normal.Sample(samplerLinearCLamp, input.texcoord).xyz * 2.f - 1.f;
            float3 bitangent = cross(input.normal, input.tangent);
            float3x3 tbn = float3x3(input.tangent, bitangent, input.normal);
            normal = normalize(mul(normal, tbn));
        }
        else
            normal = input.normal;

        return float4(saturate((normal * 0.5) + 0.5), 0.f);

        break;
    default:

        if (g_material.isDiffuseTexture)
            return g_diffuse.Sample(samplerLinearCLamp, input.texcoord);
        else
            return float4(g_material.diffuse, 1);

        break;
    }
}