//--------------------------------------------------------------------------------------
// PanelScreen.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <vector>

#include <XboxFrontPanel.h>

#include "FrontPanel/RasterFont.h"
#include "StepTimer.h"
#include "DeviceResources.h"

class FrontPanelManager;

class PanelScreen
{
public:
    PanelScreen(FrontPanelManager *owner)
        : m_owner(owner)
        , m_leftNeighbor(nullptr)
        , m_rightNeighbor(nullptr)
        , m_upNeighbor(nullptr)
        , m_downNeighbor(nullptr)
    {
    }

    virtual ~PanelScreen();
    virtual void Update(DX::StepTimer const& timer);
    virtual void OnNeighborsChanged();
    virtual bool OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS whichButton);
    virtual void Resume(const PanelScreen *prevScreen);

    virtual void CreateDeviceDependentResources(DX::DeviceResources *deviceResources, IXboxFrontPanelControl *frontPanelControl);
    virtual void CreateWindowSizeDependentResources(DX::DeviceResources *deviceResources);
    virtual void GPURender(DX::DeviceResources *deviceResources);

    virtual void RenderFrontPanel() = 0;

    void SetLeftNeighbor(PanelScreen *leftNeighbor);
    void SetRightNeighbor(PanelScreen *rightNeighbor);
    void SetUpNeighbor(PanelScreen *upNeighbor);
    void SetDownNeighbor(PanelScreen *downNeighbor);

protected:
    FrontPanelManager *m_owner;
    PanelScreen       *m_leftNeighbor;
    PanelScreen       *m_rightNeighbor;
    PanelScreen       *m_upNeighbor;
    PanelScreen       *m_downNeighbor;
};

