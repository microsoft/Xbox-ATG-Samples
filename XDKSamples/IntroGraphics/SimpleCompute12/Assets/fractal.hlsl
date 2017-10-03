//--------------------------------------------------------------------------------------
// File: fractal.hlsl
//
// This is a compute shader that draws a Mandelbrot fractal.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define RS\
[\
    RootSignature\
    (\
       "CBV(b0, visibility=SHADER_VISIBILITY_ALL),\
        DescriptorTable(Sampler(s0, numDescriptors=1), visibility=SHADER_VISIBILITY_ALL),\
        DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_ALL),\
        DescriptorTable(UAV(u0, numDescriptors=1), visibility=SHADER_VISIBILITY_ALL)"\
    )\
]

RWTexture2D<float4> OutputTexture : register(u0);
Texture2D ColorMapTexture : register(t0);
SamplerState ColorMapSampler : register(s0);

cbuffer cb0
{
	float4 g_MaxThreadIter : packoffset(c0);
	float4 g_Window : packoffset(c1);
}

[numthreads(8, 8, 1)]
RS
void main(uint3 DTid : SV_DispatchThreadID)
{
	float2 WindowLocal = ((float2)DTid.xy / g_MaxThreadIter.xy) * float2(1, -1) + float2(-0.5f, 0.5f);
	float2 coord = WindowLocal.xy * g_Window.xy + g_Window.zw;

	uint maxiter = (uint)g_MaxThreadIter.z * 4;
	uint iter = 0;
	float2 constant = coord;
	float2 sq;
	do
	{
		float2 newvalue;
		sq = coord * coord;
		newvalue.x = sq.x - sq.y;
		newvalue.y = 2 * coord.y * coord.x;
		coord = newvalue + constant;
		iter++;
	} while (iter < maxiter && (sq.x + sq.y) < 4.0);

	float colorIndex = frac((float)iter / g_MaxThreadIter.z);
	float4 SampledColor = ColorMapTexture.SampleLevel(ColorMapSampler, float2(colorIndex, 0), 0);

	OutputTexture[DTid.xy] = SampledColor;
}