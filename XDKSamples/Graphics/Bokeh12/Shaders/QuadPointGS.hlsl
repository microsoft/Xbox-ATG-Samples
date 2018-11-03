//--------------------------------------------------------------------------------------
// QuadPointGS.hlsl
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Bokeh.hlsli"

// This function takes the screen position in source screen coordinates, loads the RGBZ for this point
// then calculates CoC and scale and the correct output resolution and outputs all that for further
// expansion into a sprite.
GSSceneInPoint GenerateSpritePointFromXY(uint xx, uint yy)
{
    BokehInfo    i;

    float4    rgbz = t0.Load(uint3(xx, yy, 0));

    float3 clr = rgbz.xyz;

    i.fCoCRadius = CalculateCoC(rgbz.w, i.fDepthDifference);

    GSSceneInPoint output;

    output.clr.xyz = clr.xyz;

    output.radius = i.fCoCRadius;

    // note on the 0.5f here
    // +0.5f when using downsampled source to make sure geometry of 0.5f size crosses pixel centres
    // +0.0f aligns texels to pixels but the sprites don't cross pixel centres at size 0.5f. it's probably not such a big deal
    // if we were working in full res, but at half res 1 pixel sprites are actually 2x2 when blended back so we can't afford to lose them
    // so we actually offset the geometry to cross pixel centre by 0.5f and also offset texture coordinate back

    float2 fSpriteOrg = (float2(xx, yy) + 0.5f) / float2(g_screenSize.xy / FIRST_DOWNSAMPLE);
    output.pos = float2(2.f * fSpriteOrg.x - 1, 1 - 2.f * fSpriteOrg.y);

    float energyScaler = g_initialEnergyScale;

    // first target is quarter res
    output.radius /= 2;

    // too large? put into the next sized viewport, adjust parameters accordingly
    uint baseVp = 0;

    // NOTE: should be able to output fewer points for low res buffers for the same effect
    // for thta I need dowsnampled depth/color images

    const uint nearOrFarIndex = (i.fDepthDifference >= 0) ? 1 : 0;        // either near (0) or far (1)

    if (output.radius >= g_switchover1[nearOrFarIndex])
    {
        baseVp = 2;

        // TODO: for best quality need to average 4x4 pixels

        output.radius /= 2;
        energyScaler /= 4;  // correctly preserved energy is /4 (because the area decreases as 1/4 give or take)
                            // we can use /3 because upsampling will lose some and for more dramatic
                            // defocused highlight but that corrupts the blend interface
    }

    if (output.radius >= g_switchover2[nearOrFarIndex])
    {
        baseVp = 4;

        // TODO: for best quality need to average 8x8 pixels

        output.radius /= 2;
        energyScaler /= 4;
    }

    // this makes a fundamental difference, probably best picked based on looks
    // the advantage of rounding is weights can be "exactly" precalculated at least
    // if we don't round we can't predict what happens to the pixel inbetween integer pixel sizes
    // unfortunately, at integer radii, we get noticeable quantisation in sizes
    if (i.fDepthDifference < 0)
    {
        // near interface is most troublesome, don't start blending until it reaches at least 2x2 pixels in size,
        // something a quarter res target can easily handle
        if (output.radius < 1.0f)
            output.radius = 0;
    }

    output.radius = round(output.radius * 2) / 2;
    float d = output.radius;    // omit *2 for diameter, scale the coeff instead


                                // we take 1 pixel and spread it's energy across a larger area. to preserve the energy
                                // we need to calculate the weight reduction which cannot be established exactly and varies
                                // with the shape of the iris.
                                // note that energy preservation doesn't matter for the far layer -- we need it for the near
                                // layer to correctly produce defocused blurs over in-focus and far layers
                                // if you're only doing the far layer, you can set it to 1
    float   energy = (d < (NUM_RADII_WEIGHTS / 2)) ?                          // this gens faster code than an if() statement (movc vs a branch)
        1.267f :                                            // this is 4/PI which in limit is the right weight for a round iris
        energies.Load(uint2(round(d * 2), 0)).x;

    output.weight = energy * energyScaler / (d * d);

    // dispatch the point to the right viewport
    output.viewportIndex = baseVp + nearOrFarIndex;

    return output;
}


// This is a workhorse of the sprite expansion function. It will decide whether the sprite is big enough and it will expand it
// into a triangle sprite.
void EmitSpriteIfBigEnough(GSSceneInPoint pt, inout TriangleStream<PSSceneInPoint> spriteStream, float3 useThisColorFor2x2)
{
    static const float4 posuv[3] =
    {
        float4(-1, -1, 0,  1),
        float4(-1,  3, 0, -1),
        float4(3, -1, 2,  1),
    };

    const float fBlurRadiusInPixels = pt.radius;

    // this will govern how much blend overlap there is between the layers
    if (fBlurRadiusInPixels >= 0.5f)
    {
        const uint   vp = pt.viewportIndex;
        const float3 clr = (vp >= 2) ? useThisColorFor2x2 : pt.clr.xyz;
        const float2 org = pt.pos.xy;
        const float  weight = pt.weight;

        for (int i = 0; i < 3; i++)
        {
            PSSceneInPoint output;

            output.pos = float4((posuv[i].xy * fBlurRadiusInPixels) / float2(g_viewports[vp].zw) + org, 0, 1);
            output.clr = clr;
            output.weight = weight;
            output.uv = posuv[i].zw + g_fIrisTextureOffset;       // iris texture size here. this is to match texels to pixels with 1x1 sprites
            output.viewportIndex = vp;

            spriteStream.Append(output);
        }

        spriteStream.RestartStrip();
    }
}

// triangles are 0.5 ms faster (1.7 vs 2.4ms with quads)!

// we haven't got anything coming from the VS, but the compiler needs to know the input types
// so we create this empty struct to keep the compiler happy
struct Empty {};

// This converts a point into a sprite
// It routes the viewport index to GS output so that the geometry ends up in the correct viewport
// There is a point primitive for each pixel of the source RGBZ texture, this shader reads the
// point primitive and the texel value, calculates the CoC, then normalisation factor, then
// decides which viewport to output the sprite to and whether to output one or four sprites, then
// finally expands the point into a sprite
[maxvertexcount(12)]  // we output from 1 to 4 triangle sprites so we need space for up to 12 output vertices
[RootSignature(BokehRS)]
void main(point Empty p[1],
    uint inst0 : SV_PrimitiveID,        // VertexID == PrimitiveID for point lists
    inout TriangleStream<PSSceneInPoint> spriteStream)
{
    // accessing data in 4x1 blocks is slightly quicker than in 2x2 blocks
    // for optimising the number of quads we have to access 2x2 blocks
    // because we need to see the difference in Y as well as in X between the
    // source pixels
    //
    // the difference is a constant 0.015 ms
    //

#ifdef OPTIMISE
#define  READ2x2
#endif

#ifdef READ2x2
    const uint topLeftX = 2 * ((inst0) % floor(g_screenSize.x / (FIRST_DOWNSAMPLE * 2.0)));
    const uint topLeftY = 2 * ((inst0) / floor(g_screenSize.x / (FIRST_DOWNSAMPLE * 2.0)));
#else
    const uint    topLeftX = (inst0 * 4) % floor(g_screenSize.x / (FIRST_DOWNSAMPLE));
    const uint    topLeftY = (inst0 * 4) / floor(g_screenSize.x / (FIRST_DOWNSAMPLE));
#endif

    GSSceneInPoint  pt[4];

    uint i;
    for (i = 0; i < 4; ++i)
    {
#ifdef  READ2x2
        const uint    xx = topLeftX + (i % 2);
        const uint    yy = topLeftY + (i / 2);
#else
        const uint    xx = topLeftX + i;
        const uint    yy = topLeftY;
#endif

        pt[i] = GenerateSpritePointFromXY(xx, yy);
    }

    // now see if we can output only 1 sprite for this saves us tons of time if we can do it

    bool bOutputOne = true;

    if ((pt[0].viewportIndex != pt[1].viewportIndex) ||
        (pt[0].viewportIndex != pt[2].viewportIndex) ||
        (pt[0].viewportIndex != pt[3].viewportIndex))
    {
        bOutputOne = false;
    }

    const float radiusThreshold = 1;
    const float colorThreshold = 0.2f;

    if (abs(pt[0].radius - pt[1].radius) > radiusThreshold ||
        abs(pt[0].radius - pt[2].radius) > radiusThreshold ||
        abs(pt[0].radius - pt[3].radius) > radiusThreshold)
    {
        bOutputOne = false;
    }

    // TODO: replace with length square
    if (distance(pt[0].clr, pt[1].clr) > colorThreshold ||
        distance(pt[0].clr, pt[2].clr) > colorThreshold ||
        distance(pt[0].clr, pt[3].clr) > colorThreshold)
    {
        bOutputOne = false;
    }

    // causes artefacts near the boundary so don't attempt to optimise near the boundary
    if (pt[0].radius < 4.f)
        bOutputOne = false;

#ifndef OPTIMISE
    bOutputOne = false;
#endif

    if (bOutputOne)
    {
        // TODO: properly merge, this is a quick hack
        GSSceneInPoint mergedSprite;

        mergedSprite.clr = 0.25f * (pt[0].clr + pt[1].clr + pt[2].clr + pt[3].clr);
        mergedSprite.weight = 0.25f * (pt[0].weight + pt[1].weight + pt[2].weight + pt[3].weight) * 4; // TODO: max weight?
        mergedSprite.radius = 0.25f * (pt[0].radius + pt[1].radius + pt[2].radius + pt[3].radius);
        mergedSprite.pos = 0.25f * (pt[0].pos + pt[1].pos + pt[2].pos + pt[3].pos);
        mergedSprite.viewportIndex = pt[0].viewportIndex;

        EmitSpriteIfBigEnough(mergedSprite, spriteStream, mergedSprite.clr);
    }
    else
    {
        // this colour will only be used if a sprite goes into a smaller render target
        // TODO: try picking the brightest clr here
        float3 averageColor = 0.25f * (pt[0].clr + pt[1].clr + pt[2].clr + pt[3].clr);
        for (i = 0; i < 4; ++i)
        {
            EmitSpriteIfBigEnough(pt[i], spriteStream, averageColor);
        }
    }
}
