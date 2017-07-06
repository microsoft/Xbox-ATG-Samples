//--------------------------------------------------------------------------------------
// PBREffect_Lighting.hslsi
//
// A physically based shader for forward rendering on DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef __PBR_MATH_HLSLI__
#define __PBR_MATH_HLSLI__

static const float PI = 3.14159265f;

float3 BiasX2(float3 x)
{
    return 2.0f * x - 1.0f;
}

// Given a local normal, transform it into a tangent space given by surface normal and tangent
float3 PeturbNormal(float3 localNormal, float3 surfaceNormalWS, float3 surfaceTangentWS)
{
    float3 normal = normalize(surfaceNormalWS);
    float3 tangent = normalize(surfaceTangentWS);
    float3 binormal = cross(normal, tangent);     // reconstructed from normal & tangent
    float3x3 TBN = { tangent, binormal, normal }; // world "frame" for local normal 

    return mul(localNormal, TBN);                // transform to local to world (tangent space)
}

// Shlick's approximation of Fresnel
float3 Fresnel_Shlick(in float3 f0, in float3 f90, in float x)
{
    return f0 + (f90 - f0) * pow(1.f - x, 5.f);
}

// No frills Lambert shading.
float Diffuse_Lambert(in float NdotL)
{
    return NdotL;
}

// Burley's diffuse BRDF
float Diffuse_Burley(in float NdotL, in float NdotV, in float LdotH, in float roughness)
{
    float fd90 = 0.5f + 2.f * roughness * LdotH * LdotH;
    return Fresnel_Shlick(1, fd90, NdotL).x * Fresnel_Shlick(1, fd90, NdotV).x;
}

// GGX specular D (normal distribution)
float Specular_D_GGX(in float alpha, in float NdotH)
{
    const float alpha2 = alpha * alpha;
    const float lower = (NdotH * NdotH * (alpha2 - 1)) + 1;
    const float result = alpha2 / (PI * lower * lower);

    return result;
}

// Schlick-Smith specular G (visibility) with Hable's LdotH optimization
float G_Shlick_Smith_Hable(float alpha, float LdotH)
{
    const float k = alpha / 2.0;
    const float k2 = k * 2;
    const float invk2 = 1 - k2;
    return rcp(LdotH * LdotH * invk2 + k2);
}

// Map a normal on unit sphere to UV coordinates
float2 SphereMap(float3 N)
{
    return float2(atan2(N.x, N.z) / (PI * 2) + 0.5, 0.5 - (asin(N.y) / PI));
}

// A microfacet based BRDF.
//
// alpha:           This is roughness * roughness as in the "Disney" PBR model by Burley et al.
//
// specularColor:   The F0 reflectance value - 0.04 for non-metals, or RGB for metals. This follows model 
//                  used by Unreal Engine 4.
//
// NdotV, NdotL, LdotH, NdotH: vector relationships between,
//      N - surface normal
//      V - eye normal
//      L - light normal
//      H - half vector between L & V.
float3 Specular_BRDF(in float alpha, in float3 specularColor, in float NdotV, in float NdotL, in float LdotH, in float NdotH)
{
    // Specular D (microfacet normal distribution) component
    float specular_D = Specular_D_GGX(alpha, NdotH);

    // Specular Fresnel
    float3 specular_F = Fresnel_Shlick(specularColor, 1, LdotH);

    // Specular G (visibility) component
    float specular_G = G_Shlick_Smith_Hable(alpha, LdotH);

    return specular_D * specular_F * specular_G;
}

// Diffuse irradiance
float3 Diffuse_IBL(in float3 N)
{
    return PBR_IrradianceTexture.Sample(PBR_IBLSampler, N);
}

// Approximate specular image based lighting by sampling radiance map at lower mips 
// according to roughness, then modulating by Fresnel term. 
float3 Specular_IBL(in float3 N, in float3 V, in float lodBias)
{
    float mip = lodBias * PBR_NumRadianceMipLevels;
    float3 dir = reflect(-V, N);
    float3 envColor = PBR_RadianceTexture.SampleLevel(PBR_IBLSampler, dir, mip);
    return envColor;
}

#endif