//--------------------------------------------------------------------------------------
// Bokeh.hlsl
//
// Demonstrates how to render depth of field using point sprites.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
//
// The approach is two fold.
//
// First it uses scattering instead of gather to simulate the blur appearing in the out-of-focus
// parts of the image. The simple idea is to output a point sprite per pixel. Each point sprite
// has a diameter of the circle of confusion at the source pixel.
//
// Second is the splitting of the image into 3 layers to make sure occlusion is correct between
// the in-focus and out-of-focus parts of the image.
//
// This is a version optimised for speed. It runs inside 2ms.
//
// One artefact that is not solved by this approach is missing self occlusion in the near range
// layer (when pixels in the near range image don't always correctly occlude pixels in the same
// range). This doesn't happen between the ranges and in the far range.
//
// The reason for the artefact is that we sort by layers, not per pixel. There is an approach
// that shows how to sort per-pixel. It involves one extra step. We do the point sprites as usual,
// then for reach pixel we output into 3 render targets based on the size of CoC. This only
// needs to be done for the near range because this artefact isn't observable in the far range.
// After the 3 RTs are done we blend them using premultiplied blending to get the nice occlusion
// inside the range.
//
// Further optimisations are possible. For example, as we start with a quarter resolution source
// and output a 1/4, 1/8 and 1/16 destinations, it's possible to reduce the number of source
// vertices by 4 and conditionally expand either 1 or 4 sprites in the GS.
//
//--------------------------------------------------------------------------------------

#include "..\shadersettings.h"


#if FIRST_DOWNSAMPLE != 2
#error
#endif


// parameters for bokeh
cbuffer bokeh : register( b0 )
{
    float       g_fMaxCoCRadiusNear     : packoffset( c0.x );
    float       g_fFocusLength          : packoffset( c0.y );
    float       g_fFocalPlane           : packoffset( c0.z );
    float       g_fFNumber              : packoffset( c0.w );
    float2      g_depthBufferSize       : packoffset( c1.x );
    float2      g_dofTexSize            : packoffset( c1.z );
    float2      g_screenSize            : packoffset( c2.x );
    float       g_fMaxCoCRadiusFar      : packoffset( c2.z );
    float       g_fIrisTextureOffset    : packoffset( c2.w );
    float4      g_viewports[ 6 ]        : packoffset( c3 );        // 0 -- near, 1 -- far, 2 - nearsmall, 3 - farsmall, etc
    float2      g_switchover1           : packoffset( c9.x );
    float2      g_switchover2           : packoffset( c9.z );
    float       g_initialEnergyScale    : packoffset( c10.x );
    // pad[ 3 ]
    float4x4    g_mInvProj              : packoffset( c11 );
};

// iris texture blend weights based on the sprite size
Texture1D   energies : register( t4 );

Texture2D   t0 : register( t0 );
Texture2D   t1 : register( t1 );
Texture2D   t2 : register( t2 );
Texture2D   t3 : register( t3 );

SamplerState s0 : register( s0 );


// used to pass interpolators for full screen quad rendering
struct PSSceneIn
{
    float2 tex : TEXCOORD0;
    float4 pos : SV_Position;
};

struct GSSceneInPoint
{
    float2  pos : POS;
    float3  clr : TEXCOORD1;
    float   weight : WEIGHT;
    float   radius : RADIUS;
    uint    viewportIndex : VPINDEX;
};

struct PSSceneInPoint
{
    float4  pos : SV_Position;
    uint    viewportIndex : SV_ViewportArrayIndex;
    float2  uv : TEXCOORD0;
    float3  clr : TEXCOORD1;
    float   weight : WEIGHT;
};

struct BokehInfo
{
    float    fCoCRadius;            // circle of confusion radius
    float    fDepthDifference;    // difference between the focus plane and depth
    float    fDepth;                // linear view space depth value
};



// calculates the size of CoC given the linear depth of a pixel.
// it uses a thin lens formula from photography so you can control DOF blur as you would
// on a camera -- by changing the aperture and focus length.
// replace this if you don't like the look of this.
float CalculateCoC( in float fDepth, out float fDepthDifference )
{
    fDepthDifference = fDepth - g_fFocalPlane;

    // thin lens
    const float var = abs( fDepthDifference ) / fDepth;
    const float cnst = g_fFocusLength * g_fFocusLength / (g_fFNumber * (g_fFocalPlane - g_fFocusLength));    // TODO: calculate on the CPU
    const float cocMetersRadius = 0.5f * var * cnst;                  // coc radius in meters
    const float cocRelativeToFilm = cocMetersRadius / 0.035f;         // size relative to 35mm sensor
    const float cocRadiusInPixels = g_depthBufferSize.x * cocRelativeToFilm;

    return min( (fDepthDifference < 0) ? g_fMaxCoCRadiusNear : g_fMaxCoCRadiusFar, cocRadiusInPixels );
}

// given a screen position, and using a colour and a depth buffer, calculate the DOF parameters
BokehInfo GetBokehInfo( uint2 pos )
{
    BokehInfo    i;

    float4    fDepthSample;        // view space coord
    float     fTexDepth;            // depth value from the depth buffer
    float     fBlurRadiusInPixels;// size of screen blur in pixels (rounded and clamped CoC size)
    float2    fInvSize;            // TODO: precalc - inverse screen size

    fInvSize = 1.f / float2(g_screenSize.xy);
    const float2 fTexCoord = (float2(pos)+0.5f) * fInvSize;        // point to the centre of the 2x2 block for depth calculation
    const float2 fNDC = float2(2.f * fTexCoord.x - 1, 1 - 2.f * fTexCoord.y);

    fTexDepth = t2.Load(uint3(pos, 0)).x;

    fDepthSample = mul(float4(fNDC, fTexDepth, 1), g_mInvProj);

    i.fDepth = fDepthSample.z / fDepthSample.w;

    i.fCoCRadius = CalculateCoC(i.fDepth, i.fDepthDifference);

    return i;
}
