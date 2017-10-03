//--------------------------------------------------------------------------------------
// QuickActionScene.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "PanelScreen.h"
#include "NavigationHint.h"
#include "StepTimer.h"

class QuickActionScreen : public PanelScreen
{
public:
    QuickActionScreen(FrontPanelManager *owner, XBOX_FRONT_PANEL_BUTTONS buttonId);

    void Update(DX::StepTimer const& timer) override;
    void RenderFrontPanel() override;

private:
    void SetLightState(bool on);

    XBOX_FRONT_PANEL_BUTTONS  m_myButton;
    BasicNavigationHint       m_nav;
    bool                      m_curLightState;
    double                    m_blinkSeconds;

};
