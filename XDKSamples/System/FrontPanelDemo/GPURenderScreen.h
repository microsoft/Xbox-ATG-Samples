//--------------------------------------------------------------------------------------
// GPURenderScreen.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "PanelScreen.h"
#include "NavigationHint.h"

#include "FrontPanel/FrontPanelRenderTarget.h"

class GPURenderScreen : public PanelScreen
{
public:
    GPURenderScreen(FrontPanelManager *owner);
    void RenderFrontPanel() override;
    void CreateDeviceDependentResources(DX::DeviceResources *deviceResources, IXboxFrontPanelControl *frontPanelControl) override;
    void CreateWindowSizeDependentResources(DX::DeviceResources *deviceResources) override;
    void GPURender(DX::DeviceResources *deviceResources) override;

private:
    // Front Panel Render Target
    // Helper class to convert a GPU resource to grayscale and then render to the Front Panel
    std::unique_ptr<ATG::FrontPanelRenderTarget> m_frontPanelRenderTarget;

    // Shader resource view for the whole screen
    // This is the input to the FrontPanelRender target
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_mainRenderTargetSRV;
	
    BasicNavigationHint                      m_nav;
};

