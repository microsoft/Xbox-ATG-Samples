//--------------------------------------------------------------------------------------
// FontViewerScreen.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "PanelScreen.h"
#include "NavigationHint.h"
#include <map>

class FontViewerScreen : public PanelScreen
{
public:
    FontViewerScreen(FrontPanelManager *owner, const wchar_t *faceName, unsigned titleHeight, const wchar_t *fileName);

    void AddFontFile(unsigned height, const wchar_t *fileName, const wchar_t *sampleText = s_defaultSampleText);

    void RenderFrontPanel() override;
    bool OnButtonPressed(XBOX_FRONT_PANEL_BUTTONS whichButton) override;


private:
    struct FontData
    {
        const wchar_t *filename;
        const wchar_t *sampleText;
    };

    using HeightToFontFile = std::map<unsigned, FontData>;

    unsigned                         m_titleHeight;
    const wchar_t                   *m_faceName;
    HeightToFontFile                 m_heightToFontFile;
    HeightToFontFile::const_iterator m_currentFont;
    BasicNavigationHint              m_nav;

    static const wchar_t *s_defaultSampleText;
};