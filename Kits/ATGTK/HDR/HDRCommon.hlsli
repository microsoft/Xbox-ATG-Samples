//--------------------------------------------------------------------------------------
// HDRCommon.hlsli
//
// Shader code helper functions for HDR, such as color rotation, ST.2084, etc.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#ifndef __HDRCOMMON_HLSLI__
#define __HDRCOMMON_HLSLI__

// The ST.2084 spec defines max nits as 10,000 nits
static const float g_MaxNitsFor2084 = 10000.0f;         

// Color rotation matrix to rotate Rec.709 color primaries into Rec.2020
static const float3x3 from709to2020 =
{
    { 0.6274040f, 0.3292820f, 0.0433136f },
    { 0.0690970f, 0.9195400f, 0.0113612f },
    { 0.0163916f, 0.0880132f, 0.8955950f }
};

// Color rotation matrix to rotate Rec.2020 color primaries into Rec.709
static const float3x3 from2020to709 =
{
    { 1.6604910f, -0.5876411f, -0.0728499f },
    { -0.1245505f, 1.1328999f, -0.0083494f },
    { -0.0181508f, -0.1005789f, 1.1187297f }
};

// Color rotation matrix to rotate P3-D65 color primaries into Rec.2020
static const float3x3 fromP3to2020 =
{
    { 0.753845f, 0.198593f, 0.047562f },
    { 0.0457456f, 0.941777f, 0.0124772f },
    { -0.00121055f, 0.0176041f, 0.983607f }
};

// Rotation matrix describing a custom color space which is bigger than Rec.709, but a little smaller than P3-D65
// This enhances colors, especially in the SDR range, by being a little more saturated. This can be used instead
// of from709to2020.
static const float3x3 fromExpanded709to2020 =
{
    { 0.6274040f, 0.3292820f, 0.0433136f },
    { 0.0457456, 0.941777, 0.0124772 },
    { -0.00121055, 0.0176041, 0.983607 }
};


//--------------------------------------------------------------------------------------------------------------------
// Name: LinearToST2084
// Desc: Calculates the normalized non-linear ST.2084 value given a normalized linear value
// In:   normalizedLinearValue  - Linear HDR scene value normalized using paper white nits, see NormalizeHDRSceneValue()
// Out:  Normalized non-linear ST.2084 value
//--------------------------------------------------------------------------------------------------------------------

float3 LinearToST2084(float3 normalizedLinearValue)
{
    float3 ST2084 = pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), 0.1593017578f)) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), 0.1593017578f)), 78.84375f);
    return ST2084;  // Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits
}


//--------------------------------------------------------------------------------------------------------------------
// Name: ST2084ToLinear
// Desc: Calculates the normalized linear value given a normalized non-linear ST.2084 value
// In:   normalizedLinearValue  - Linear HDR scene value normalized using paper white nits, see NormalizeHDRSceneValue()
// Out:  Normalized non-linear ST.2084 value
//--------------------------------------------------------------------------------------------------------------------

float3 ST2084ToLinear(float3 ST2084)
{
    float3 normalizedLinear = pow(abs(max(pow(abs(ST2084), 1.0f / 78.84375f) - 0.8359375f, 0.0f) / (18.8515625f - 18.6875f * pow(abs(ST2084), 1.0f / 78.84375f))), 1.0f / 0.1593017578f);
    return normalizedLinear;
}


//--------------------------------------------------------------------------------------------------------------------
// Name: NormalizeHDRSceneValue
// Desc: Per spec, the max nits for ST.2084 is 10,000 nits. We need to establish what the value of 1.0f means
//       by normalizing the values using the defined nits for paper white. According to SDR specs, paper white
//       is 80 nits, but that is paper white in a cinema with a dark environment, and is perceived as grey on
//       a display in office and living room environments. This value should be tuned according to the nits
//       that the consumer perceives as white in his living room, e.g. 200 nits. As refernce, PC monitors is
//       normally in the range 200-300 nits, SDR TVs 150-250 nits
// In:   hdrSceneValue      - Linear HDR scene value normalized using paper white nits
//       paperWhiteNits     - Defines how bright white is, which controls how bright the SDR range is, e.g. 200 nits
// Out:  A normalized linear value that can be used as input to the ST.2084 curve
//--------------------------------------------------------------------------------------------------------------------

float3 NormalizeHDRSceneValue(float3 hdrSceneValue, float paperWhiteNits)
{
    float3 normalizedLinearValue = hdrSceneValue * paperWhiteNits / g_MaxNitsFor2084;
    return normalizedLinearValue;       // Don't clamp between [0..1], so we can still perform operations on scene values higher than 10,000 nits
}


//--------------------------------------------------------------------------------------------------------------------
// Name: CalcHDRSceneValue
// Desc: Calc the value that the HDR scene has to use to output a certain brightness
// In:   nits               - The brightness in nits
//       paperWhiteNits     - Defines how bright white is, which controls how bright the SDR range is, e.g. 200 nits
// Out:  The linear HDR scene value that represents the brightness in nits
//--------------------------------------------------------------------------------------------------------------------

float CalcHDRSceneValue(float nits, float paperWhiteNits)
{
    return nits / paperWhiteNits;
}


//--------------------------------------------------------------------------------------------------------------------
// Name: CalcHDRSceneValue
// Desc: Maps HDR values to the max brightness of the TV, so that we don't clip details that are brighter than what the
//       TV can display. This can be used instead of LinearToST2084(). Refer to the HDRDisplayMapping and HDRCalibrations
//       samples, and the "HDR Display Mapping" white paper
// In:   normalizedLinearValue          - Linear HDR scene value normalized using paper white nits
//       softShoulderStart2084          - Threshold at beginning of soft shoulder for HDR display mapping, normalized non-linear ST.2084 value
//       maxBrightnessOfTV2084          - Max perceived brightness of TV, nits converted to normalized non-linear ST.2084 value 
//       maxBrightnessOfHDRScene2084    - Max perceived brightness of HDR scene, nits converted to normalized non-linear ST.2084 value 
// Out:  Normalized non-linear ST.2084 value that is guarantee not to exceed the TV's max brightness
//--------------------------------------------------------------------------------------------------------------------

float3 MapHDRSceneToDisplayCapabilities(float3 normalizedLinearValue, float softShoulderStart2084, float maxBrightnessOfTV2084, float maxBrightnessOfHDRScene2084)
{
    float3 ST2084 = LinearToST2084(normalizedLinearValue);

    // Use a simple Bezier curve to create a soft shoulder
    const float p0 = softShoulderStart2084;                 // First point is: soft shoulder start nits
    const float p1 = maxBrightnessOfTV2084;                 // Middle point is: TV max nits
    const float p2 = maxBrightnessOfTV2084;                 // Last point is also TV max nits, since values higher than TV max nits are essentially clipped to TV max brightness
    const float max = maxBrightnessOfHDRScene2084;          // To determine range, use max brightness of HDR scene

    float3 t = saturate((ST2084 - p0) / (max - p0));        // Amount to lerp wrt current value
    float3 b0 = (p0 * (1 - t)) + (p1 * t);                  // Lerp between p0 and p1
    float3 b1 = (p1 * (1 - t)) + (p2 * t);                  // Lerp between p1 and p2
    float3 mappedValue = (b0 * (1 - t)) + (b1 * t);         // Final lerp for Bezier

    mappedValue = min(mappedValue, ST2084);                 // If HDR scene max luminance is too close to shoulders, then it could end up producing a higher value than the ST.2084 curve,
                                                            // which will saturate colors, i.e. the opposite of what HDR display mapping should do, therefore always take minimum of the two
    return (ST2084 > softShoulderStart2084) ? mappedValue : ST2084;
}


//--------------------------------------------------------------------------------------------------------------------
// Name: ConvertToHDR10
// Desc: Convert linear HDR values to HDR10
// In:   hdrSceneValue      - The linear HDR scene values
//       paperWhiteNits     - Defines how bright white is, which controls how bright the SDR range is, e.g. 200 nits
// Out:  The converted HDR10 values
//--------------------------------------------------------------------------------------------------------------------

float4 ConvertToHDR10(float4 hdrSceneValue, float paperWhiteNits)
{   
    float3 rec2020 = mul(from709to2020, hdrSceneValue.rgb);                             // Rotate Rec.709 color primaries into Rec.2020 color primaries
    float3 normalizedLinearValue = NormalizeHDRSceneValue(rec2020, paperWhiteNits);     // Normalize using paper white nits to prepare for ST.2084     
    float3 HDR10 = LinearToST2084(normalizedLinearValue);                               // Apply ST.2084 curve

    return float4(HDR10.rgb, hdrSceneValue.a);
}


#endif


