//--------------------------------------------------------------------------------------
// CreateEnergyTexRS.hlsl
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define CreateEnergyTexRS \
    "RootFlags(0), " \
    "DescriptorTable(SRV(t0)), " \
    "DescriptorTable(UAV(u0, numDescriptors = 2)), " \
    "StaticSampler(s0," \
                    "filter = FILTER_MIN_MAG_MIP_LINEAR, " \
                    "addressU = TEXTURE_ADDRESS_BORDER, " \
                    "addressV = TEXTURE_ADDRESS_BORDER, " \
                    "addressW = TEXTURE_ADDRESS_BORDER, " \
                    "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK," \
                    "comparisonFunc = COMPARISON_ALWAYS, " \
                    "maxLOD = 3.402823466e+38f)"