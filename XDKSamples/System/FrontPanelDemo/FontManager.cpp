//--------------------------------------------------------------------------------------
// FontManager.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "FontManager.h"

FontManager::FontManager()
{
}

FontManager &FontManager::Instance()
{
    static FontManager result;
    return result;
}

void FontManager::ClearCache()
{
    m_cachedFonts.clear();
}

ATG::RasterFont& FontManager::LoadFont(const wchar_t * fileName)
{
    auto itr = m_cachedFonts.find(fileName);

    if (itr != m_cachedFonts.end())
    {
        return itr->second;
    }

    m_cachedFonts[fileName] = ATG::RasterFont(fileName);
    return m_cachedFonts[fileName];
}


