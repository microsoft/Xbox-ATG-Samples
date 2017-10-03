//--------------------------------------------------------------------------------------
// NavigationHint.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "FrontPanel/RasterFont.h"

class NavigationHint
{
public:
    NavigationHint(
        const wchar_t *fontName,
        wchar_t leftIndicator,
        wchar_t rightIndicator,
        wchar_t upIndicator,
        wchar_t downIndicator,
        unsigned clientWidth,
        unsigned clientHeight);

    virtual ~NavigationHint();

    unsigned GetWidth() const { return m_width; }
    unsigned GetHeight() const { return m_height; }

    void DrawLeftIndicator(const ATG::BufferDesc &desc, int x, int y);
    void DrawRightIndicator(const ATG::BufferDesc &desc, int x, int y);
    void DrawUpIndicator(const ATG::BufferDesc &desc, int x, int y);
    void DrawDownIndicator(const ATG::BufferDesc &desc, int x, int y);

    void SetClientDimensions(unsigned clientWidth, unsigned clientHeight);
    RECT GetClientDimensions() const { return m_clientRect; }
    
protected:
    ATG::RasterFont &m_font;
    const wchar_t    m_leftIndicator;
    const wchar_t    m_rightIndicator;
    const wchar_t    m_upIndicator;
    const wchar_t    m_downIndicator;
                     
    unsigned         m_width;
    unsigned         m_height;
                     
    RECT             m_clientRect;
                     
    unsigned         m_leftOffsetX;
    unsigned         m_leftOffsetY;
    unsigned         m_rightOffsetX;
    unsigned         m_rightOffsetY;
    unsigned         m_upOffsetX;
    unsigned         m_upOffsetY;
    unsigned         m_downOffsetX;
    unsigned         m_downOffsetY;
};

class BasicNavigationHint : public NavigationHint
{
public:
    BasicNavigationHint();
};

