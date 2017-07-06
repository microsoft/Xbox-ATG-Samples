//--------------------------------------------------------------------------------------
// Skybox.h
//
// A sky box rendering helper for DirectX 12. Takes DDS cubemap as input. 
// 
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <GeometricPrimitive.h>
#include "SkyboxEffect.h"

namespace ATG
{
    class Skybox
    {
    public:
        Skybox(
            ID3D12Device* device,
            D3D12_GPU_DESCRIPTOR_HANDLE cubeTexture,
            const DirectX::RenderTargetState& rtState,
            const DirectX::CommonStates& commonStates)
        {
            using namespace DirectX;
            using namespace DirectX::SimpleMath;

            // Skybox effect
            EffectPipelineStateDescription skyPSD(&ATG::SkyboxEffect::VertexType::InputLayout,
                CommonStates::Opaque,
                CommonStates::DepthRead,
                CommonStates::CullClockwise,
                rtState,
                D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

            m_effect = std::make_unique<ATG::SkyboxEffect>(device, skyPSD);
            m_effect->SetTexture(cubeTexture, commonStates.LinearWrap());

            // "Skybox" geometry
            m_sky = DirectX::GeometricPrimitive::CreateGeoSphere(2.f);
        }

        void XM_CALLCONV Update(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection)
        {
            m_effect->SetMatrices(view, projection);
        }

        void Render(ID3D12GraphicsCommandList* cmdList)
        {
            m_effect->Apply(cmdList);
            m_sky->Draw(cmdList);
        }

    private:
        std::unique_ptr<DirectX::GeometricPrimitive>    m_sky;
        std::unique_ptr<ATG::SkyboxEffect>              m_effect;
    };
}