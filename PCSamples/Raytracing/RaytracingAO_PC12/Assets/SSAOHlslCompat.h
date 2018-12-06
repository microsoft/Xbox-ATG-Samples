#ifndef SSAOHLSLCOMPAT_H
#define SSAOHLSLCOMPAT_H

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;
#endif

#define NUM_BUFFERS 4

struct SSAORenderConstantBuffer
{
    XMFLOAT4 invThicknessTable[3];
    XMFLOAT4 sampleWeightTable[3];
    XMFLOAT2 invSliceDimension;
    FLOAT  normalMultiply;
};

struct BlurAndUpscaleConstantBuffer
{
    XMFLOAT2 invLowResolution;
    XMFLOAT2 invHighResolution;
    FLOAT noiseFilterStrength;
    FLOAT stepSize;
    FLOAT blurTolerance;
    FLOAT upsampleTolerance;
};

#endif