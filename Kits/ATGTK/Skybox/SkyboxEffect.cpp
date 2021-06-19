//--------------------------------------------------------------------------------------
// SkyboxEffect.cpp
//
// A sky box rendering helper.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SkyboxEffect.h"

#include <..\Src\EffectCommon.h>

using namespace DirectX;
using namespace DX;

// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "Compiled/XboxOneSkyboxEffect_VS.inc"
#include "Compiled/XboxOneSkyboxEffect_PS.inc"
#else
#include "Compiled/SkyboxEffect_VS.inc"
#include "Compiled/SkyboxEffect_PS.inc"
#endif
}

struct __declspec(align(16)) SkyboxEffectConstants
{
    XMMATRIX worldViewProj;
};

// Traits type describes our characteristics to the EffectBase template.
struct SkyboxEffectTraits
{
    typedef SkyboxEffectConstants ConstantBufferType;

    static const int VertexShaderCount = 1;
    static const int PixelShaderCount = 1;
    static const int ShaderPermutationCount = 1;
    static const int RootSignatureCount = 1;
};


class SkyboxEffect::Impl : public EffectBase<SkyboxEffectTraits>
{
public:

    Impl(_In_ ID3D12Device* device, const EffectPipelineStateDescription& pipelineStateDesc);
    void Apply(_In_ ID3D12GraphicsCommandList* commandList);

    int GetPipelineStatePermutation() const;

    enum Descriptors
    {
        InputSRV,
        InputSampler,
        ConstantBuffer,
        DescriptorsCount
    };

    D3D12_GPU_DESCRIPTOR_HANDLE descriptors[DescriptorsCount];
};

const D3D12_SHADER_BYTECODE EffectBase<SkyboxEffectTraits>::VertexShaderBytecode[] =
{
    { SkyboxEffect_VS, sizeof(SkyboxEffect_VS) },
};


const int EffectBase<SkyboxEffectTraits>::VertexShaderIndices[] =
{
    0,      // basic
};


const D3D12_SHADER_BYTECODE EffectBase<SkyboxEffectTraits>::PixelShaderBytecode[] =
{
    { SkyboxEffect_PS, sizeof(SkyboxEffect_PS) },
};


const int EffectBase<SkyboxEffectTraits>::PixelShaderIndices[] =
{
    0,      // basic
};

// Global pool of per-device SkyboxEffect resources.
SharedResourcePool<ID3D12Device*, EffectBase<SkyboxEffectTraits>::DeviceResources> EffectBase<SkyboxEffectTraits>::deviceResourcesPool;


SkyboxEffect::Impl::Impl(_In_ ID3D12Device* device, const EffectPipelineStateDescription& pipelineStateDesc)
    : EffectBase(device),
    descriptors{}
{
    static_assert(static_cast<int>(std::size(EffectBase<SkyboxEffectTraits>::VertexShaderIndices)) == SkyboxEffectTraits::ShaderPermutationCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<SkyboxEffectTraits>::VertexShaderBytecode)) == SkyboxEffectTraits::VertexShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<SkyboxEffectTraits>::PixelShaderBytecode)) == SkyboxEffectTraits::PixelShaderCount, "array/max mismatch");
    static_assert(static_cast<int>(std::size(EffectBase<SkyboxEffectTraits>::PixelShaderIndices)) == SkyboxEffectTraits::ShaderPermutationCount, "array/max mismatch");

    // Create root signature
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Only the input assembler stage needs access to the constant buffer.
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

    CD3DX12_DESCRIPTOR_RANGE textureSRVs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE textureSamplers(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[3];
    rootParameters[InputSRV].InitAsDescriptorTable(1, &textureSRVs);
    rootParameters[InputSampler].InitAsDescriptorTable(1, &textureSamplers);
    rootParameters[ConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rsigDesc;
    rsigDesc.Init(static_cast<UINT>(std::size(rootParameters)), rootParameters, 0, nullptr, rootSignatureFlags);

    // Create the root signature
    mRootSignature = GetRootSignature(0, rsigDesc);

    // Get shaders
    int sp = GetPipelineStatePermutation();
    int vi = EffectBase<SkyboxEffectTraits>::VertexShaderIndices[sp];
    int pi = EffectBase<SkyboxEffectTraits>::PixelShaderIndices[sp];

    pipelineStateDesc.CreatePipelineState(
        device,
        mRootSignature,
        EffectBase<SkyboxEffectTraits>::VertexShaderBytecode[vi],
        EffectBase<SkyboxEffectTraits>::PixelShaderBytecode[pi],
        mPipelineState.ReleaseAndGetAddressOf());
}

int SkyboxEffect::Impl::GetPipelineStatePermutation() const
{
    return 0;
}


// Sets our state onto the D3D device.
void SkyboxEffect::Impl::Apply(_In_ ID3D12GraphicsCommandList* commandList)
{
    matrices.SetConstants(dirtyFlags, constants.worldViewProj);

    // Set constants to GPU
    UpdateConstants();

    // Set the root signature
    commandList->SetGraphicsRootSignature(mRootSignature);

    // Set the root parameters
    commandList->SetGraphicsRootDescriptorTable(InputSRV, descriptors[InputSRV]);
    commandList->SetGraphicsRootDescriptorTable(InputSampler, descriptors[InputSampler]);

    // Set constants
    commandList->SetGraphicsRootConstantBufferView(ConstantBuffer, GetConstantBufferGpuAddress());

    // Set the pipeline state
    commandList->SetPipelineState(EffectBase::mPipelineState.Get());
}

// Public constructor.
SkyboxEffect::SkyboxEffect(_In_ ID3D12Device* device, const EffectPipelineStateDescription& pipelineStateDesc)
    : pImpl(new Impl(device, pipelineStateDesc))
{
}

// Move constructor.
SkyboxEffect::SkyboxEffect(SkyboxEffect&& moveFrom)
    : pImpl(std::move(moveFrom.pImpl))
{
}


// Move assignment.
SkyboxEffect& SkyboxEffect::operator= (SkyboxEffect&& moveFrom)
{
    pImpl = std::move(moveFrom.pImpl);
    return *this;
}


// Public destructor.
SkyboxEffect::~SkyboxEffect()
{
}

// IEffect methods.
void SkyboxEffect::Apply(_In_ ID3D12GraphicsCommandList* commandList)
{
    pImpl->Apply(commandList);
}

// Texture settings.
void SkyboxEffect::SetTexture(_In_ D3D12_GPU_DESCRIPTOR_HANDLE srvDescriptor, _In_ D3D12_GPU_DESCRIPTOR_HANDLE samplerDescriptor)
{
    pImpl->descriptors[Impl::InputSRV]     = srvDescriptor;
    pImpl->descriptors[Impl::InputSampler] = samplerDescriptor;
}

void XM_CALLCONV SkyboxEffect::SetMatrices(FXMMATRIX view, CXMMATRIX projection)
{
    // Skybox remove translation, preserve rotation
    DirectX::SimpleMath::Matrix v = view;
    v.Translation(DirectX::SimpleMath::Vector3(0, 0, 0));

    // Skybox has no world matrix
    pImpl->matrices.world = XMMatrixIdentity();
    pImpl->matrices.view = v;
    pImpl->matrices.projection = projection;

    pImpl->dirtyFlags |= EffectDirtyFlags::WorldViewProj;
}
