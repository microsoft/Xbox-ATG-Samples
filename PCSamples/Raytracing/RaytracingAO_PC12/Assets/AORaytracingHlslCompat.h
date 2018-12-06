#ifndef RAYTRACINGHLSLCOMPAT_H
#define RAYTRACINGHLSLCOMPAT_H

// Workaround for NV driver not supporting null local root signatures. 
// Use an empty local root signature where a shader does not require it.
#define USE_NON_NULL_LOCAL_ROOT_SIG 1

#define EPSILON .0001

#define NOISE_W 100
#define MAX_SAMPLES 15
#define MAX_OCCLUSION_RAYS (MAX_SAMPLES * MAX_SAMPLES)

// Concat space to number.
#define _concat(A, B) A ## B
#define _spaceConcat(A) _concat(space, A) 

#ifdef HLSL
#include "HlslCompat.h"
#else
using namespace DirectX;

// Shader will use byte encoding to access indices.
typedef uint32_t Index;
#endif

struct AOConstantBuffer
{
    // float4 is needed to prevent CPU from performing a full pack.
    XMFLOAT4 rays[MAX_OCCLUSION_RAYS];
};


struct AOOptionsConstantBuffer
{
    FLOAT m_distance;
    FLOAT m_falloff;
    UINT m_numSamples;
    UINT m_sampleType;
};

namespace TraceRayParameters
{
    static const UINT InstanceMask = ~0U;   // Everything is visible.
    namespace HitGroup
    {
        enum Offset
        {
            Primary,
            Secondary, // No hit group needed for secondary rays.
            Count
        };

        static const UINT GeometryStride = HitGroup::Count;
    }

    namespace MissShader
    {
        enum Offset
        {
            Primary,
            Secondary,
            Count
        };
    }
}

// Spaces must be definitions for macro concat to work.
#define DEFAULT_SPACE 0
#define SPACE_TEXTURE 1
#define SPACE_MATERIAL 2
#define SPACE_LOCAL 3

#endif // RAYTRACINGHLSLCOMPAT_H