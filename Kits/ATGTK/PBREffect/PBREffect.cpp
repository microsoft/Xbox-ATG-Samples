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

// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "Compiled/XboxOnePBREffect_VSConstant.inc"
#include "Compiled/XboxOnePBREffect_VSConstantVelocity.inc"

#include "Compiled/XboxOnePBREffect_PSConstant.inc"
#include "Compiled/XboxOnePBREffect_PSTextured.inc"
#include "Compiled/XboxOnePBREffect_PSTexturedVelocity.inc"
         
#else    
#include "Compiled/PBREffect_VSConstant.inc"
#include "Compiled/PBREffect_VSConstantVelocity.inc"

#include "Compiled/PBREffect_PSConstant.inc"
#include "Compiled/PBREffect_PSTextured.inc"
#include "Compiled/PBREffect_PSTexturedVelocity.inc"

#endif
}

// Constant buffer layout. Must match the shader!
struct PBREffectConstants
{    
    XMVECTOR eyePosition;
    XMMATRIX world;
    XMVECTOR worldInverseTranspose[3];
    XMMATRIX worldViewProj;
    XMMATRIX prevWorldViewProj; // for velocity generation

    XMVECTOR lightDirection[IEffectLights::MaxDirectionalLights];           
    XMVECTOR lightDiffuseColor[IEffectLights::MaxDirectionalLights];
    
    // PBR Parameters
    XMVECTOR Albedo;
    float    Metallic;
    float    Roughness;
    int      numRadianceMipLevels;

    // Size of render target 
    float   targetWidth;
    float   targetHeight;
};

static_assert( ( sizeof(PBREffectConstants) % 16 ) == 0, "CB size not padded correctly" );


// Traits type describes our characteristics to the EffectBase template.
struct PBREffectTraits
{
    typedef PBREffectConstants ConstantBufferType;

    static const int VertexShaderCount = 2;
    static const int PixelShaderCount = 3;
    static const int ShaderPermutationCount = 3;
    static const int RootSignatureCount = 1;
};


// Internal PBREffect implementation class.
class ATG::PBREffect::Impl : public EffectBase<PBREffectTraits>
{
public:
    Impl(_In_ ID3D12Device* device, 
        int effectFlags, 
        const EffectPipelineStateDescription& pipelineDescription,
        bool generateVelocity);

    void Apply(_In_ ID3D12GraphicsCommandList* commandList);

    int GetPipelineStatePermutation(bool textureEnabled, bool velocityEnabled) const;

    static const int MaxDirectionalLights = 3;
    
    int flags;

    // When PBR moves into DirectXTK, this could become an effect flag.
    bool doGenerateVelocity;

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
    { PBREffect_VSConstantVelocity, sizeof(PBREffect_VSConstantVelocity) },
};


const int EffectBase<PBREffectTraits>::VertexShaderIndices[] =
{
    0,      // basic
    0,      // textured
    1,      // textured + velocity
};


const D3D12_SHADER_BYTECODE EffectBase<PBREffectTraits>::PixelShaderBytecode[] =
{
    { PBREffect_PSConstant, sizeof(PBREffect_PSConstant) },
    { PBREffect_PSTextured, sizeof(PBREffect_PSTextured) },
    { PBREffect_PSTexturedVelocity, sizeof(PBREffect_PSTexturedVelocity) }
};


const int EffectBase<PBREffectTraits>::PixelShaderIndices[] =
{
    0,      // basic
    1,      // textured
    2,      // textured + velocity
};

// Global pool of per-device PBREffect resources. Required by EffectBase<>, but not used.
SharedResourcePool<ID3D12Device*, EffectBase<PBREffectTraits>::DeviceResources> EffectBase<PBREffectTraits>::deviceResourcesPool;

// Constructor.
ATG::PBREffect::Impl::Impl(_In_ ID3D12Device* device, int effectFlags, const EffectPipelineStateDescription& pipelineDescription, bool generateVelocity)
    : EffectBase(device),
      flags(effectFlags),
      doGenerateVelocity(generateVelocity),
      descriptors{}
{
    static_assert(static_cast<int>(std::size(EffectBase<PBREffectTraits>::VertexShaderIndices)) == PBREffectTraits::ShaderPermutationCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<PBREffectTraits>::VertexShaderBytecode)) == PBREffectTraits::VertexShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<PBREffectTraits>::PixelShaderBytecode)) == PBREffectTraits::PixelShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<PBREffectTraits>::PixelShaderIndices)) == PBREffectTraits::ShaderPermutationCount, "array/max mismatch");

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

    for (size_t i = 0; i < std::size(textureSRV); i++)
    {
        rootParameters[i].InitAsDescriptorTable(1, &textureSRV[i]);
    }

    for (size_t i = 0; i < std::size(textureSampler); i++)
    {
        rootParameters[i + SurfaceSampler].InitAsDescriptorTable(1, &textureSampler[i]);
    }

    rootParameters[ConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rsigDesc;
    rsigDesc.Init(static_cast<UINT>(std::size(rootParameters)), rootParameters, 0, nullptr, rootSignatureFlags);

    mRootSignature = GetRootSignature(0, rsigDesc);

    // Create pipeline state
    int sp = GetPipelineStatePermutation((effectFlags & EffectFlags::Texture) != 0,
                                         doGenerateVelocity);
    int vi = EffectBase<PBREffectTraits>::VertexShaderIndices[sp];
    int pi = EffectBase<PBREffectTraits>::PixelShaderIndices[sp];
   
    pipelineDescription.CreatePipelineState(
        device,
        mRootSignature,
        EffectBase<PBREffectTraits>::VertexShaderBytecode[vi],
        EffectBase<PBREffectTraits>::PixelShaderBytecode[pi],
        mPipelineState.ReleaseAndGetAddressOf());
}


int ATG::PBREffect::Impl::GetPipelineStatePermutation(bool textureEnabled, bool velocityEnabled) const
{
    int permutation = 0;

    if (textureEnabled) permutation += 1;
    if (velocityEnabled) permutation = 2; // only textured velocity is supported

    return permutation;
}


// Sets our state onto the D3D device.
void ATG::PBREffect::Impl::Apply(_In_ ID3D12GraphicsCommandList* commandList)
{
    // Store old wvp for velocity calculation in shader
    constants.prevWorldViewProj = constants.worldViewProj;

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
ATG::PBREffect::PBREffect(_In_ ID3D12Device* device,
                     int effectFlags, 
                    const EffectPipelineStateDescription& pipelineDescription, 
                    bool generateVelocity)
    : pImpl(new Impl(device, effectFlags, pipelineDescription, generateVelocity))
{
}


// Move constructor.
ATG::PBREffect::PBREffect(PBREffect&& moveFrom)
  : pImpl(std::move(moveFrom.pImpl))
{
}


// Move assignment.
ATG::PBREffect& ATG::PBREffect::operator= (ATG::PBREffect&& moveFrom)
{
    pImpl = std::move(moveFrom.pImpl);
    return *this;
}


// Public destructor.
ATG::PBREffect::~PBREffect()
{
}

// IEffect methods.
void ATG::PBREffect::Apply(_In_ ID3D12GraphicsCommandList* commandList)
{
    pImpl->Apply(commandList);
}

// Camera settings.
void XM_CALLCONV ATG::PBREffect::SetWorld(FXMMATRIX value)
{
    pImpl->matrices.world = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV ATG::PBREffect::SetView(FXMMATRIX value)
{
    pImpl->matrices.view = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}


void XM_CALLCONV ATG::PBREffect::SetProjection(FXMMATRIX value)
{
    pImpl->matrices.projection = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj;
}


void XM_CALLCONV ATG::PBREffect::SetMatrices(FXMMATRIX world, CXMMATRIX view, CXMMATRIX projection)
{
    pImpl->matrices.world = world;
    pImpl->matrices.view = view;
    pImpl->matrices.projection = projection;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj | EffectDirtyFlags::WorldInverseTranspose | EffectDirtyFlags::EyePosition | EffectDirtyFlags::FogVector;
}

void XM_CALLCONV ATG::PBREffect::SetLightDirection(int whichLight, FXMVECTOR value)
{
    pImpl->constants.lightDirection[whichLight] = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}


void XM_CALLCONV ATG::PBREffect::SetLightColorAndIntensity(int whichLight, FXMVECTOR value)
{
    pImpl->constants.lightDiffuseColor[whichLight] = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}
 

void ATG::PBREffect::EnableDefaultLighting()
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
void ATG::PBREffect::SetConstantAlbedo(FXMVECTOR value)
{
    pImpl->constants.Albedo = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}

void ATG::PBREffect::SetConstantMetallic(float value)
{
    pImpl->constants.Metallic = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}

void ATG::PBREffect::SetConstantRoughness(float value)
{
    pImpl->constants.Roughness = value;

    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}

void ATG::PBREffect::SetSurfaceTextures(_In_ D3D12_GPU_DESCRIPTOR_HANDLE albedo,
    _In_ D3D12_GPU_DESCRIPTOR_HANDLE normal,
    _In_ D3D12_GPU_DESCRIPTOR_HANDLE RMA,
    _In_ D3D12_GPU_DESCRIPTOR_HANDLE sampler)
{
    pImpl->descriptors[Impl::RootParameterIndex::AlbedoTexture]  =   albedo;
    pImpl->descriptors[Impl::RootParameterIndex::NormalTexture]  =   normal;
    pImpl->descriptors[Impl::RootParameterIndex::RMATexture]     =   RMA;
    pImpl->descriptors[Impl::RootParameterIndex::SurfaceSampler] =   sampler;
}

void ATG::PBREffect::SetIBLTextures(_In_ D3D12_GPU_DESCRIPTOR_HANDLE radiance,
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

void ATG::PBREffect::SetRenderTargetSizeInPixels(int width, int height)
{
    pImpl->constants.targetWidth = static_cast<float>(width);
    pImpl->constants.targetHeight = static_cast<float>(height);
    pImpl->dirtyFlags |= EffectDirtyFlags::ConstantBuffer;
}

//--------------------------------------------------------------------------------------
// PBR 
const D3D12_INPUT_ELEMENT_DESC ATG::VertexPositionNormalTextureTangent::InputElements[] =
{
    { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static_assert(sizeof(ATG::VertexPositionNormalTextureTangent) == 48, "Vertex struct/layout mismatch");

const D3D12_INPUT_LAYOUT_DESC ATG::VertexPositionNormalTextureTangent::InputLayout =
{
    ATG::VertexPositionNormalTextureTangent::InputElements,
    ATG::VertexPositionNormalTextureTangent::InputElementCount
};
