//--------------------------------------------------------------------------------------
// BokehRS.hlsl
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define BokehRS \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
    "DescriptorTable(SRV(t0, numDescriptors = 4), visibility = SHADER_VISIBILITY_PIXEL), " \
    "DescriptorTable(SRV(t4), visibility = SHADER_VISIBILITY_GEOMETRY), " \
    "DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_GEOMETRY), " \
    "CBV(b0), " \
    "StaticSampler(s0," \
                    "filter = FILTER_MIN_MAG_LINEAR_MIP_POINT, " \
                    "addressU = TEXTURE_ADDRESS_BORDER, " \
                    "addressV = TEXTURE_ADDRESS_BORDER, " \
                    "addressW = TEXTURE_ADDRESS_BORDER, " \
                    "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK," \
                    "comparisonFunc = COMPARISON_ALWAYS, " \
                    "maxLOD = 3.402823466e+38f)"
