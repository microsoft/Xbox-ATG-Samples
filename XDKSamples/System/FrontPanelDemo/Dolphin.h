//--------------------------------------------------------------------------------------
// Dolphin.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

class Dolphin
{
public:
    Dolphin();

    void Load(ID3D11Device *device, ID3D11DeviceContext *context, DirectX::EffectFactory *fxFactory);

    void OnDeviceLost();

    void Update(float totalTime, float elapsedTime);

    void Render(
        ID3D11DeviceContext *d3dDeviceContext,
        ID3D11PixelShader *pixelShader,
        ID3D11ShaderResourceView *causticResourceView);

    void Translate(DirectX::XMVECTOR t);
    DirectX::XMMATRIX GetWorld();
    float GetBlendWeight();

private:
    DirectX::SimpleMath::Vector3    m_translation;
    DirectX::SimpleMath::Matrix     m_world;
    float                           m_animationTime;
    float                           m_blendWeight;

    D3D11_PRIMITIVE_TOPOLOGY m_primitiveType;
    unsigned int             m_vertexStride;
    unsigned int             m_indexCount;
    DXGI_FORMAT              m_vertexFormat;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureView;
    Microsoft::WRL::ComPtr<ID3D11Buffer>             m_vb1;
    Microsoft::WRL::ComPtr<ID3D11Buffer>             m_vb2;
    Microsoft::WRL::ComPtr<ID3D11Buffer>             m_vb3;
    Microsoft::WRL::ComPtr<ID3D11Buffer>             m_ib;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>        m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>       m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>        m_vertexLayout;
};
