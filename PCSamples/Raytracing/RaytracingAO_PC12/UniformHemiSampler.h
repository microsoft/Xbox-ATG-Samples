//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Sampler.h"

// Uniform sampling over a hemisphere.
class UniformHemiSampler : public Sampler
{
public:
    using Sampler::Sample;
    virtual DirectX::XMFLOAT3 Sample(float a, float b, float c);
};