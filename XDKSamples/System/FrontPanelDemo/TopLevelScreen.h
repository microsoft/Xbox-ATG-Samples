//--------------------------------------------------------------------------------------
// TopLevelScreen.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "PanelScreen.h"
#include "NavigationHint.h"

class TopLevelScreen : public PanelScreen
{
public:
    TopLevelScreen(
        FrontPanelManager *owner,
        const wchar_t *titleText,
        const wchar_t *labelText,
        const wchar_t *descriptionText);

    void RenderFrontPanel() override;

private:
    class TopLevelNavigationHint : public NavigationHint
    {
    public:
        TopLevelNavigationHint();
    };

    const unsigned MIN_LABEL_WIDTH     = 12;
    const unsigned RIGHT_LABEL_PADDING = 1;

    std::wstring           m_titleText;
    std::wstring           m_labelText;
    std::wstring           m_descriptionText;
    ATG::RasterFont       &m_titleFont;
    ATG::RasterFont       &m_descriptionFont;
    TopLevelNavigationHint m_nav;
};