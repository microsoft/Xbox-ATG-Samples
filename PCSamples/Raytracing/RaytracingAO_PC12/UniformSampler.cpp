//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "UniformSampler.h"

using namespace DirectX;

XMFLOAT3 UniformSampler::Sample(float a, float b, float c)
{
    return { a, b, c };
}