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
    float4 position     : SV_Position;
    float4 color        : COLOR0;
};

struct Interpolants
{
    float4 position     : SV_Position;
    float4 color        : COLOR0;
};

Interpolants main( Vertex In )
{
    return In;
}