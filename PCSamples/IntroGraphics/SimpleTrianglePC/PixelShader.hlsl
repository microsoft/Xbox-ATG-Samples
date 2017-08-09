//------------------------------------------------------------------------------------
// PixelShader.hlsl
//
// Simple shader to render a triangle
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

struct Interpolants
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

struct Pixel
{
    float4 color    : SV_Target;
};

Pixel main( Interpolants In )
{
    Pixel Out;
    Out.color = In.color;
    return Out;
}