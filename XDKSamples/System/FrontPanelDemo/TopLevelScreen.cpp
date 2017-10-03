//--------------------------------------------------------------------------------------
// TopLevelScreen.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"

#include "TopLevelScreen.h"
#include "FontManager.h"
#include "FrontPanelManager.h"

using namespace ATG;

TopLevelScreen::TopLevelScreen(FrontPanelManager *owner, const wchar_t *titleText, const wchar_t *labelText, const wchar_t *descriptionText)
    : PanelScreen(owner)
    , m_titleText(titleText)
    , m_labelText(labelText)
    , m_descriptionText(descriptionText)
    , m_titleFont(FontManager::Instance().LoadFont(L"assets\\Segoe_UI_bold24.rasterfont"))
    , m_descriptionFont(FontManager::Instance().LoadFont(L"assets\\Segoe_UI16.rasterfont"))
{
    RECT r = m_titleFont.MeasureString(labelText);

    unsigned clientWidth = std::max(MIN_LABEL_WIDTH + RIGHT_LABEL_PADDING, static_cast<unsigned>(r.right - r.left));

    m_nav.SetClientDimensions(clientWidth, r.bottom - r.top);
}

void TopLevelScreen::RenderFrontPanel()
{
    // Render to the front panel
    auto& frontPanelDisplay = FrontPanelDisplay::Get();

    frontPanelDisplay.Clear();

    BufferDesc fpDesc = frontPanelDisplay.GetBufferDescriptor();

    int x = 0;
    int y = 0;

    // Draw the title text
    {
        m_titleFont.DrawString(fpDesc, x, y, m_titleText.c_str());
        y += m_titleFont.GetLineSpacing();
    }

    // Draw the label text
    {
        int x0 = fpDesc.width - m_nav.GetWidth();
        int y0 = 0;
        
        RECT lRect = m_titleFont.MeasureString(m_labelText.c_str());
        float labelWidth = float(lRect.right - lRect.left);
        float navWidth = float(m_nav.GetWidth());
        float labelHeight = float(lRect.bottom - lRect.top);
        float navHeight = float(m_nav.GetHeight());

        x0 += int(floor(0.5f + (navWidth - labelWidth) / 2.0f)) - RIGHT_LABEL_PADDING;
        y0 += int(floor(0.5f + (navHeight - labelHeight) / 2.0f));
        m_titleFont.DrawString(fpDesc, x0, y0, m_labelText.c_str());
    }


    // Draw the description text
    m_descriptionFont.DrawString(fpDesc, x, y, m_descriptionText.c_str());

    // Draw the navigation hints
    {
        x = fpDesc.width - m_nav.GetWidth();
        y = 0;

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

TopLevelScreen::TopLevelNavigationHint::TopLevelNavigationHint()
    : NavigationHint(
        L"assets\\Symbols16.rasterfont",
        0xE3B1, 0xE3B2, 0, 0, // 0xE3B0, 0xE3AF,
        0, 0 )
{
}
