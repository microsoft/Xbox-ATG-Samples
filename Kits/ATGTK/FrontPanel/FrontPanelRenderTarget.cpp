//--------------------------------------------------------------------------------------
// FrontPanelRenderTarget.cpp
//
// Microsoft Xbox One XDK for DirectX 11
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"

#include "FrontPanelRenderTarget.h"
#include "DirectXHelpers.h"

#include <algorithm>

using Microsoft::WRL::ComPtr;

using namespace ATG;
using namespace DirectX;

FrontPanelRenderTarget::FrontPanelRenderTarget()
    : m_displayWidth(0),
    m_displayHeight(0),
    m_renderTargetFormat(DXGI_FORMAT_UNKNOWN)
{
}

FrontPanelRenderTarget::~FrontPanelRenderTarget()
{
}

void FrontPanelRenderTarget::CreateDeviceDependentResources(_In_ IXboxFrontPanelControl *frontPanelControl, _In_ ID3D11Device *device)
{
    m_frontPanelControl = frontPanelControl;

    // Determine the render target size in pixels
    DX::ThrowIfFailed(m_frontPanelControl->GetScreenWidth(&m_displayWidth));
    DX::ThrowIfFailed(m_frontPanelControl->GetScreenHeight(&m_displayHeight));

    // Get the render target format for the Front Panel
    DX::ThrowIfFailed(m_frontPanelControl->GetScreenPixelFormat(&m_renderTargetFormat));

    // Create memory blt buffer
    m_buffer.reset(new uint8_t[m_displayWidth * m_displayHeight]);

    // Create staging blt buffer
    {
        CD3D11_TEXTURE2D_DESC desc(
            m_renderTargetFormat,
            m_displayWidth,
            m_displayHeight,
            1, // The render target view has only one texture.
            1, // Use a single mipmap level.
            0,
            D3D11_USAGE_STAGING,
            D3D11_CPU_ACCESS_READ
        );

        DX::ThrowIfFailed(
            device->CreateTexture2D( &desc, 0, m_staging.ReleaseAndGetAddressOf())
        );

        SetDebugObjectName(m_staging.Get(), "FrontPanel Staging");
    }

    // Create the render target
    {
        CD3D11_TEXTURE2D_DESC desc(
            m_renderTargetFormat,
            m_displayWidth,
            m_displayHeight,
            1, // The render target view has only one texture.
            1, // Use a single mipmap level.
            D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_DEFAULT,
            0
        );

        DX::ThrowIfFailed(
            device->CreateTexture2D(&desc, nullptr, m_renderTarget.ReleaseAndGetAddressOf())
        );

        SetDebugObjectName(m_renderTarget.Get(), "FrontPanel RT");
    }

    // Create the render target view
    {
        CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2D, m_renderTargetFormat);

        DX::ThrowIfFailed(device->CreateRenderTargetView(
            m_renderTarget.Get(),
            &renderTargetViewDesc,
            m_renderTargetView.ReleaseAndGetAddressOf()
        ));

        SetDebugObjectName(m_renderTargetView.Get(), "FrontPanel RTV");
    }

    // Create the monochrome post-processor
    m_panelBlt = std::make_unique <BasicPostProcess>(device);
}

void FrontPanelRenderTarget::Clear(_In_ ID3D11DeviceContext * context, const float ColorRGBA[4])
{
    context->ClearRenderTargetView(m_renderTargetView.Get(), ColorRGBA);
}

void FrontPanelRenderTarget::SetAsRenderTarget(_In_ ID3D11DeviceContext * context)
{
    auto renderTargetView = m_renderTargetView.Get();
    context->OMSetRenderTargets(1, &renderTargetView, nullptr);
}

void FrontPanelRenderTarget::GPUBlit(_In_ ID3D11DeviceContext * context, _In_ ID3D11ShaderResourceView * srcSRV)
{
    assert(m_panelBlt);

    SetAsRenderTarget(context);

    // Set the viewports for the off-screen render target
    D3D11_VIEWPORT viewport = { 0.0f, 0.0f, float(m_displayWidth), float(m_displayHeight), 0.0f, 1.0f };
    context->RSSetViewports(1, &viewport);

    // Convert pixels from srcSRV to grayscale
    m_panelBlt->SetSourceTexture(srcSRV);
    m_panelBlt->Process(context);
    context->Flush();

    ID3D11ShaderResourceView* srv = nullptr;
    context->PSSetShaderResources(0, 1, &srv);
}

void FrontPanelRenderTarget::CopyToBuffer(_In_ ID3D11DeviceContext * context, BufferDesc &desc)
{
    assert(m_staging && m_renderTarget);
    context->CopyResource(m_staging.Get(), m_renderTarget.Get());

    MapGuard mapped(context, m_staging.Get(), 0, D3D11_MAP_READ, 0);

    auto sptr = reinterpret_cast<uint8_t*>(mapped.pData);
    uint8_t *dptr = desc.data;

    unsigned width = std::min(desc.width, mapped.RowPitch);
    unsigned height = std::min(desc.height, m_displayHeight);

    for (size_t h = 0; h < height; ++h)
    {
        memcpy_s(dptr, width, sptr, mapped.RowPitch);
        sptr += mapped.RowPitch;
        dptr += desc.width;
    }
}

void FrontPanelRenderTarget::PresentToFrontPanel(_In_ ID3D11DeviceContext *context)
{
    assert(m_frontPanelControl);

    unsigned bufSize = m_displayWidth * m_displayHeight;

    BufferDesc desc;
    desc.data = m_buffer.get();
    desc.size = bufSize;
    desc.width = m_displayWidth;
    desc.height = m_displayHeight;

    CopyToBuffer(context, desc);
    
    m_frontPanelControl->PresentBuffer(bufSize, m_buffer.get());
}
