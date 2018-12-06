//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#define HLSL
#define RAYTRACING
#include "GeneralH.hlsli"
#include "AORaytracingHlslCompat.h"
#include "GlobalSharedHlslCompat.h"

// Global args.
ConstantBuffer<SceneConstantBuffer> g_sceneCB : register(b0);
ConstantBuffer<AOConstantBuffer> g_aoCB : register(b1);
ConstantBuffer<AOOptionsConstantBuffer> g_optionsCB : register(b2);

RWTexture2D<float4> RenderTarget : register(u0);

RaytracingAccelerationStructure Scene : register(t0, space0);

SamplerState pointWrapSampler : register(s0);
SamplerState linearClampSampler : register(s1);

// Global materials.

// Local args.
ConstantBuffer<MaterialConstantBuffer> g_materialCB : register(b0, _spaceConcat(SPACE_LOCAL));

ByteAddressBuffer Indices : register(t0, _spaceConcat(SPACE_LOCAL));
StructuredBuffer<Vertex> Vertices : register(t1, _spaceConcat(SPACE_LOCAL));
Texture2D<float4> g_diffuse : register(t2, _spaceConcat(SPACE_LOCAL));
Texture2D<float4> g_specular : register(t3, _spaceConcat(SPACE_LOCAL));
Texture2D<float4> g_normal : register(t4, _spaceConcat(SPACE_LOCAL));

typedef BuiltInTriangleIntersectionAttributes Attributes;

struct ColorPayload
{
    float4 color;
};

struct AOBouncePayload
{
    float val;
};

// Retrieve hit world position.
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Retrieve attribute at a hit position interpolated from vertex attributes using the hit's barycentrics.
float3 HitAttribute(float3 vertexAttribute[3], Attributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

float2 HitAttribute(float2 vertexAttribute[3], Attributes attr)
{
    return vertexAttribute[0] +
        attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
        attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;
    //We are not sure why yet, but it is way to small.
    screenPos *= 20;

    // Unproject the pixel coordinate into a ray.
    float4 world = mul(float4(screenPos, 0, 1), g_sceneCB.projectionToWorld);
    world.xyz /= world.w;

    origin = g_sceneCB.cameraPosition.xyz;
    direction = normalize(world.xyz - origin);
}

[shader("raygeneration")]
void AORaygenShader()
{
    float3 rayDir;
    float3 origin;

    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);

    // Trace the ray.
    // Set the ray's extents.
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = rayDir;
    // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.001;
    ray.TMax = 10000.0;
    ColorPayload payload = { float4(0, 0, 0, 0) };
    
    TraceRay(
        Scene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
        TraceRayParameters::InstanceMask,
        TraceRayParameters::HitGroup::Primary,
        TraceRayParameters::HitGroup::GeometryStride,
        TraceRayParameters::MissShader::Primary,
        ray,
        payload);

    // Write the raytraced color to the output texture.

    //payload.color = BACKGROUND;
    RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

[shader("closesthit")]
void AOClosestHitShader(inout ColorPayload payload, in Attributes attr)
{
    float3 hitPosition = HitWorldPosition();
    
    // Get the base index of the triangle's first 32 bit index.
    unsigned int indexSizeInBytes = 4;
    unsigned int indicesPerTriangle = 3;
    unsigned int triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    unsigned int baseIndex = PrimitiveIndex() * triangleIndexStride;

    // Load up 3 32 bit indices for the triangle.
    const uint3 indices = Indices.Load3(baseIndex);

    // Retrieve corresponding vertex normals for the triangle vertices.
    float3 vertexNormals[3] = {
        Vertices[indices[0]].normal,
        Vertices[indices[1]].normal,
        Vertices[indices[2]].normal
    };

    // Retrieve corresponding vertex tangents for the triangle vertices.
    float3 vertexTangents[3] = {
        Vertices[indices[0]].tangent,
        Vertices[indices[1]].tangent,
        Vertices[indices[2]].tangent
    };

    // Retrieve corresponding vertex texcoords for the triangle vertices.
    float2 vertexTexcoords[3] = {
        Vertices[indices[0]].texcoord,
        Vertices[indices[1]].texcoord,
        Vertices[indices[2]].texcoord
    };

    // Load normal.
    float3 normal;
    float3 origNormal = HitAttribute(vertexNormals, attr);
    float2 uv = HitAttribute(vertexTexcoords, attr);
    if (g_materialCB.isNormalTexture)
    {
        normal = g_normal.SampleLevel(linearClampSampler, uv, 0).xyz * 2.f - 1.f;

        // Compute TBN.
        float3 tangent = HitAttribute(vertexTangents, attr);
        float3 bitangent = cross(origNormal, tangent);
        float3x3 tbn = float3x3(tangent, bitangent, origNormal);

        // Transform normal to world space.
        normal = mul(normal, tbn);
    }
    else
    {
        normal = origNormal;
    }

    // Cast ambient occlusion rays.
    RayDesc ray;
    ray.TMin = 0.001;
    ray.TMax = g_optionsCB.m_distance;

    uint seed = CreateSeed(DispatchRaysIndex().xy);
    float3 noise = normalize(
        float3(
            RandFloat(seed),
            RandFloat(seed),
            RandFloat(seed)
        )
    );

    float3 tangent = normalize(
        -dot(noise, normal) * normal
        + noise
    );

    // Calculate bitangent and tbn matrix.
    float3 bitangent = cross(normal, tangent);
    float3x3 tbn = float3x3(tangent, bitangent, normal);

    // Core of ambient occlusion.
    float weightSum = .0f;
    float occlusion = .0f;

    unsigned int numRays = g_optionsCB.m_numSamples * g_optionsCB.m_numSamples;
    for (unsigned int i = 0; i < numRays; i++)
    {
        ray.Direction = mul(g_aoCB.rays[i].xyz, tbn);

        // Move in the direction of the normal to prevent self collision.
        ray.Origin = hitPosition + EPSILON * origNormal;

        AOBouncePayload aoBouncePayload = { 1.f };
        TraceRay(
            Scene,
            RAY_FLAG_CULL_BACK_FACING_TRIANGLES
            | RAY_FLAG_FORCE_OPAQUE, // ~skip any hit shaders
            TraceRayParameters::InstanceMask,
            TraceRayParameters::HitGroup::Secondary,
            TraceRayParameters::HitGroup::GeometryStride,
            TraceRayParameters::MissShader::Secondary,
            ray,
            aoBouncePayload);

        // Cosine weight the value since the sample distribution is cosine weighted.
        if (g_optionsCB.m_sampleType == 0)
        {
            float weight = cos(dot(ray.Direction, normal));
            occlusion += aoBouncePayload.val * weight;
        }
        else
        {
            occlusion += aoBouncePayload.val;
        }
    }

    occlusion /= float(numRays);
    
    // Lighing model.
    float3 diffuse;
    if (g_materialCB.isDiffuseTexture)
    {
        diffuse = g_diffuse.SampleLevel(linearClampSampler, uv, 0).xyz;
    }
    else
    {
        diffuse = g_materialCB.diffuse;
    }
    
    float3 specular;
    if (g_materialCB.isSpecularTexture)
    {
        specular = g_specular.SampleLevel(linearClampSampler, uv, 0).xyz;
    }
    else
    {
        specular = g_materialCB.specular;
    }
    
    float3 color = (1.f - occlusion) * diffuse;
    payload.color = float4(color, 1);
}

[shader("closesthit")]
void AOBounceClosestHitShader(inout AOBouncePayload payload, in Attributes attr)
{
    float distance = RayTCurrent();

    // apply attenuation
    payload.val = exp(-distance * g_optionsCB.m_falloff);
}

[shader("miss")]
void AOMissShader(inout ColorPayload payload)
{
    payload.color = BACKGROUND;
}

// The miss shader is not necessary for AO, but is included to be
// explicit about what occurs during a missed secondary ray.
[shader("miss")]
void AOBounceMissShader(inout AOBouncePayload payload)
{
    payload.val = 0.f;
}

#endif // AORAYTRACING_HLSL