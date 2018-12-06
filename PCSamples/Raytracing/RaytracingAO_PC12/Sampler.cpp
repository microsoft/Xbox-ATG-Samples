//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Sampler.h"

using namespace DirectX;

// Random generator.
std::random_device Sampler::m_randomDevice;

std::mt19937 Sampler::m_generator(m_randomDevice());

// [0, 1] distribution.
std::uniform_real_distribution<float> Sampler::m_dis(
    0.f, std::nextafter(1.f, std::numeric_limits<float>::max()));

// [-1, 1] distribution.
std::uniform_real_distribution<float> Sampler::m_signedDis(
    -1.f, std::nextafter(1.f, std::numeric_limits<float>::max()));

// Choose a float with uniform distribution from [0, 1].
float Sampler::RandFloat()
{
    return m_dis(m_generator);
}

// Choose a float with uniform distribution from [-1, 1].
float Sampler::RandSignedFloat()
{
    return m_signedDis(m_generator);
}

void Sampler::SetSeed(unsigned int seed)
{
    m_generator = std::mt19937(seed);
}

// Default Sample.
XMFLOAT3 Sampler::Sample()
{
    return Sample(RandFloat(), RandFloat(), RandFloat());
}

// Generate a set of samples.
// Note that numSamples does not necessarily
// equal the number of data points returned (varies based on sample method).
// float4 is used for packing reasons.
void Sampler::Sample(XMFLOAT4* data, unsigned int numSamples)
{
    XMFLOAT3 sample;

    for (unsigned int i = 0; i < numSamples; i++)
    {
        sample = Sample();
        data[i] = { sample.x, sample.y, sample.z, 0 };
    }
}