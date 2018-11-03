//--------------------------------------------------------------------------------------
// QuadPointVS.hlsl
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Bokeh.hlsli"

// the runtime requires a vertex shader at all times, and we don't need it to render point sprites, so we have to use an empty VS
[RootSignature(BokehRS)]
void main()
{
}
