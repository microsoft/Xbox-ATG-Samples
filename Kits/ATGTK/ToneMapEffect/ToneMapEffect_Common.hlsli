//--------------------------------------------------------------------------------------
// ToneMapEffect_Common.hlsli
//
// A simple flimic tonemapping effect for DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef __TONEMAPEFFECT_COMMON_HLSLI__
#define __TONEMAPEFFECT_COMMON_HLSLI__

#define ToneMapRS \
"RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"            DENY_HULL_SHADER_ROOT_ACCESS )," \
"DescriptorTable ( SRV(t0) ),"\
"DescriptorTable ( Sampler(s0) )" 

#endif