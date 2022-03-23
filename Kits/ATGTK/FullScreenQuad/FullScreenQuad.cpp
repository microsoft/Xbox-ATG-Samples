//--------------------------------------------------------------------------------------
// FullScreenQuad.cpp
//
// Class to draw a full-screen quad
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "FullScreenQuad.h"

#include "DirectXHelpers.h"

using Microsoft::WRL::ComPtr;
using namespace DX;

#if defined(__d3d12_h__) || defined(__d3d12_x_h__) || defined(__XBOX_D3D12_X__)
//======================================================================================
// Direct3D 12
//======================================================================================

// Root signature layout
enum RootParameterIndex
{
    ConstantBuffer,
    TextureSRV,
    TextureSRV_2,
    Count
};

// Initialize
void FullScreenQuad::Initialize(_In_ ID3D12Device* d3dDevice)
{
    const D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

    const CD3DX12_DESCRIPTOR_RANGE textureSRVs(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    const CD3DX12_DESCRIPTOR_RANGE textureSRVs_2(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0, // register
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0.0f,
        16,
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
        0.0f,
        D3D12_FLOAT32_MAX,
        D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_PARAMETER rootParameters[static_cast<uint32_t>(RootParameterIndex::Count)] = {};
    rootParameters[RootParameterIndex::ConstantBuffer].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[RootParameterIndex::TextureSRV].InitAsDescriptorTable(1, &textureSRVs, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[RootParameterIndex::TextureSRV_2].InitAsDescriptorTable(1, &textureSRVs_2, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(static_cast<UINT>(std::size(rootParameters)), rootParameters, 1, &sampler, rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    DX::ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    DX::ThrowIfFailed(d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_GRAPHICS_PPV_ARGS(m_d3dRootSignature.ReleaseAndGetAddressOf())));
}

// Draw the full screen quad
_Use_decl_annotations_
void FullScreenQuad::Draw(
    ID3D12GraphicsCommandList* d3dCommandList,
    ID3D12PipelineState* d3dPSO,
    D3D12_GPU_DESCRIPTOR_HANDLE texture,
    D3D12_GPU_VIRTUAL_ADDRESS constantBuffer)
{
    d3dCommandList->SetGraphicsRootSignature(m_d3dRootSignature.Get());
    d3dCommandList->SetGraphicsRootConstantBufferView(RootParameterIndex::ConstantBuffer, constantBuffer);
    d3dCommandList->SetGraphicsRootDescriptorTable(RootParameterIndex::TextureSRV, texture);
    d3dCommandList->SetGraphicsRootDescriptorTable(RootParameterIndex::TextureSRV_2, texture);
    d3dCommandList->SetPipelineState(d3dPSO);
    d3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3dCommandList->DrawInstanced(3, 1, 0, 0);
}

_Use_decl_annotations_
void FullScreenQuad::Draw(
    ID3D12GraphicsCommandList* d3dCommandList,
    ID3D12PipelineState* d3dPSO,
    D3D12_GPU_DESCRIPTOR_HANDLE texture,
    D3D12_GPU_DESCRIPTOR_HANDLE texture2,
    D3D12_GPU_VIRTUAL_ADDRESS constantBuffer)
{
    d3dCommandList->SetGraphicsRootSignature(m_d3dRootSignature.Get());
    d3dCommandList->SetGraphicsRootConstantBufferView(RootParameterIndex::ConstantBuffer, constantBuffer);
    d3dCommandList->SetGraphicsRootDescriptorTable(RootParameterIndex::TextureSRV, texture);
    d3dCommandList->SetGraphicsRootDescriptorTable(RootParameterIndex::TextureSRV_2, texture2);
    d3dCommandList->SetPipelineState(d3dPSO);
    d3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3dCommandList->DrawInstanced(3, 1, 0, 0);
}

void FullScreenQuad::ReleaseDevice()
{
    m_d3dRootSignature.Reset();
}

#else
//======================================================================================
// Direct3D 11
//======================================================================================

#include "CommonStates.h"
#include "ReadData.h"

// Initialize
void FullScreenQuad::Initialize(_In_ ID3D11Device* d3dDevice)
{
    if (d3dDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
    {
        throw std::exception("FullScreenQuad requires Direct3D hardware feature level 10.0 or better");
    }

    auto vertexShaderBlob = DX::ReadData(L"FullScreenQuadVS.cso");
    DX::ThrowIfFailed(
        d3dDevice->CreateVertexShader(vertexShaderBlob.data(), vertexShaderBlob.size(), nullptr,
            m_vertexShader.ReleaseAndGetAddressOf()));

    auto pixelShaderBlob = DX::ReadData(L"FullScreenQuadPS.cso");
    DX::ThrowIfFailed(
        d3dDevice->CreatePixelShader(pixelShaderBlob.data(), pixelShaderBlob.size(), nullptr,
            m_pixelShader.ReleaseAndGetAddressOf()));
}

_Use_decl_annotations_
void FullScreenQuad::Draw(
    ID3D11DeviceContext* d3dContext,
    DirectX::CommonStates& states,
    ID3D11ShaderResourceView* texture,
    std::function<void __cdecl()> setCustomState)
{
    // Set the texture.
    ID3D11ShaderResourceView* textures[1] = { texture };
    d3dContext->PSSetShaderResources(0, 1, textures);

    auto sampler = states.LinearClamp();
    d3dContext->PSSetSamplers(0, 1, &sampler);

    // Set state objects.
    d3dContext->OMSetBlendState(states.Opaque(), nullptr, 0xffffffff);
    d3dContext->OMSetDepthStencilState(states.DepthNone(), 0);
    d3dContext->RSSetState(states.CullNone());

    // Set shaders.
    d3dContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    d3dContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    if (setCustomState)
    {
        setCustomState();
    }

    // Draw quad.
    d3dContext->IASetInputLayout(nullptr);
    d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    d3dContext->Draw(3, 0);
}

void FullScreenQuad::ReleaseDevice()
{
    m_vertexShader.Reset();
    m_pixelShader.Reset();
}

#endif
