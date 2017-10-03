//--------------------------------------------------------------------------------------
// VertexShaders.hlsli
//
// Vertex Shaders for the Dolphin sample
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
struct VSOUT
{
    float4 vPosition    : SV_POSITION;
    float4 vLightAndFog : COLOR0_center;		// COLOR0.x = light, COLOR0.y = fog
    float4 vTexCoords   : TEXCOORD1;			// TEXCOORD0.xy = basetex, TEXCOORD0.zw = caustictex
};


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------

cbuffer cb0
{
    uniform float4   g_vZero             : register(c0);  // ( 0, 0, 0, 0 )
    uniform float4   g_vConstants        : register(c1);  // ( 1, 0.5, -, - )
    uniform float3   g_vBlendWeights     : register(c2);  // ( fWeight1, fWeight2, fWeight3, 0 )
    uniform float4x4 g_matWorldViewProj  : register(c4);  // world-view-projection matrix
    uniform float4x4 g_matWorldView      : register(c8);  // world-view matrix
    uniform float4x4 g_matView           : register(c12); // view matrix
    uniform float4x4 g_matProjection     : register(c16); // projection matrix
    uniform float3   g_vSeafloorLightDir : register(c20); // seafloor light direction
    uniform float3   g_vDolphinLightDir  : register(c21); // dolphin light direction
    uniform float4   g_vDiffuse			 : register(c23);
    uniform float4   g_vAmbient          : register(c24);
    uniform float4   g_vFogRange         : register(c22); // ( x, fog_end, (1/(fog_end-fog_start)), x)
    uniform float4   g_vTexGen           : register(c25);
}

//--------------------------------------------------------------------------------------
// Name: ShadeSeaFloorVertex()
// Desc: Vertex shader for the seafloor
//--------------------------------------------------------------------------------------
VSOUT ShadeSeaFloorVertex(const float3 vPosition : SV_POSITION,
    const float3 vNormal : NORMAL,
    const float2 vBaseTexCoords : TEXCOORD0)
{
    // Transform to view space (world matrix is identity)
    float4 vViewPosition = mul(float4(vPosition, 1.0f), g_matView);

    // Transform to projection space
    float4 vOutputPosition = mul(vViewPosition, g_matProjection);

    // Lighting calculation
    float fLightValue = max(dot(vNormal, g_vSeafloorLightDir), g_vZero.x);

    // Generate water caustic tex coords from vertex xz position
    float2 vCausticTexCoords = g_vTexGen.xx * vViewPosition.xz + g_vTexGen.zw;

    // Fog calculation:
    float fFogValue = clamp((g_vFogRange.y - vViewPosition.z) * g_vFogRange.z, g_vZero.x, g_vConstants.x);

    // Compress output values
    VSOUT  Output;
    Output.vPosition = vOutputPosition;
    Output.vLightAndFog.x = fLightValue;
    Output.vLightAndFog.y = fFogValue;
    Output.vTexCoords.xy = 10 * vBaseTexCoords;
    Output.vTexCoords.zw = vCausticTexCoords;

    Output.vLightAndFog.z = 0.0f;
    Output.vLightAndFog.w = 0.0f;

    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeDolphinVertex()
// Desc: Vertex shader for the dolphin
//--------------------------------------------------------------------------------------
VSOUT ShadeDolphinVertex(const float3 vPosition0 : POSITION0,
    const float3 vPosition1 : POSITION1,
    const float3 vPosition2 : POSITION2,
    const float3 vNormal0 : NORMAL0,
    const float3 vNormal1 : NORMAL1,
    const float3 vNormal2 : NORMAL2,
    const float2 vBaseTexCoords : TEXCOORD0)
{
    // Tween the 3 positions (v0,v1,v2) into one position
    float4 vModelPosition = float4(vPosition0 * g_vBlendWeights.x + vPosition1 * g_vBlendWeights.y + vPosition2 * g_vBlendWeights.z, 1.0f);

    // Transform position to the clipping space
    float4 vOutputPosition = mul(vModelPosition, g_matWorldViewProj);

    // Transform position to the camera space
    float4 vViewPosition = mul(vModelPosition, g_matWorldView);

    // Tween the 3 normals (v3,v4,v5) into one normal
    float3 vModelNormal = vNormal0 * g_vBlendWeights.x + vNormal1 * g_vBlendWeights.y + vNormal2 * g_vBlendWeights.z;

    // Do the lighting calculation
    float fLightValue = max(dot(vModelNormal, g_vDolphinLightDir), g_vZero.x);

    // Generate water caustic tex coords from vertex xz position
    float2 vCausticTexCoords = g_vConstants.yy * vViewPosition.xz;

    // Fog calculation:
    float fFogValue = clamp((g_vFogRange.y - vViewPosition.z) * g_vFogRange.z, g_vZero.x, g_vConstants.x);

    // Compress output values
    VSOUT  Output;
    Output.vPosition = vOutputPosition;
    Output.vLightAndFog.x = fLightValue;
    Output.vLightAndFog.y = fFogValue;
    Output.vTexCoords.xy = vBaseTexCoords;
    Output.vTexCoords.zw = vCausticTexCoords;

    Output.vLightAndFog.z = 0.0f;
    Output.vLightAndFog.w = 1.0f;

    return Output;
}


