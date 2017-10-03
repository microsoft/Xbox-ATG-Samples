//--------------------------------------------------------------------------------------
// GPURenderScreen.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"

#include "GPURenderScreen.h"

#include "FrontPanel/FrontPanelDisplay.h"

using namespace ATG;

GPURenderScreen::GPURenderScreen(FrontPanelManager * owner)
    : PanelScreen(owner)
{
    // Set up the front panel render target
    m_frontPanelRenderTarget = std::make_unique<FrontPanelRenderTarget>();
}

void GPURenderScreen::RenderFrontPanel()
{
    auto& frontPanelDisplay = FrontPanelDisplay::Get();
    BufferDesc fpDesc = frontPanelDisplay.GetBufferDescriptor();
    
    // Draw the navigation hints
    {
        int x = fpDesc.width - m_nav.GetWidth();
        int y = 0;

        if (m_leftNeighbor)
        {
            m_nav.DrawLeftIndicator(fpDesc, x, y);
        }

        if (m_rightNeighbor)
        {
            m_nav.DrawRightIndicator(fpDesc, x, y);
        }

        if (m_upNeighbor)
        {
            m_nav.DrawUpIndicator(fpDesc, x, y);
        }

        if (m_downNeighbor)
        {
            m_nav.DrawDownIndicator(fpDesc, x, y);
        }
    }

    frontPanelDisplay.Present();
}

void GPURenderScreen::CreateDeviceDependentResources(DX::DeviceResources * deviceResources, IXboxFrontPanelControl * frontPanelControl)
{
    auto device = deviceResources->GetD3DDevice();

    // Create the front panel render target resources
    m_frontPanelRenderTarget->CreateDeviceDependentResources(frontPanelControl, device);
}

void GPURenderScreen::CreateWindowSizeDependentResources(DX::DeviceResources * deviceResources)
{
    // Create a shader resource view for the main render target
    auto device = deviceResources->GetD3DDevice();
    DX::ThrowIfFailed(device->CreateShaderResourceView(deviceResources->GetRenderTarget(), nullptr, m_mainRenderTargetSRV.GetAddressOf()));
}


void GPURenderScreen::GPURender(DX::DeviceResources * deviceResources)
{
    auto& frontPanelDisplay = FrontPanelDisplay::Get();
    BufferDesc fpDesc = frontPanelDisplay.GetBufferDescriptor();

    auto context = deviceResources->GetD3DDeviceContext();

    // Blit to the Front Panel render target and then present to the Front Panel
    m_frontPanelRenderTarget->GPUBlit(context, m_mainRenderTargetSRV.Get());

    m_frontPanelRenderTarget->CopyToBuffer(context, fpDesc);

    RenderFrontPanel();
}
