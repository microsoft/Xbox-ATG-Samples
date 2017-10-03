//--------------------------------------------------------------------------------------
// VertexShader.hlsl
//
// Simple vertex shader for rendering a textured quad
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

struct Vertex
{
    float4 position     : SV_Position;
    float2 texcoord     : TEXCOORD0;
};

struct Interpolants
{
    float4 position     : SV_Position;
    float2 texcoord     : TEXCOORD0;
};

#define MainRS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT" \
    "          | DENY_DOMAIN_SHADER_ROOT_ACCESS" \
    "          | DENY_GEOMETRY_SHADER_ROOT_ACCESS" \
    "          | DENY_HULL_SHADER_ROOT_ACCESS)," \
    "DescriptorTable (SRV(t0), visibility=SHADER_VISIBILITY_PIXEL),"\
    "StaticSampler(s0, visibility=SHADER_VISIBILITY_PIXEL)"

[RootSignature(MainRS)]
Interpolants main( Vertex In )
{
    return In;
}