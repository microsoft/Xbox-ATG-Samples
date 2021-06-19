//--------------------------------------------------------------------------------------
// FullScreenQuad.hlsli
//
// Common shader code to draw a full-screen quad
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#ifndef __FULLSCREENQUAD_HLSLI__
#define __FULLSCREENQUAD_HLSLI__

#define FullScreenQuadRS \
    "RootFlags ( DENY_VERTEX_SHADER_ROOT_ACCESS |" \
    "            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "            DENY_HULL_SHADER_ROOT_ACCESS )," \
    "CBV(b0, visibility = SHADER_VISIBILITY_PIXEL ), " \
    "DescriptorTable (SRV(t0), visibility = SHADER_VISIBILITY_PIXEL),"\
    "DescriptorTable (SRV(t1), visibility = SHADER_VISIBILITY_PIXEL),"\
    "StaticSampler(s0,"\
    "           filter =   FILTER_MIN_MAG_MIP_POINT,"\
    "           addressU = TEXTURE_ADDRESS_CLAMP,"\
    "           addressV = TEXTURE_ADDRESS_CLAMP,"\
    "           addressW = TEXTURE_ADDRESS_CLAMP,"\
    "           visibility = SHADER_VISIBILITY_PIXEL )"

SamplerState PointSampler : register(s0);
Texture2D<float4> Texture : register(t0);

struct Interpolators
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

#endif