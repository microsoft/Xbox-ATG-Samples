//--------------------------------------------------------------------------------------
// QuickActionScene.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"

#include "QuickActionScreen.h"
#include "FontManager.h"
#include "FrontPanelManager.h"

#include "FrontPanel/FrontPanelInput.h"
#include "FrontPanel/FrontPanelDisplay.h"

using namespace ATG;

QuickActionScreen::QuickActionScreen(FrontPanelManager *owner, XBOX_FRONT_PANEL_BUTTONS buttonId)
    : PanelScreen(owner)
    , m_myButton(buttonId)
    , m_curLightState(true)
    , m_blinkSeconds(0.0f)
{
}

void QuickActionScreen::Update(DX::StepTimer const & timer)
{
    m_blinkSeconds += timer.GetElapsedSeconds();
    
    if (m_blinkSeconds > 0.35f)
    {
        m_blinkSeconds = 0;
        SetLightState(!m_curLightState);
    }
}

void QuickActionScreen::RenderFrontPanel()
{
    // Get the fonts
    ATG::RasterFont &titleFont       = FontManager::Instance().LoadFont(L"assets\\Segoe_UI_bold16.rasterfont");
    ATG::RasterFont &descriptionFont = FontManager::Instance().LoadFont(L"assets\\Segoe_UI16.rasterfont");
    ATG::RasterFont &symbolFont      = FontManager::Instance().LoadFont(L"assets\\Symbols32.rasterfont");
    ATG::RasterFont &buttonFont      = FontManager::Instance().LoadFont(L"assets\\Segoe_UI_bold24.rasterfont");

    // Render to the front panel
    auto& frontPanelDisplay = FrontPanelDisplay::Get();

    frontPanelDisplay.Clear();

    BufferDesc fpDesc = frontPanelDisplay.GetBufferDescriptor();

    unsigned buttonIdx = unsigned(FrontPanelManager::GetIndexForButtonId(m_myButton));
    bool buttonHasAssignment = m_owner->IsActionAssigned(m_myButton);
    int x = 40;
    int y = 0;

    // Draw the title text
    {
        titleFont.DrawStringFmt(fpDesc, x, y, L"Button %i Action", buttonIdx + 1);
        y += titleFont.GetLineSpacing();
    }

    // Draw the button graphic
    {
        wchar_t buttonGlyph = '1' + wchar_t(buttonIdx);

        int bx = 2;
        int by = 16;

        symbolFont.DrawGlyph(fpDesc, bx, by, 0xE48C, buttonHasAssignment ? 0xF0 : 0x40);

        RECT rCrcl = symbolFont.MeasureGlyph(0xE48C);
        float wCrcl = float(rCrcl.right - rCrcl.left);
        float hCrcl = float(rCrcl.bottom - rCrcl.top);

        RECT rBtn = buttonFont.MeasureGlyph(buttonGlyph);
        float wBtn = float(rBtn.right - rBtn.left);
        float hBtn = float(rBtn.bottom - rBtn.top);
        
        bx += int(floor(0.5f + (wCrcl - wBtn) / 2.0f));
        by += int(floor(0.5f + (hCrcl - hBtn) / 2.0f));

        buttonFont.DrawGlyph(fpDesc, bx, by, buttonGlyph, 0x00);
    }

    // Draw the description text
    {
        if (buttonHasAssignment)
        {
            auto& assignment = m_owner->GetActionAssignment(m_myButton);
            descriptionFont.DrawStringFmt(fpDesc, x, y, L"%s\n%s", assignment.name.c_str(), assignment.description.c_str());
        }
        else
        {
            descriptionFont.DrawString(fpDesc, x, y, L"There is no action assigned to this\nbutton.");
        }
    }

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

    // Turn on the light for the button corresponding to this screen
    SetLightState(true);
}

void QuickActionScreen::SetLightState(bool on)
{
    m_curLightState = on;
    m_blinkSeconds = 0.0f;
    
    XBOX_FRONT_PANEL_LIGHTS lights = m_owner->GetAssignedLights();
    
    if (on)
    {
        lights = static_cast<XBOX_FRONT_PANEL_LIGHTS>(lights | m_myButton);
    }
    else
    {
        lights = static_cast<XBOX_FRONT_PANEL_LIGHTS>(lights & ~m_myButton);
    }

    FrontPanelInput::Get().SetLightStates(lights);
}
