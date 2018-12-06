//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <random>

class Sampler
{
public:
    static float RandFloat();
    static float RandSignedFloat();
    static void SetSeed(unsigned int seed);

    virtual DirectX::XMFLOAT3 Sample(float a, float b, float c) = 0;
    virtual void Sample(DirectX::XMFLOAT4* data, unsigned int numSamples);
    virtual DirectX::XMFLOAT3 Sample();

protected:
    static std::random_device m_randomDevice;
    static std::mt19937 m_generator;
    static std::uniform_real_distribution<float> m_dis;
    static std::uniform_real_distribution<float> m_signedDis;
};