//--------------------------------------------------------------------------------------
// SimpleLighting.hlsl
//
// Shader demonstrating Lambertian lighting from multiple sources
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
cbuffer Constants : register( b0 )
{
	float4x4 mWorld;
	float4x4 mView;
	float4x4 mProjection;
	float4   lightDir[ 2 ];
	float4   lightColor[ 2 ];
	float4   outputColor;
};

 
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Normal : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Name: TriangleVS
// Desc: Vertex shader
//--------------------------------------------------------------------------------------
PS_INPUT TriangleVS( VS_INPUT input )
{
    PS_INPUT output = ( PS_INPUT )0;
    output.Pos = mul( input.Pos, mWorld );
    output.Pos = mul( output.Pos, mView );
    output.Pos = mul( output.Pos, mProjection );
    output.Normal = mul( input.Normal, ( ( float3x3 ) mWorld ) );
    
    return output;
}


//--------------------------------------------------------------------------------------
// Name: TrianglePS
// Desc: Pixel shader applying Lambertian lighting from two lights
//--------------------------------------------------------------------------------------
float4 LambertPS( PS_INPUT input ) : SV_Target
{
    float4 finalColor = 0;
    
    //do NdotL lighting for 2 lights
    for( int i=0; i< 2; i++ )
    {
        finalColor += saturate( dot( ( float3 ) lightDir[ i ], input.Normal ) * lightColor[ i ] );
    }
    finalColor.a = 1;
    return finalColor;
}


//--------------------------------------------------------------------------------------
// Name: TriangleSolidColorPS
// Desc: Pixel shader applying solid color
//--------------------------------------------------------------------------------------
float4 SolidColorPS( PS_INPUT input ) : SV_Target
{
    return outputColor;
}
