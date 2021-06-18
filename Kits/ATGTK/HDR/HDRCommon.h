//--------------------------------------------------------------------------------------
// HDRCommon.h
//
// Simple helper functions for HDR
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#pragma once

#include <cstdlib>
#include <cmath>

namespace DX
{
    // The ST.2084 spec defines max nits as 10,000 nits
    const float c_MaxNitsFor2084 = 10000.0f;

    // Apply the ST.2084 curve to normalized linear values and outputs normalized non-linear values
    inline float LinearToST2084(float normalizedLinearValue)
    {
        float ST2084 = powf((0.8359375f + 18.8515625f * powf(abs(normalizedLinearValue), 0.1593017578f)) / (1.0f + 18.6875f * powf(abs(normalizedLinearValue), 0.1593017578f)), 78.84375f);
        return ST2084;  // Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits
    }

    // ST.2084 to linear, resulting in a linear normalized value
    inline float ST2084ToLinear(float ST2084)
    {
        float normalizedLinear = powf(std::max(powf(abs(ST2084), 1.0f / 78.84375f) - 0.8359375f, 0.0f) / (18.8515625f - 18.6875f * powf(abs(ST2084), 1.0f / 78.84375f)), 1.0f / 0.1593017578f);
        return normalizedLinear;
    }

    // Takes as inout the non-normalized value from the HDR scene. This function is only used to output UI values
    inline float LinearToST2084(float hdrSceneValue, float paperWhiteNits)
    {
        // HDR scene will have values zero to larger than 1, e.g. [0..100], but the ST.2084 curve
        // transforms a normalized linear value, so we first need to normalize the HDR scene value.
        // To do this, we need define at what brightness/nits paper white is, i.e. how bright is
        // the value of 1.0f in the HDR scene.
        float normalizedLinearValue = hdrSceneValue * paperWhiteNits / c_MaxNitsFor2084;

        return LinearToST2084(normalizedLinearValue);
    }

    // Calculates the normalized linear value going into the ST.2084 curve, using the nits
    inline float CalcNormalizedLinearValue(float nits)
    {
        return nits / c_MaxNitsFor2084;     // Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits
    }

    // Calc normalized linear value from hdrScene and paper white nits
    inline float CalcNormalizedLinearValue(float hdrSceneValue, float paperWhiteNits)
    {
        float normalizedLinearValue = hdrSceneValue * paperWhiteNits / c_MaxNitsFor2084;
        return normalizedLinearValue;       // Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits
    }

    // Calculates the brightness in nits for a certain normalized linear value
    inline float CalcNits(float normalizedLinearValue)
    {
        return normalizedLinearValue * c_MaxNitsFor2084;
    }

    // Calculates the brightness in nits for a certain linear value in the HDR scene
    inline float CalcNits(float hdrSceneValue, float paperWhiteNits)
    {
        float normalizedLinear = CalcNormalizedLinearValue(hdrSceneValue, paperWhiteNits);
        return CalcNits(normalizedLinear);
    }

    // Calc the value that the HDR scene has to use to output a certain brightness
    inline float CalcHDRSceneValue(float nits, float paperWhiteNits)
    {
        return nits / paperWhiteNits;
    }

    // Calc HDR scene value from normalized value and paper white nits
    inline float CalcHDRSceneValueFromNormalizedValue(float normalizedLinearValue, float paperWhiteNits)
    {
        float hdrSceneValue = normalizedLinearValue * c_MaxNitsFor2084 / paperWhiteNits;
        return hdrSceneValue;
    }
}
