//--------------------------------------------------------------------------------------
// EsramVisualizeEffect.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "EsramVisualizeEffect.h"

using namespace DirectX;
using namespace Microsoft::WRL;

namespace ATG
{
    const wchar_t* EsramVisualizeEffect::s_shaderFilename = L"EsramVisualize_CS.cso";
    const XMINT2 EsramVisualizeEffect::s_groupSize = XMINT2(8, 8);

    EsramVisualizeEffect::EsramVisualizeEffect(ID3D12Device* device)
    {
        assert(device != nullptr);

        auto shaderBlob = DX::ReadData(s_shaderFilename);

        // Create root signature.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | 
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        CD3DX12_DESCRIPTOR_RANGE textureUAVs(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

        CD3DX12_ROOT_PARAMETER rootParameters[RootParameterIndex::RootParameterCount] = {};
        rootParameters[RootParameterIndex::ConstantBuffer].InitAsConstantBufferView(0);
        rootParameters[RootParameterIndex::TextureUAV].InitAsDescriptorTable(1, &textureUAVs);

        CD3DX12_ROOT_SIGNATURE_DESC rsDesc = {};
        rsDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        ComPtr<ID3DBlob> rsBlob, errorBlob;
        DX::ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &errorBlob));
        DX::ThrowIfFailed(device->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_GRAPHICS_PPV_ARGS(m_rootSignature.GetAddressOf())));

        SetDebugObjectName(m_rootSignature.Get(), L"EsramVisualizeEffect");
        
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = m_rootSignature.Get();
        desc.CS = { shaderBlob.data(), shaderBlob.size() };

        DX::ThrowIfFailed(device->CreateComputePipelineStateX(&desc, 0, nullptr, IID_GRAPHICS_PPV_ARGS(m_pipelineState.GetAddressOf())));

        SetDebugObjectName(m_pipelineState.Get(), L"EsramVisualizeEffect");
    }

    void EsramVisualizeEffect::Process(ID3D12GraphicsCommandList* commandList)
    {
        // Set pipeline
        commandList->SetComputeRootSignature(m_rootSignature.Get());
        commandList->SetPipelineState(m_pipelineState.Get());

        // Set constant buffer
        if (m_dirtyFlag)
        {
            ComPtr<ID3D12Device> device;
            commandList->GetDevice(IID_GRAPHICS_PPV_ARGS(device.GetAddressOf()));

            m_dirtyFlag = false;
            m_constantBuffer = GraphicsMemory::Get(device.Get()).AllocateConstant(m_constants);
        }
        commandList->SetComputeRootConstantBufferView(RootParameterIndex::ConstantBuffer, m_constantBuffer.GpuAddress());

        // Set UAV texture
        commandList->SetComputeRootDescriptorTable(RootParameterIndex::TextureUAV, m_texture);

        // Dispatch
        int width = m_constants.bounds.z - m_constants.bounds.x;
        int height = m_constants.bounds.w - m_constants.bounds.y;

        int threadGroupX = (width - 1) / s_groupSize.x + 1;
        int threadGroupY = (height - 1) / s_groupSize.y + 1;

        commandList->Dispatch(threadGroupX, threadGroupY, 1);
    }

    void EsramVisualizeEffect::SetConstants(const Constants& constants)
    {
        // Do some good ol' validation.
        assert(constants.bounds.x < constants.bounds.z && constants.bounds.y < constants.bounds.w);
        assert(constants.selectedIndex >= 0 && constants.selectedIndex < _countof(Constants::textures));

        for (int i = 0; i < _countof(Constants::textures); ++i)
        {
            auto& tex = constants.textures[i];

            assert(tex.timeRange.x <= tex.timeRange.y);
            assert(tex.pageRangeCount >= 0 && tex.pageRangeCount < _countof(Constants::Texture::pageRanges));

            for (int j = 0; j < tex.pageRangeCount; ++j)
            {
                assert(tex.pageRanges[j].x < tex.pageRanges[j].y);
            }
        }

        m_dirtyFlag = true;
        m_constants = constants;
    }

    void EsramVisualizeEffect::SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE handle)
    {
        m_texture = handle;
    }
}
