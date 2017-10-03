//--------------------------------------------------------------------------------------
// NavigationHint.hpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"

#include "NavigationHint.h"
#include "FontManager.h"

NavigationHint::NavigationHint(
    const wchar_t *fontName,
    wchar_t leftIndicator,
    wchar_t rightIndicator,
    wchar_t upIndicator,
    wchar_t downIndicator,
    unsigned clientWidth,
    unsigned clientHeight)
    : m_font(FontManager::Instance().LoadFont(fontName))
    , m_leftIndicator(leftIndicator)
    , m_rightIndicator(rightIndicator)
    , m_upIndicator(upIndicator)
    , m_downIndicator(downIndicator)
    , m_width(0)
    , m_height(0)
    , m_leftOffsetX(0)
    , m_leftOffsetY(0)
    , m_rightOffsetX(0)
    , m_rightOffsetY(0)
    , m_upOffsetX(0)
    , m_upOffsetY(0)
    , m_downOffsetX(0)
    , m_downOffsetY(0)
{
    SetClientDimensions(clientWidth, clientHeight);
}

NavigationHint::~NavigationHint()
{
}

void NavigationHint::DrawLeftIndicator(const ATG::BufferDesc &desc, int x, int y)
{
    if (m_leftIndicator)
    {
        m_font.DrawGlyph(desc, x + m_leftOffsetX, y + m_leftOffsetY, m_leftIndicator);
    }
}

void NavigationHint::DrawRightIndicator(const ATG::BufferDesc & desc, int x, int y)
{
    if (m_rightIndicator)
    {
        m_font.DrawGlyph(desc, x + m_rightOffsetX, y + m_rightOffsetY, m_rightIndicator);
    }
}

void NavigationHint::DrawUpIndicator(const ATG::BufferDesc & desc, int x, int y)
{
    if (m_upIndicator)
    {
        m_font.DrawGlyph(desc, x + m_upOffsetX, y + m_upOffsetY, m_upIndicator);
    }
}

void NavigationHint::DrawDownIndicator(const ATG::BufferDesc & desc, int x, int y)
{
    if (m_downIndicator)
    {
        m_font.DrawGlyph(desc, x + m_downOffsetX, y + m_downOffsetY, m_downIndicator);
    }
}

void NavigationHint::SetClientDimensions(unsigned clientWidth, unsigned clientHeight)
{
    unsigned upWidth = 0;
    unsigned upHeight = 0;
    if (m_upIndicator)
    {
        auto upMeasurement = m_font.MeasureGlyph(m_upIndicator);
        upWidth = upMeasurement.right - upMeasurement.left;
        upHeight = upMeasurement.bottom - upMeasurement.top;
    }

    unsigned downWidth = 0;
    unsigned downHeight = 0;
    if(m_downIndicator)
    {
        auto downMeasurement = m_font.MeasureGlyph(m_downIndicator);
        downWidth = downMeasurement.right - downMeasurement.left;
        downHeight = downMeasurement.bottom - downMeasurement.top;
    }

    unsigned leftWidth = 0;
    unsigned leftHeight = 0;
    if (m_leftIndicator)
    {
        auto leftMeasurement = m_font.MeasureGlyph(m_leftIndicator);
        leftWidth = leftMeasurement.right - leftMeasurement.left;
        leftHeight = leftMeasurement.bottom - leftMeasurement.top;
    }
    
    unsigned rightWidth = 0;
    unsigned rightHeight = 0;
    if(m_rightIndicator)
    {
        auto rightMeasurement = m_font.MeasureGlyph(m_rightIndicator);
        rightWidth = rightMeasurement.right - rightMeasurement.left;
        rightHeight = rightMeasurement.bottom - rightMeasurement.top;
    }

    unsigned cw = 1;
    cw = std::max(cw, clientWidth);
    cw = std::max(cw, upWidth + 2);
    cw = std::max(cw, downWidth + 2);
    
    unsigned ch = 1;
    ch = std::max(ch, clientHeight);

    // update the client rectangle
    m_clientRect.left   = leftWidth;
    m_clientRect.right  = leftWidth + cw;
    m_clientRect.top    = upHeight;
    m_clientRect.bottom = upHeight + ch;
        
    // Compute the width and height
    m_width = leftWidth + cw + rightWidth;
    m_height = upHeight + ch + downHeight;
    m_height = std::max(m_height, leftHeight);
    m_height = std::max(m_height, rightHeight);

    // Compute the coordinates for the left indicator
    m_leftOffsetX = 0;
    m_leftOffsetY = static_cast<unsigned>(floor(0.5f + (m_height - leftHeight) / 2.0f));

    // Compute the coordinates for the right indicator
    m_rightOffsetX = leftWidth + cw;
    m_rightOffsetY = static_cast<unsigned>(floor(0.5f + (m_height - rightHeight) / 2.0f));

    // Compute the cooridates for the up indicator
    m_upOffsetX = static_cast<unsigned>(floor(0.5f + (m_width - upWidth) / 2.0f));
    m_upOffsetY = 0;

    // Compute the coordinates for the down indicator
    m_downOffsetX = static_cast<unsigned>(floor(0.5f + (m_width - downWidth) / 2.0f));
    m_downOffsetY = upHeight + ch;
}

BasicNavigationHint::BasicNavigationHint()
    : NavigationHint(
        L"assets\\LucidaConsole12.rasterfont",
        0x25C4, 0x25BA, 0x25B2, 0x25BC,
        0, 0
    )
{
}
