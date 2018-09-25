//--------------------------------------------------------------------------------------
// VertexShader.hlsl
//
// Simple vertex shader for rendering a triangle
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

struct Vertex
{
    float3 position     : SV_Position;
    float4 color        : COLOR0;
};

struct Interpolants
{
    float4 position     : SV_Position;
    float4 color        : COLOR0;
};

[RootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)")]
Interpolants main( Vertex In )
{
    Interpolants Out;
    Out.position = float4(In.position, 1.0f);
    Out.color = In.color;
    return Out;
}