//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "CosineHemiSampler.h"

using namespace DirectX;

DirectX::XMFLOAT3 CosineHemiSampler::Sample(float a, float b, float /*c*/)
{
    float sintheta = acos(sqrt(a));
    float phi = b * XM_2PI;

    float x = sintheta * cos(phi);
    float y = sintheta * sin(phi);
    float z = sqrt(1.f - x * x - y * y);

    return { x, y, z };
}