//--------------------------------------------------------------------------------------
// SkyboxEffect.h
//
// A sky box rendering helper for.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#pragma once

#include <Effects.h>

namespace DX
{
    class SkyboxEffect : public DirectX::IEffect
    {
    public:
        // Use this to render tonemap quad (defined here so PSO can use it).
        using VertexType = DirectX::VertexPositionTexture;

        explicit SkyboxEffect(_In_ ID3D12Device* device, const DirectX::EffectPipelineStateDescription& pipelineStateDesc);
        SkyboxEffect(SkyboxEffect&& moveFrom);
        SkyboxEffect& operator= (SkyboxEffect&& moveFrom);

        SkyboxEffect(SkyboxEffect const&) = delete;
        SkyboxEffect& operator= (SkyboxEffect const&) = delete;

        virtual ~SkyboxEffect();
        
        void SetTexture(_In_ D3D12_GPU_DESCRIPTOR_HANDLE srvDescriptor, _In_ D3D12_GPU_DESCRIPTOR_HANDLE samplerDescriptor);
        
        void XM_CALLCONV SetMatrices(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection);

        void __cdecl Apply(_In_ ID3D12GraphicsCommandList* commandList) override;
    private:
        // Private implementation.
        class Impl;

        std::unique_ptr<Impl> pImpl;
    };
}