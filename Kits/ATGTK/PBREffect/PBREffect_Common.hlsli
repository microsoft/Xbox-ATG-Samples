//--------------------------------------------------------------------------------------
// PBREffect_Header.hslsi
//
// A physically based shader for forward rendering on DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef __PBREFFECT_HEADER_HLSLI__
#define __PBREFFECT_HEADER_HLSLI__

#define PBREffectRS \
"RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"            DENY_HULL_SHADER_ROOT_ACCESS )," \
"DescriptorTable ( SRV(t0) ),"\
"DescriptorTable ( SRV(t1) ),"\
"DescriptorTable ( SRV(t2) ),"\
"DescriptorTable ( SRV(t3) ),"\
"DescriptorTable ( SRV(t4) ),"\
"DescriptorTable ( Sampler(s0) ),"\
"DescriptorTable ( Sampler(s1) ),"\
"CBV(b0)"

Texture2D<float4> PBR_AlbedoTexture     : register(t0);
Texture2D<float3> PBR_NormalTexture     : register(t1);
Texture2D<float3> PBR_RMATexture        : register(t2);
TextureCube<float3> PBR_RadianceTexture   : register(t3);
TextureCube<float3> PBR_IrradianceTexture : register(t4);

sampler PBR_SurfaceSampler : register(s0);
sampler PBR_IBLSampler     : register(s1);

cbuffer PBR_Constants : register(b0)
{
    float3   PBR_EyePosition            : packoffset(c0);
    float4x4 PBR_World                  : packoffset(c1);
    float3x3 PBR_WorldInverseTranspose  : packoffset(c5);
    float4x4 PBR_WorldViewProj          : packoffset(c8);
    float4x4 PBR_PrevWorldViewProj      : packoffset(c12);

    float3 PBR_LightDirection[3]        : packoffset(c16);
    float3 PBR_LightColor[3]            : packoffset(c19);   // "Specular and diffuse light" in PBR
 
    float3 PBR_ConstantAlbedo           : packoffset(c22);   // Constant values if not a textured effect
    float  PBR_ConstantMetallic         : packoffset(c23.x);
    float  PBR_ConstantRoughness        : packoffset(c23.y);

    int PBR_NumRadianceMipLevels        : packoffset(c23.z);

    // Size of render target
    float PBR_TargetWidth               : packoffset(c23.w);
    float PBR_TargetHeight              : packoffset(c24.x);
};

// Vertex formats
struct VSInputNmTxTangent
{
    float4 Position : SV_Position;
    float3 Normal   : NORMAL;
    float4 Tangent  : TANGENT;
    float2 TexCoord : TEXCOORD0;
};

struct VSOutputPixelLightingTxTangent
{
    float2 TexCoord   : TEXCOORD0;
    float4 PositionWS : TEXCOORD1;
    float3 NormalWS   : TEXCOORD2;
    float3 TangentWS  : TEXCOORD3;
    float4 Diffuse    : COLOR0;
    float4 PositionPS : SV_Position;
};

struct PSInputPixelLightingTxTangent
{
    float2 TexCoord   : TEXCOORD0;
    float4 PositionWS : TEXCOORD1;
    float3 NormalWS   : TEXCOORD2;
    float3 TangentWS  : TEXCOORD3;
    float4 Diffuse    : COLOR0;
};


// Common vertex shader code
struct CommonVSOutputPixelLighting
{
    float4 Pos_ps;
    float3 Pos_ws;
    float3 Normal_ws;
};

struct VSOut_Velocity
{
    VSOutputPixelLightingTxTangent current;
    float4                         prevPosition : TEXCOORD4;
};

CommonVSOutputPixelLighting ComputeCommonVSOutputPixelLighting(float4 position, float3 normal, float4x4 world, float4x4 worldViewProj, float3x3 worldInverseT)
{
    CommonVSOutputPixelLighting vout;

    vout.Pos_ps = mul(position, worldViewProj);
    vout.Pos_ws = mul(position, world).xyz;
    vout.Normal_ws = normalize(mul(normal, worldInverseT));

    return vout;
}

// Common math code
#include "PBREffect_Math.hlsli"

// Apply Disney-style physically based rendering to a surface with:
//
// V, N:             Eye and surface normals
//
// numLights:        Number of directional lights.
//
// lightColor[]:     Color and intensity of directional light.
//
// lightDirection[]: Light direction.
float3 PBR_LightSurface(
    in float3 V, in float3 N,
    in int numLights, in float3 lightColor[3], in float3 lightDirection[3],
    in float3 albedo, in float roughness, in float metallic, in float ambientOcclusion)
{
    // Specular coefficiant - fixed reflectance value for non-metals
    static const float kSpecularCoefficient = 0.04;

    const float NdotV = saturate(dot(N, V));

    // Burley roughness bias
    const float alpha = roughness * roughness;    

    // Blend base colors
    const float3 c_diff = lerp(albedo, float3(0, 0, 0), metallic)       * ambientOcclusion;
    const float3 c_spec = lerp(kSpecularCoefficient, albedo, metallic)  * ambientOcclusion;

    // Output color
    float3 acc_color = 0;                         

    // Accumulate light values
    for (int i = 0; i < numLights; i++)
    {
        // light vector (to light)
        const float3 L = normalize(-lightDirection[i]);

        // Half vector
        const float3 H = normalize(L + V);

        // products
        const float NdotL = saturate(dot(N, L));
        const float LdotH = saturate(dot(L, H));
        const float NdotH = saturate(dot(N, H));

        // Diffuse & specular factors
        float diffuse_factor = Diffuse_Burley(NdotL, NdotV, LdotH, roughness);
        float3 specular      = Specular_BRDF(alpha, c_spec, NdotV, NdotL, LdotH, NdotH);

        // Directional light
        acc_color += NdotL * lightColor[i] * (((c_diff * diffuse_factor) + specular));
    }

    // Add diffuse irradiance
    float3 diffuse_env = Diffuse_IBL(N);
    acc_color += c_diff * diffuse_env;

    // Add specular radiance 
    float3 specular_env = Specular_IBL(N, V, roughness);
    acc_color += c_spec * specular_env;

    return acc_color;
}

#endif // __PBREFFECT_HEADER_HLSLI__
