//--------------------------------------------------------------------------------------
// SkyboxEffect_Common.hlsli
//
// A sky box rendering helper.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#ifndef __SKYBOXEFFECT_COMMON_HLSLI__
#define __SKYBOXEFFECT_COMMON_HLSLI__

#define SkyboxRS \
"RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"            DENY_HULL_SHADER_ROOT_ACCESS )," \
"DescriptorTable ( SRV(t0) ),"\
"DescriptorTable ( Sampler(s0) )," \
"CBV(b0)"

cbuffer Skybox_Constants : register(b0)
{
    float4x4 Skybox_WorldViewProj;
}

// Vertex formats
struct VSInputNmTxTangent
{
    float4 Position : SV_Position;
};

struct VSOutputPixelLightingTxTangent
{
    float3 TexCoord   : TEXCOORD0;
    float4 PositionPS : SV_Position;
};

struct PSInputPixelLightingTxTangent
{
    float3 TexCoord   : TEXCOORD0;
};


#endif