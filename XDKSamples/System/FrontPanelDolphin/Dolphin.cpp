//--------------------------------------------------------------------------------------
// Dolphin.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "Dolphin.h"
#include "ReadData.h"

using namespace DirectX;

Dolphin::Dolphin()
    : m_world(XMMatrixIdentity())
    , m_translation(0, 0, 0)
    , m_animationTime(0)
    , m_blendWeight(0)
    , m_primitiveType(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
    , m_vertexStride(0)
    , m_indexCount(0)
    , m_vertexFormat(DXGI_FORMAT_UNKNOWN)
{
    m_animationTime = float(rand() % 100);
}

void Dolphin::Load(ID3D11Device *device, ID3D11DeviceContext *context, DirectX::EffectFactory *fxFactory)
{
    std::unique_ptr<DirectX::Model> dolphinModel1 = Model::CreateFromSDKMESH(device, L"assets\\mesh\\Dolphin1.sdkmesh", *fxFactory);
    std::unique_ptr<DirectX::Model> dolphinModel2 = Model::CreateFromSDKMESH(device, L"assets\\mesh\\Dolphin2.sdkmesh", *fxFactory);
    std::unique_ptr<DirectX::Model> dolphinModel3 = Model::CreateFromSDKMESH(device, L"assets\\mesh\\Dolphin3.sdkmesh", *fxFactory);

    fxFactory->CreateTexture(L"dolphin.bmp", context, m_textureView.ReleaseAndGetAddressOf());

    {
        auto& meshes = dolphinModel1->meshes;
        auto& meshParts = meshes[0]->meshParts;
        auto& part = *meshParts[0];
        m_vb1 = part.vertexBuffer;
        m_ib = part.indexBuffer;
        m_primitiveType = part.primitiveType;
        m_indexCount = part.indexCount;
        m_vertexStride = part.vertexStride;
        m_vertexFormat = part.indexFormat;
    }

    {
        auto& meshes = dolphinModel2->meshes;
        auto& meshParts = meshes[0]->meshParts;
        auto& part = *meshParts[0];
        m_vb2 = part.vertexBuffer;
    }

    {
        auto& meshes = dolphinModel3->meshes;
        auto& meshParts = meshes[0]->meshParts;
        auto& part = *meshParts[0];
        m_vb3 = part.vertexBuffer;
    }
    
    {
        auto blob = DX::ReadData(L"DolphinVS.cso");
        DX::ThrowIfFailed(device->CreateVertexShader(blob.data(), blob.size(), nullptr, m_vertexShader.ReleaseAndGetAddressOf()));

        const D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "POSITION",  1, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",    1, DXGI_FORMAT_R32G32B32_FLOAT, 1, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",  1, DXGI_FORMAT_R32G32_FLOAT,    1, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "POSITION",  2, DXGI_FORMAT_R32G32B32_FLOAT, 2, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",    2, DXGI_FORMAT_R32G32B32_FLOAT, 2, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",  2, DXGI_FORMAT_R32G32_FLOAT,    2, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        DX::ThrowIfFailed(device->CreateInputLayout(layout, _countof(layout), blob.data(), blob.size(), m_vertexLayout.ReleaseAndGetAddressOf()));
    }
}

void Dolphin::OnDeviceLost()
{
    m_textureView.Reset();
    m_vb1.Reset();
    m_vb2.Reset();
    m_vb3.Reset();
    m_ib.Reset();
    m_inputLayout.Reset();
    m_vertexShader.Reset();
    m_vertexLayout.Reset();
}

void Dolphin::Update(float, float elapsedTime)
{
    // update the animation time for the dolphin
    m_animationTime += elapsedTime;

    // store the blend weight (this determines how fast the tale wags)
    m_blendWeight = sinf(6 * m_animationTime);

    // compute our rotation matrices and combine them with scale into our world matrix
    m_world = XMMatrixScaling(0.01f, 0.01f, 0.01f);
    
    // This rotation adds a little wiggle
    m_world = XMMatrixMultiply(XMMatrixRotationZ(cos(4 * m_animationTime) / 6.0f), m_world);

    // Translate and then rotate to make the dolphin swim in a circle
    m_world = XMMatrixMultiply(m_world, XMMatrixTranslation(0.0f, 0.0f, 8.0f));
    m_world = XMMatrixMultiply(m_world, XMMatrixRotationY(-m_animationTime / 2.0f));

    // Perturb the dolphin's position in vertical direction so that it looks more "floaty"
    m_world = XMMatrixMultiply(m_world, XMMatrixTranslation(0.0f, cos(4 * m_animationTime) / 3.0f, 0.0f));
}

void Dolphin::Render(ID3D11DeviceContext * d3dDeviceContext, ID3D11PixelShader * pixelShader, ID3D11ShaderResourceView * causticResourceView)
{
    unsigned int DolphinStrides[3] = { m_vertexStride, m_vertexStride, m_vertexStride };
    unsigned int DolphinOffsets[3] = { 0, 0, 0 };
    ID3D11Buffer *dolphinVBs[3] = { m_vb1.Get(), m_vb2.Get(), m_vb3.Get() };
    ID3D11ShaderResourceView *textureResourceView = m_textureView.Get();

    d3dDeviceContext->IASetInputLayout(m_vertexLayout.Get());
    d3dDeviceContext->IASetVertexBuffers(0, 3, dolphinVBs, DolphinStrides, DolphinOffsets);
    d3dDeviceContext->IASetIndexBuffer(m_ib.Get(), m_vertexFormat, 0);
    d3dDeviceContext->IASetPrimitiveTopology(m_primitiveType);
    d3dDeviceContext->VSSetShader(m_vertexShader.Get(), NULL, 0);
    d3dDeviceContext->GSSetShader(NULL, NULL, 0);
    d3dDeviceContext->PSSetShader(pixelShader, NULL, 0);
    d3dDeviceContext->PSSetShaderResources(0, 1, &textureResourceView);
    d3dDeviceContext->PSSetShaderResources(1, 1, &causticResourceView);
    d3dDeviceContext->DrawIndexed(m_indexCount, 0, 0);
}

void Dolphin::Translate(DirectX::XMVECTOR t)
{
    m_translation = XMVectorAdd(m_translation, t);
}

DirectX::XMMATRIX Dolphin::GetWorld()
{
    // combine our existing world matrix with the translation
    XMMATRIX result = XMMatrixMultiply(m_world, XMMatrixTranslationFromVector(m_translation));
    return result;
}

float Dolphin::GetBlendWeight()
{
    return m_blendWeight;
}
