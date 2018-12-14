//--------------------------------------------------------------------------------------
// RMS.hlsli
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define RMSRS \
    "CBV(b0, visibility=SHADER_VISIBILITY_ALL)," \
    "DescriptorTable(SRV(t0), visibility=SHADER_VISIBILITY_ALL)," \
    "DescriptorTable(SRV(t1), visibility=SHADER_VISIBILITY_ALL)," \
    "DescriptorTable(UAV(u0), visibility=SHADER_VISIBILITY_ALL)," \
    "DescriptorTable(UAV(u1), visibility=SHADER_VISIBILITY_ALL)"

#define RMS_THREADGROUP_WIDTH 64

cbuffer RMSCB : register(b0)
{
    uint g_textureWidth;
    uint g_mipLevel;
    bool g_reconstructZA;
    bool g_reconstructZB;
};
