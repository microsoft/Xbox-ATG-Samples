//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "UniformHemiSampler.h"

using namespace DirectX;

XMFLOAT3 UniformHemiSampler::Sample(float a, float b, float /*c*/)
{
    float sintheta = sqrt(a);
    float cosTheta = sqrt(1.f - sintheta * sintheta);
    float phi = b * XM_2PI;

    float x = sintheta * cos(phi);
    float y = sintheta * sin(phi);
    float z = cosTheta;

    return { x, y, z };
}