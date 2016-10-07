//--------------------------------------------------------------------------------------
// ToneMapEffect_VS.hlsl
//
// A simple flimic tonemapping effect for DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "ToneMapEffect_Common.hlsli"

[RootSignature(ToneMapRS)]
void main(inout float2 texCoord : TEXCOORD0,
          inout float4 position : SV_Position)
{
}