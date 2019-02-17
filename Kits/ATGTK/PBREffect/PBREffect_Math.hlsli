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
static const float EPSILON = 1e-6f;

float3 BiasX2(float3 x)
{
    return 2.f * x - 1.f;
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

// Normal z component reconstruction for supporting BC5
float3 TwoChannelNormalX2(float2 normal)
{
    float2 xy = 2.0f * normal - 1.0f;
    float z = sqrt(1 - dot(xy, xy));
    return float3(xy.x, xy.y, z);
}

// Shlick's approximation of Fresnel
// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float3 Fresnel_Shlick(in float3 f0, in float3 f90, in float x)
{
    return f0 + (f90 - f0) * pow(1.f - x, 5.f);
}

// Burley B. "Physically Based Shading at Disney"
// SIGGRAPH 2012 Course: Practical Physically Based Shading in Film and Game Production, 2012.
float Diffuse_Burley(in float NdotL, in float NdotV, in float LdotH, in float roughness)
{
    float fd90 = 0.5f + 2.f * roughness * LdotH * LdotH;
    return Fresnel_Shlick(1, fd90, NdotL).x * Fresnel_Shlick(1, fd90, NdotV).x;
}

// GGX specular D (normal distribution)
// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
float Specular_D_GGX(in float alpha, in float NdotH)
{
    const float alpha2 = alpha * alpha;
    const float lower = (NdotH * NdotH * (alpha2 - 1)) + 1;
    return alpha2 / max(EPSILON, PI * lower * lower);
}

// Schlick-Smith specular G (visibility) with Hable's LdotH optimization
// http://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf
// http://graphicrants.blogspot.se/2013/08/specular-brdf-reference.html
float G_Shlick_Smith_Hable(float alpha, float LdotH)
{
    return rcp(lerp(LdotH * LdotH, 1, alpha * alpha * 0.25f));
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
    return PBR_RadianceTexture.SampleLevel(PBR_IBLSampler, dir, mip);
}

#endif