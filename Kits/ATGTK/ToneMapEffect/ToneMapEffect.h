//--------------------------------------------------------------------------------------
// ToneMapEffect.h
//
// A simple flimic tonemapping effect for DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <Effects.h>

namespace ATG
{
    class ToneMapEffect : public DirectX::IEffect
    {
    public:
        // Use this to render tonemap quad (defined here so PSO can use it).
        using VertexType = DirectX::VertexPositionTexture;

        explicit ToneMapEffect(_In_ ID3D12Device* device, DXGI_FORMAT outputBufferFormat);
        ToneMapEffect(ToneMapEffect&& moveFrom);
        ToneMapEffect& operator= (ToneMapEffect&& moveFrom);

        ToneMapEffect(ToneMapEffect const&) = delete;
        ToneMapEffect& operator= (ToneMapEffect const&) = delete;

        virtual ~ToneMapEffect();
        
        void SetTexture(_In_ D3D12_GPU_DESCRIPTOR_HANDLE srvDescriptor, _In_ D3D12_GPU_DESCRIPTOR_HANDLE samplerDescriptor);
        
        void __cdecl Apply(_In_ ID3D12GraphicsCommandList* commandList) override;
    private:
        // Private implementation.
        class Impl;

        std::unique_ptr<Impl> pImpl;
    };
}