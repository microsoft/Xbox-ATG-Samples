//--------------------------------------------------------------------------------------
// PBREffect_PSTextured.hslsi
//
// A physically based shader for forward rendering on DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "PBREffect_Common.hlsli"
#include "PixelPacking_Velocity.hlsli"

struct PSOut_Velocity
{
    float4 color : SV_Target0;
    packed_velocity_t velocity : SV_Target1;
};

// Pixel shader: pixel lighting + texture.
[RootSignature(PBREffectRS)]
PSOut_Velocity PSTexturedVelocity(VSOut_Velocity pin)
{
    PSOut_Velocity output;

    // vectors
    const float3 V = normalize(PBR_EyePosition - pin.current.PositionWS.xyz); // view vector
    const float3 L = normalize(-PBR_LightDirection[0]);               // light vector ("to light" oppositve of light's direction)
     
    // Before lighting, peturb the surface's normal by the one given in normal map.
    float3 localNormal = BiasX2(PBR_NormalTexture.Sample(PBR_SurfaceSampler, pin.current.TexCoord).xyz);
    
    float3 N = PeturbNormal( localNormal, pin.current.NormalWS, pin.current.TangentWS);

    // Get albedo, then roughness, metallic and ambient occlusion
    float4 albedo = PBR_AlbedoTexture.Sample(PBR_SurfaceSampler, pin.current.TexCoord);
    float3 RMA = PBR_RMATexture.Sample(PBR_SurfaceSampler, pin.current.TexCoord);
  
    // Shade surface
    float3 color = PBR_LightSurface(V, N, 3, PBR_LightColor, PBR_LightDirection, albedo.rgb, RMA.x, RMA.y, RMA.z);

    output.color = float4(color, albedo.w);
    
    // Calculate velocity of this point
    float4 prevPos = pin.prevPosition;
    prevPos.xyz /= prevPos.w;
    prevPos.xy *= float2(0.5f, -0.5f);
    prevPos.xy += 0.5f;
    prevPos.xy *= float2(PBR_TargetWidth, PBR_TargetHeight);

    output.velocity = PackVelocity(prevPos.xyz - pin.current.PositionPS.xyz);

    return output;
}
