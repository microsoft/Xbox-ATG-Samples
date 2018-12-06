//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Sampler.h"

// Stratified Sampler only works in 2D.
template<class T>
class StratifiedSampler : public T
{
public:
    static_assert(
        std::is_base_of<Sampler, T>::value,
        "T must be a subclass of Sample."
    );

    // inheret methods.
    using T::Sample;

    // numSamples is squared.
    void Sample(XMFLOAT4* data, unsigned int numSamples)
    {
        float offset = 1.f / float(numSamples);

        for (unsigned int i = 0; i < numSamples; i++)
        {
            for (unsigned int j = 0; j < numSamples; j++)
            {
                const float u = float(i) * offset + RandFloat() * offset;
                const float v = float(j) * offset + RandFloat() * offset;

                XMFLOAT3 sample = Sample(u, v, 0);
                data[i * numSamples + j] = { sample.x, sample.y, sample.z, 0 };
            }
        }
    }
};