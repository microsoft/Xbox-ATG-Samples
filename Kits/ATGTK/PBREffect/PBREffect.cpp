//--------------------------------------------------------------------------------------
// PBREffect.cpp
//
// A physically based shader for forward rendering on DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "PBREffect.h"
#include <..\Src\EffectCommon.h>

using namespace DirectX;
using namespace ATG;

// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "Compiled/XboxOnePBREffect_VSConstant.inc"
         
#include "Compiled/XboxOnePBREffect_PSConstant.inc"
#include "Compiled/XboxOnePBREffect_PSTextured.inc"
         
#else    
#include "Compiled/PBREffect_VSConstant.inc"
       
#include "Compiled/PBREffect_PSConstant.inc"
#include "Compiled/PBREffect_PSTextured.inc"
#endif
}

// Constant buffer layout. Must match the shader!
struct PBREffectConstants
{    
    XMVECTOR eyePosition;
    XMMATRIX world;
    XMVECTOR worldInverseTranspose[3];
    XMMATRIX worldViewProj;

    XMVECTOR lightDirection[IEffectLights::MaxDirectionalLights];           
    XMVECTOR lightDiffuseColor[IEffectLights::MaxDirectionalLights];
    
    // New PBR Parameters
    XMVECTOR Albedo;
    float    Metallic;
    float    Roughness;

    int     numRadianceMipLevels;
};

static_assert( ( sizeof(PBREffectConstants) % 16 ) == 0, "CB size not padded correctly" );


// Traits type describes our characteristics to the EffectBase template.
struct PBREffectTraits
{
    typedef PBREffectConstants ConstantBufferType;

    static const int VertexShaderCount = 1;
    static const int PixelShaderCount = 2;
    static const int ShaderPermutationCount = 2;
    static const int RootSignatureCount = 1;
};


// Internal PBREffect implementation class.
class ATG::PBREffect::Impl : public EffectBase<PBREffectTraits>
{
public:
    Impl(_In_ ID3D12Device* device, int effectFlags, const EffectPipelineStateDescription& pipelineDescription);
    void Apply(_In_ ID3D12GraphicsCommandList* commandList);

    int GetPipelineStatePermutation(bool textureEnabled) const;

    static const int MaxDirectionalLights = 3;
    
    int flags;

    enum RootParameterIndex
    {
        AlbedoTexture,
        NormalTexture,
        RMATexture,
        RadianceTexture,
        IrradianceTexture,
        SurfaceSampler,
        RadianceSampler,
        ConstantBuffer,
        RootParametersCount
    };

    D3D12_GPU_DESCRIPTOR_HANDLE descriptors[RootParametersCount];
};


const D3D12_SHADER_BYTECODE EffectBase<PBREffectTraits>::VertexShaderBytecode[] =
{
    { PBREffect_VSConstant, sizeof(PBREffect_VSConstant) },

};


const int EffectBase<PBREffectTraits>::VertexShaderIndices[] =
{
    0,      // basic
    0,      // textured
};


const D3D12_SHADER_BYTECODE EffectBase<PBREffectTraits>::PixelShaderBytecode[] =
{
    { PBREffect_PSConstant, sizeof(PBREffect_PSConstant) },
    { PBREffect_PSTextured, sizeof(PBREffect_PSTextured) },
};


const int EffectBase<PBREffectTraits>::PixelShaderIndices[] =
{
    0,      // basic
    1,      // textured
};

// Global pool of per-device PBREffect resources. Required by EffectBase<>, but not used.
SharedResourcePool<ID3D12Device*, EffectBase<PBREffectTraits>::DeviceResources> EffectBase<PBREffectTraits>::deviceResourcesPool;

// Constructor.
PBREffect::Impl::Impl(_In_ ID3D12Device* device, int effectFlags, const EffectPipelineStateDescription& pipelineDescription)
    : EffectBase(device),
      flags(effectFlags),
    descriptors{}
{
    static_assert( _countof(EffectBase<PBREffectTraits>::VertexShaderIndices) == PBREffectTraits::ShaderPermutationCount, "array/max mismatch" );
    static_assert( _countof(EffectBase<PBREffectTraits>::VertexShaderBytecode) == PBREffectTraits::VertexShaderCount, "array/max mismatch" );
    static_assert( _countof(EffectBase<PBREffectTraits>::PixelShaderBytecode) == PBREffectTraits::PixelShaderCount, "array/max mismatch" );
    static_assert( _countof(EffectBase<PBREffectTraits>::PixelShaderIndices) == PBREffectTraits::ShaderPermutationCount, "array/max mismatch" );

    // Lighting
    static const XMVECTORF32 defaultLightDirection = { 0, -1, 0, 0 };
    for (int i = 0; i < MaxDirectionalLights; i++)
    {
        constants.lightDirection[i] = defaultLightDirection;
        constants.lightDiffuseColor[i] = g_XMZero;
    }

    // Default PBR values
    constants.Albedo = XMVectorSet(1, 1, 1, 1);
    constants.Metallic = 0.5f;
    constants.Roughness = 0.2f;
    constants.numRadianceMipLevels = 1;

    // Create root signature
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Only the input assembler stage needs access to the constant buffer.
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

    CD3DX12_ROOT_PARAMETER rootParameters[RootParametersCount];
    CD3DX12_DESCRIPTOR_RANGE textureSRV[5] = {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0),
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1),
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2),
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3),
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4),
    };

    CD3DX12_DESCRIPTOR_RANGE textureSampler[2] = {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0),
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 1)
    };

    for (int i = 0; i < _countof(textureSRV); i++)
    {
        rootParameters[i].InitAsDescriptorTable(1, &textureSRV[i]);
    }

    for (int i = 0; i < _countof(textureSampler); i++)
    {
        rootParameters[i + SurfaceSampler].InitAsDescriptorTable(1, &textureSampler[i]);
    }

    rootParameters[ConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rsigDesc;
    rsigDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    mRootSignature = GetRootSignature(0, rsigDesc);

    // Create pipeline state
    int sp = GetPipelineStatePermutation((effectFlags & EffectFlags::Texture) != 0);
    int vi = EffectBase<PBREffectTraits>::VertexShaderIndices[sp];
    int pi = EffectBase<PBREffectTraits>::PixelShaderIndices[sp];
   
    pipelineDescription.CreatePipelineState(
        device,
        mRootSignature,
        EffectBase<PBREffectTraits>::VertexShaderBytecode[vi],
        EffectBase<PBREffectTraits>::PixelShaderBytecode[pi],
        mPipelineState.ReleaseAndGetAddressOf());
}


int PBREffect::Impl::GetPipelineStatePermutation(bool textureEnabled) const
{
    int permutation = 0;

    if (textureEnabled) permutation += 1;

    return permutation;
}


// Sets our state onto the D3D device.
void PBREffect::Impl::Apply(_In_ ID3D12GraphicsCommandList* commandList)
{
    // Compute derived parameter values.
    matrices.SetConstants(dirtyFlags, constants.worldViewProj);        
       
    // World inverse transpose matrix.
    if (dirtyFlags & EffectDirtyFlags::WorldInverseTranspose)
    {
        constants.world = XMMatrixTranspose(matrices.world);

        XMMATRIX worldInverse = XMMatrixInverse(nullptr, matrices.world);

        constants.worldInverseTranspose[0] = worldInverse.r[0];
        constants.worldInverseTranspose[1] = worldInverse.r[1];
        constants.worldInverseTranspose[2] = worldInverse.r[2];

        dirtyFlags &= ~EffectDirtyFlags::WorldInverseTranspose;
        dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
    }

    // Eye position vector.
    if (dirtyFlags & EffectDirtyFlags::EyePosition)
    {
        XMMATRIX viewInverse = XMMatrixInverse(nullptr, matrices.view);

        constants.eyePosition = viewInverse.r[3];

        dirtyFlags &= ~EffectDirtyFlags::EyePosition;
        dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
    }

    // Set constants to GPU
    UpdateConstants();

    // Set the root signature
    commandList->SetGraphicsRootSignature(mRootSignature);

    // Set the root parameters
    if ((flags & EffectFlags::Texture) == 0)
    {
        // only update radiance texture and samples
        commandList->SetGraphicsRootDescriptorTable(RadianceTexture, descriptors[RadianceTexture]);
        commandList->SetGraphicsRootDescriptorTable(IrradianceTexture, descriptors[IrradianceTexture]);
        commandList->SetGraphicsRootDescriptorTable(RadianceSampler, descriptors[RadianceSampler]);
    }
    else
    {
        // Update everything
        for (int i = 0; i < ConstantBuffer; i++)
        {
            commandList->SetGraphicsRootDescriptorTable(i, descriptors[i]);
        }
    }

    // Set constants
    commandList->SetGraphicsRootConstantBufferView(ConstantBuffer, GetConstantBufferGpuAddress());

    // Set the pipeline state
    commandList->SetPipelineState(EffectBase::mPipelineState.Get());
}

// Public constructor.
PBREffect::PBREffect(_In_ ID3D12Device* device, int effectFlags, const EffectPipelineStateDescription& pipelineDescription)
    : pImpl(new Impl(device, effectFlags, pipelineDescription))
{
}


// Move constructor.
PBREffect::PBREffect(PBREffect&& moveFrom)
  : pImpl(std::move(moveFrom.pImpl))
{
}


// Move assignment.
PBREffect& PBREffect::operator= (PBREffect&& moveFrom)
{
    pImpl = std::move(moveFrom.pImpl);
    return *this;
}


// Public destructor.
PBREffect::~PBREffect()
{
}

// IEffect methods.
void PBREffect::Apply(_In_ ID3D12GraphicsCommandList* commandList)
{
    pImpl->Apply(commandList);
}

// Camera settings.
void XM_CALLCONV PBREffect::SetWorld(FXMMATRIX value)
{
    pImpl->matrices.world = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV PBREffect::SetView(FXMMATRIX value)
{
    pImpl->matrices.view = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV PBREffect::SetProjection(FXMMATRIX value)
{
    pImpl->matrices.projection = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj;
}


void XM_CALLCONV PBREffect::SetMatrices(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection)
{
    pImpl->matrices.world = world;
    pImpl->matrices.view = view;
    pImpl->matrices.projection = projection;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}

void XM_CALLCONV PBREffect::SetLightDirection(int whichLight, FXMVECTOR value)
{
    pImpl->constants.lightDirection[whichLight] = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void XM_CALLCONV PBREffect::SetLightColorAndIntensity(int whichLight, FXMVECTOR value)
{
    pImpl->constants.lightDiffuseColor[whichLight] = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}
 

void PBREffect::EnableDefaultLighting()
{
    static const XMVECTORF32 defaultDirections[Impl::MaxDirectionalLights] =
    {
        { -0.5265408f, -0.5735765f, -0.6275069f },
        { 0.7198464f,  0.3420201f,  0.6040227f },
        { 0.4545195f, -0.7660444f,  0.4545195f },
    };

    static const XMVECTORF32 defaultDiffuse[Impl::MaxDirectionalLights] =
    {
        { 1.0000000f, 0.9607844f, 0.8078432f },
        { 0.9647059f, 0.7607844f, 0.4078432f },
        { 0.3231373f, 0.3607844f, 0.3937255f },
    };

    for (int i = 0; i < Impl::MaxDirectionalLights; i++)
    {
        SetLightDirection(i, defaultDirections[i]);
        SetLightColorAndIntensity(i, defaultDiffuse[i]);
    }
}


// PBR Settings
void PBREffect::SetConstantAlbedo(FXMVECTOR value)
{
    pImpl->constants.Albedo = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}

void PBREffect::SetConstantMetallic(float value)
{
    pImpl->constants.Metallic = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}

void PBREffect::SetConstantRoughness(float value)
{
    pImpl->constants.Roughness = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}

#ifdef DEBUG
void PBREffect::SetDebugFlags(bool diffuse, bool D, bool F, bool G)
{
    pImpl->constants.enable_Diffuse = diffuse;
    pImpl->constants.enable_Specular_D = D;
    pImpl->constants.enable_Specular_F = F;
    pImpl->constants.enable_Specular_G = G;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}
#endif

void PBREffect::SetSurfaceTextures(_In_ D3D12_GPU_DESCRIPTOR_HANDLE albedo,
    _In_ D3D12_GPU_DESCRIPTOR_HANDLE normal,
    _In_ D3D12_GPU_DESCRIPTOR_HANDLE RMA,
    _In_ D3D12_GPU_DESCRIPTOR_HANDLE sampler)
{
    pImpl->descriptors[Impl::RootParameterIndex::AlbedoTexture]  =   albedo;
    pImpl->descriptors[Impl::RootParameterIndex::NormalTexture]  =   normal;
    pImpl->descriptors[Impl::RootParameterIndex::RMATexture]     =   RMA;
    pImpl->descriptors[Impl::RootParameterIndex::SurfaceSampler] =   sampler;
}

void PBREffect::SetIBLTextures(_In_ D3D12_GPU_DESCRIPTOR_HANDLE radiance,
                                int numRadianceMips,
                                _In_ D3D12_GPU_DESCRIPTOR_HANDLE irradiance,
                                _In_ D3D12_GPU_DESCRIPTOR_HANDLE sampler)
{
    pImpl->descriptors[Impl::RootParameterIndex::RadianceTexture] = radiance;
    pImpl->descriptors[Impl::RootParameterIndex::RadianceSampler] = sampler;
    pImpl->constants.numRadianceMipLevels = numRadianceMips;

    pImpl->descriptors[Impl::RootParameterIndex::IrradianceTexture] = irradiance;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}

//--------------------------------------------------------------------------------------
// PBR 
const D3D12_INPUT_ELEMENT_DESC VertexPositionNormalTextureTangent::InputElements[] =
{
    { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static_assert(sizeof(VertexPositionNormalTextureTangent) == 48, "Vertex struct/layout mismatch");

const D3D12_INPUT_LAYOUT_DESC VertexPositionNormalTextureTangent::InputLayout =
{
    VertexPositionNormalTextureTangent::InputElements,
    VertexPositionNormalTextureTangent::InputElementCount
};