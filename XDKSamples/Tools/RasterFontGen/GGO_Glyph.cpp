//--------------------------------------------------------------------------------------
// GGO_Glyph.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "GGO_Glyph.h"

using namespace ATG;

ATG::GGO_Glyph::GGO_Glyph()
    : m_ggoFormat(0)
    , m_character(0)
    , m_glyphMetrics{}
    , m_bufferSize(0)
{
}

ATG::GGO_Glyph::GGO_Glyph(HDC hdc, wchar_t wc, uint32_t ggoFormat)
    : m_ggoFormat(ggoFormat)
    , m_character(wc)
    , m_glyphMetrics{}
    , m_bufferSize(0)
{

    switch (m_ggoFormat)
    {
    case GGO_BITMAP:
    case GGO_GRAY2_BITMAP:
    case GGO_GRAY4_BITMAP:
    case GGO_GRAY8_BITMAP:
        break;

    default:
        throw DX::exception_fmt<64>("Unsupported GGO format (%i). Must use a bitmap format see docs for GetGlyphOutline", m_ggoFormat);
    }

    MAT2 matrix = {};
    matrix.eM11.value = 1;
    matrix.eM12.value = 0;
    matrix.eM21.value = 0;
    matrix.eM22.value = 1;

    uint32_t result = GetGlyphOutline(
        hdc,
        wc,
        m_ggoFormat,
        &m_glyphMetrics,
        0,
        nullptr,
        &matrix);

    if (result != GDI_ERROR)
    {
        m_bufferSize = result;
        m_spritePixels = std::make_unique<uint8_t[]>(result);

        // GGO_BITMAP:
        // Returns the glyph bitmap.When the function returns, the buffer pointed to by lpBuffer
        // contains a 1 - bit - per - pixel bitmap whose rows start on double-word boundaries.

        result = GetGlyphOutline(
            hdc,
            wc,
            m_ggoFormat,
            &m_glyphMetrics,
            result,
            m_spritePixels.get(),
            &matrix);
    }

    if (result == GDI_ERROR)
    {
        throw DX::exception_fmt<128>("Not able to get the glyph bitmap for character code: %i", wc);
    }
}

ATG::GGO_Glyph::GGO_Glyph(GGO_Glyph && moveFrom)
    : m_ggoFormat(moveFrom.m_ggoFormat)
    , m_character(moveFrom.m_character)
    , m_bufferSize(moveFrom.m_bufferSize)
{
    memcpy(&m_glyphMetrics, &moveFrom.m_glyphMetrics, sizeof(GLYPHMETRICS));
    m_spritePixels = std::move(moveFrom.m_spritePixels);
}

GGO_Glyph & ATG::GGO_Glyph::operator=(GGO_Glyph && moveFrom)
{
    m_ggoFormat = moveFrom.m_ggoFormat;
    m_character = moveFrom.m_character;
    m_bufferSize = moveFrom.m_bufferSize;
    memcpy(&m_glyphMetrics, &moveFrom.m_glyphMetrics, sizeof(GLYPHMETRICS));
    m_spritePixels = std::move(moveFrom.m_spritePixels);

    return *this;
}
