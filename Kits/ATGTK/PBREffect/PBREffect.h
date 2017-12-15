//--------------------------------------------------------------------------------------
// PBREffect.h
//
// A physically based shader for forward rendering on DirectX 12.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <Effects.h>

namespace ATG
{
    class PBREffect : public DirectX::IEffect
    {
    public:
        explicit PBREffect(_In_ ID3D12Device* device, int effectFlags, 
            const DirectX::EffectPipelineStateDescription& pipelineDescription,
            bool generateVelocity = false);
        PBREffect(PBREffect&& moveFrom);
        PBREffect& operator= (PBREffect&& moveFrom);

        PBREffect(PBREffect const&) = delete;
        PBREffect& operator= (PBREffect const&) = delete;

        virtual ~PBREffect();

        // IEffect methods.
        void __cdecl Apply(_In_ ID3D12GraphicsCommandList* commandList) override;

        // Camera settings.
        void XM_CALLCONV SetWorld(DirectX::FXMMATRIX value);
        void XM_CALLCONV SetView(DirectX::FXMMATRIX value);
        void XM_CALLCONV SetProjection(DirectX::FXMMATRIX value);
        void XM_CALLCONV SetMatrices(DirectX::FXMMATRIX world, DirectX::CXMMATRIX view, DirectX::CXMMATRIX projection);

        // Light settings.
        void __cdecl     SetLightEnabled(int whichLight, bool value);
        void XM_CALLCONV SetLightDirection(int whichLight, DirectX::FXMVECTOR value);
        void XM_CALLCONV SetLightColorAndIntensity(int whichLight, DirectX::FXMVECTOR value);

        // Set up a default light rig
        void __cdecl EnableDefaultLighting();

        // PBR Settings
        void XM_CALLCONV SetConstantAlbedo(DirectX::FXMVECTOR value);
        void __cdecl     SetConstantMetallic(float value);
        void __cdecl     SetConstantRoughness(float value);

        // Texture settings.
        void __cdecl SetSurfaceTextures(
            _In_ D3D12_GPU_DESCRIPTOR_HANDLE albedo,
            _In_ D3D12_GPU_DESCRIPTOR_HANDLE normal,
            _In_ D3D12_GPU_DESCRIPTOR_HANDLE RoughnessMetallicAmbientOcclusion,
            _In_ D3D12_GPU_DESCRIPTOR_HANDLE sampler);

        void __cdecl SetIBLTextures(
            _In_ D3D12_GPU_DESCRIPTOR_HANDLE radiance,
            int numRadianceMips,
            _In_ D3D12_GPU_DESCRIPTOR_HANDLE irradiance,
            _In_ D3D12_GPU_DESCRIPTOR_HANDLE sampler);

        // Render target size, required for velocity buffer output
        void __cdecl SetRenderTargetSizeInPixels(int width, int height);

    private:
        // Private implementation.
        class Impl;

        std::unique_ptr<Impl> pImpl;
    };

    // For PBR effects
    struct VertexPositionNormalTextureTangent
    {
        VertexPositionNormalTextureTangent() = default;

        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 textureCoordinate;
        DirectX::XMFLOAT4 tangent;

        VertexPositionNormalTextureTangent(DirectX::XMFLOAT3 const& position, 
            DirectX::XMFLOAT3 const& normal, 
            DirectX::XMFLOAT2 const& textureCoordinate, 
            DirectX::XMFLOAT4 const& tangent)
            : position(position),
            normal(normal),
            textureCoordinate(textureCoordinate),
            tangent(tangent)
        {
        }

        VertexPositionNormalTextureTangent(DirectX::FXMVECTOR position, 
            DirectX::FXMVECTOR normal, 
            DirectX::CXMVECTOR textureCoordinate, 
            DirectX::FXMVECTOR tangent)
        {
            XMStoreFloat3(&this->position, position);
            XMStoreFloat3(&this->normal, normal);
            XMStoreFloat2(&this->textureCoordinate, textureCoordinate);
            XMStoreFloat4(&this->tangent, tangent);
        }

        static const D3D12_INPUT_LAYOUT_DESC InputLayout;

    private:
        static const int InputElementCount = 4;
        static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
    };
}