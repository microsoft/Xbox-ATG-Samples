//--------------------------------------------------------------------------------------
// ColorPS.hlsl
//
// A custom pixel shader for SpriteBatch that only outputs color 
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RootSig.fxh"

[RootSignature(SpriteStaticRS)]
float4 main(float4 color : COLOR) : SV_Target0
{
    return color;
}