//--------------------------------------------------------------------------------------
// FontManager.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <unordered_map>

#include "FrontPanel/RasterFont.h"


class FontManager
{
    FontManager();

public:

    static FontManager &Instance();

    void ClearCache();

    ATG::RasterFont &LoadFont(const wchar_t *fileName);

private:

    std::unordered_map<std::wstring, ATG::RasterFont> m_cachedFonts;
};