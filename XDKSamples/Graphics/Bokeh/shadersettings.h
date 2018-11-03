//--------------------------------------------------------------------------------------
// ShaderSettings.h
//
// A header shader between Bokeh.hlsl and BokehEffect.cpp that allows to set some
// common compile-time parameters
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//
//--------------------------------------------------------------------------------------


// This is the scale of the initial downsample of the original color and depth buffers
// into the single RGBZ buffer
// This must be 2
#define FIRST_DOWNSAMPLE    2

// This defines how many individual weights we recognise. Usually normalisation weights
// vary a lot when a sprite is tiny, but then they follow a more predictable area rule.
// For example, for a round iris texture this rule is 1/(PI * R^2).
// However, for unusual iris textures the area rule can't be derived in closed form so
// our numerical weight calculation is still needed.
// This is why this is set to 64 and not to 8. Eight would be enough for round irises.
// Note that the whole normalisation effort isn't needed for far range blur, only for
// near range blur.
#define NUM_RADII_WEIGHTS   64

#pragma warning ( disable: 4715 )