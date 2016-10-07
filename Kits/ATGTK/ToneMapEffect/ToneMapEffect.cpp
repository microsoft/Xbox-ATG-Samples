//--------------------------------------------------------------------------------------
// ToneMapEffect.cpp
//
// A simple flimic tonemapping effect for DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "ToneMapEffect.h"

#include <..\Src\EffectCommon.h>

using namespace DirectX;
using namespace ATG;

// Include the precompiled shader code.
namespace
{
#if defined(_XBOX_ONE) && defined(_TITLE)
#include "Compiled/XboxOneToneMapEffect_VS.inc"
#include "Compiled/XboxOneToneMapEffect_PS.inc"
#else
#include "Compiled/ToneMapEffect_VS.inc"
#include "Compiled/ToneMapEffect_PS.inc"
#endif
}

struct __declspec(align(16)) ToneMapEffectConstants
{
// no constants
    XMVECTOR _dummy;
};

// Traits type describes our characteristics to the EffectBase template.
struct ToneMapEffectTraits
{
    typedef ToneMapEffectConstants ConstantBufferType;

    static const int VertexShaderCount = 1;
    static const int PixelShaderCount = 1;
    static const int ShaderPermutationCount = 1;
    static const int RootSignatureCount = 1;
};


class ATG::ToneMapEffect::Impl : public EffectBase<ToneMapEffectTraits>
{
public:

    Impl(_In_ ID3D12Device* device, DXGI_FORMAT outputTargetFormat);
    void Apply(_In_ ID3D12GraphicsCommandList* commandList);

    int GetPipelineStatePermutation() const;

    enum Descriptors
    {
        InputSRV,
        InputSampler,
        DescriptorsCount
    };

    D3D12_GPU_DESCRIPTOR_HANDLE descriptors[DescriptorsCount];
};

const D3D12_SHADER_BYTECODE EffectBase<ToneMapEffectTraits>::VertexShaderBytecode[] =
{
    { ToneMapEffect_VS, sizeof(ToneMapEffect_VS) },
};


const int EffectBase<ToneMapEffectTraits>::VertexShaderIndices[] =
{
    0,      // basic
};


const D3D12_SHADER_BYTECODE EffectBase<ToneMapEffectTraits>::PixelShaderBytecode[] =
{
    { ToneMapEffect_PS, sizeof(ToneMapEffect_PS) },
};


const int EffectBase<ToneMapEffectTraits>::PixelShaderIndices[] =
{
    0,      // basic
};

// Global pool of per-device ToneMapEffect resources.
SharedResourcePool<ID3D12Device*, EffectBase<ToneMapEffectTraits>::DeviceResources> EffectBase<ToneMapEffectTraits>::deviceResourcesPool;


ToneMapEffect::Impl::Impl(_In_ ID3D12Device* device, DXGI_FORMAT outputTargetFormat)
    : EffectBase(device),
    descriptors{}
{
    static_assert(_countof(EffectBase<ToneMapEffectTraits>::VertexShaderIndices) == ToneMapEffectTraits::ShaderPermutationCount, "array/max mismatch");
    static_assert(_countof(EffectBase<ToneMapEffectTraits>::VertexShaderBytecode) == ToneMapEffectTraits::VertexShaderCount, "array/max mismatch");
    static_assert(_countof(EffectBase<ToneMapEffectTraits>::PixelShaderBytecode) == ToneMapEffectTraits::PixelShaderCount, "array/max mismatch");
    static_assert(_countof(EffectBase<ToneMapEffectTraits>::PixelShaderIndices) == ToneMapEffectTraits::ShaderPermutationCount, "array/max mismatch");

    // Create root signature
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // Only the input assembler stage needs access to the constant buffer.
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

    CD3DX12_DESCRIPTOR_RANGE textureSRVs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE textureSamplers(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[2];
    rootParameters[0].InitAsDescriptorTable(1, &textureSRVs);
    rootParameters[1].InitAsDescriptorTable(1, &textureSamplers);
    
    CD3DX12_ROOT_SIGNATURE_DESC rsigDesc;
    rsigDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    // Create the root signature
    mRootSignature = GetRootSignature(0, rsigDesc);

    // Get shaders
    int sp = GetPipelineStatePermutation();
    int vi = EffectBase<ToneMapEffectTraits>::VertexShaderIndices[sp];
    int pi = EffectBase<ToneMapEffectTraits>::PixelShaderIndices[sp];

    // Create pipeline state
    RenderTargetState rtState(outputTargetFormat, DXGI_FORMAT_UNKNOWN); // no DSV

    EffectPipelineStateDescription tonemapPSD(&VertexType::InputLayout,
        CommonStates::Opaque,
        CommonStates::DepthNone,
        CommonStates::CullNone,
        rtState,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

    tonemapPSD.CreatePipelineState(
        device,
        mRootSignature,
        EffectBase<ToneMapEffectTraits>::VertexShaderBytecode[vi],
        EffectBase<ToneMapEffectTraits>::PixelShaderBytecode[pi],
        mPipelineState.ReleaseAndGetAddressOf());
}

int ToneMapEffect::Impl::GetPipelineStatePermutation() const
{
    return 0;
}


// Sets our state onto the D3D device.
void ToneMapEffect::Impl::Apply(_In_ ID3D12GraphicsCommandList* commandList)
{
    // Set the root signature
    commandList->SetGraphicsRootSignature(mRootSignature);

    // Set the root parameters
    // TODO don't assume these are contiguous
    commandList->SetGraphicsRootDescriptorTable(0, descriptors[InputSRV]);
    commandList->SetGraphicsRootDescriptorTable(1, descriptors[InputSampler]);

    // Set the pipeline state
    commandList->SetPipelineState(EffectBase::mPipelineState.Get());
}

// Public constructor.
ToneMapEffect::ToneMapEffect(_In_ ID3D12Device* device, DXGI_FORMAT outputTargetFormat)
    : pImpl(new Impl(device, outputTargetFormat))
{
}

// Move constructor.
ToneMapEffect::ToneMapEffect(ToneMapEffect&& moveFrom)
    : pImpl(std::move(moveFrom.pImpl))
{
}


// Move assignment.
ToneMapEffect& ToneMapEffect::operator= (ToneMapEffect&& moveFrom)
{
    pImpl = std::move(moveFrom.pImpl);
    return *this;
}


// Public destructor.
ToneMapEffect::~ToneMapEffect()
{
}

// IEffect methods.
void ToneMapEffect::Apply(_In_ ID3D12GraphicsCommandList* commandList)
{
    pImpl->Apply(commandList);
}

// Texture settings.
void ToneMapEffect::SetTexture(_In_ D3D12_GPU_DESCRIPTOR_HANDLE srvDescriptor, _In_ D3D12_GPU_DESCRIPTOR_HANDLE samplerDescriptor)
{
    pImpl->descriptors[Impl::InputSRV]     = srvDescriptor;
    pImpl->descriptors[Impl::InputSampler] = samplerDescriptor;
}