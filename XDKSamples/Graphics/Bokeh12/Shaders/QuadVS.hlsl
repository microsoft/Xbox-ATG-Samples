//--------------------------------------------------------------------------------------
// QuadVS.hlsl
//
// Demonstrates how to render depth of field using point sprites.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Bokeh.hlsli"

static float4 s_positions[3] =
{
    float4(-1.0f, 1.0f, 0.0f, 1.0f),
    float4(3.0f,  1.0f, 0.0f, 1.0f),
    float4(-1.0f, -3.0f, 0.0f, 1.0f),
};

static float2 s_texcoords[3] =
{
    float2(0.0f, 0.0f),
    float2(2.0f, 0.0f),
    float2(0.0f, 2.0f),
};

// output a quad
[RootSignature(BokehRS)]
PSSceneIn main(uint index : SV_VertexID)
{
    PSSceneIn output;
    output.pos = s_positions[index % 3];
    output.tex = s_texcoords[index % 3];

    return output;
}