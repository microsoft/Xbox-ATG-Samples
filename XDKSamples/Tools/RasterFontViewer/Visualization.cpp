//--------------------------------------------------------------------------------------
// Visualization.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"

#include "Visualization.h"

#include "FrontPanel\RasterFont.h"

using namespace ATG;

void ATG::DrawDebugBox(HDC hDC, const RECT & r, COLORREF color)
{
    HGDIOBJ stockPen = GetStockObject(DC_PEN);
    HGDIOBJ oldPen = SelectObject(hDC, stockPen);
    COLORREF oldColor = SetDCPenColor(hDC, color);

    int oldMode = SetBkMode(hDC, OPAQUE);

    MoveToEx(hDC, r.left, r.top, nullptr);
    LineTo(hDC, r.right, r.top);
    LineTo(hDC, r.right, r.bottom);
    LineTo(hDC, r.left, r.bottom);
    LineTo(hDC, r.left, r.top);

    SetBkMode(hDC, oldMode);
    SetDCPenColor(hDC, oldColor);
    SelectObject(hDC, oldPen);

    DeleteObject(stockPen);
}

namespace
{
    void DrawOneGlyph(
        const RasterGlyphSheet &glyphSheet,
        const RasterGlyphSheet::RasterGlyph &glyph,

        HDC memDC,
        RECT &r,
        unsigned int &baseline,
        unsigned int &col,
        unsigned int &row,
        
        unsigned int tableColumns,
        unsigned int maxCellHeight,
        unsigned int maxCellWidth,
        unsigned int vertPadding,
        unsigned int horzPadding)
    {
        // Render a debug box around the cell containing the glyph
        RECT cellRect;
        cellRect.top = r.top;
        cellRect.bottom = cellRect.top + maxCellHeight + 2;

        cellRect.left = r.left;
        cellRect.right = cellRect.left + maxCellWidth + 2;

        DrawDebugBox(memDC, cellRect, RGB(255, 0, 0));

        // Render debug box representing the black box of the glyph
        RECT bbRect;
        bbRect.top = baseline - glyph.blackBoxOriginY;
        bbRect.bottom = bbRect.top + glyph.blackBoxHeight + 1;
        bbRect.left = r.left + 1;
        bbRect.right = bbRect.left + glyph.blackBoxWidth + 1;

        DrawDebugBox(memDC, bbRect, RGB(0, 255, 0));

        //Render the glyph pixels
        glyphSheet.ForEachGlyphPixel(glyph, bbRect.left + 1, bbRect.top + 1, [&](unsigned x, unsigned y, uint8_t color) {
            uint8_t clr = !!color * 255;
            if (clr)
                SetPixel(memDC, x, y, RGB(clr, clr, clr));
        });

        // Advance to the next cell
        ++col;
        r.left += maxCellWidth + horzPadding;
        if (col == tableColumns)
        {
            col = 0;
            r.left = horzPadding;
            ++row;
            r.top += maxCellHeight + vertPadding;
            baseline = r.top + glyphSheet.GetEffectiveAscent();
        }

        r.right = r.left + maxCellWidth;
        r.bottom = r.top + maxCellHeight;

    }

} // ANONYMOUS namespace

HBITMAP ATG::DrawRasterGlyphSheet(const RasterGlyphSheet &glyphSheet, const std::vector<WCRANGE> &regions)
{
    // Set up a memory device context for laying out the glyph sheet
    HDC hDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hDC);

    // Figure out the set of glyphs to draw
    std::vector<wchar_t> charsToDraw;
    {
        std::set<wchar_t> setToDraw;
        for (auto itr = regions.begin(); itr != regions.end(); ++itr)
        {
            auto& range = *itr;
            for (wchar_t wc = range.wcLow; wc < range.wcLow + range.cGlyphs; ++wc)
            {
                auto glyph = glyphSheet.FindGlyph(wc);
                if (glyph)
                {
                    setToDraw.insert(wc);
                }
            }
        }
        for (auto itr = setToDraw.begin(); itr != setToDraw.end(); ++itr)
        {
            charsToDraw.push_back(*itr);
        }
    }

    std::sort(charsToDraw.begin(), charsToDraw.end());

    unsigned glyphCount = charsToDraw.empty() ? unsigned(glyphSheet.GetGlyphCount()) : unsigned(charsToDraw.size());
    unsigned maxCellWidth = 0;
    unsigned maxCellHeight = glyphSheet.GetEffectiveAscent() + glyphSheet.GetEffectiveDescent();
    
    if (charsToDraw.empty())
    {
        for (auto itr = glyphSheet.begin(); itr != glyphSheet.end(); ++itr)
        {
            auto& glyph = *itr;
            maxCellWidth = std::max(maxCellWidth, unsigned(glyph.blackBoxWidth));
        }
    }
    else
    {
        std::for_each(charsToDraw.begin(), charsToDraw.end(), [&](wchar_t wc) {
            auto glyph = *glyphSheet.FindGlyph(wc);
            maxCellWidth = std::max(maxCellWidth, unsigned(glyph.blackBoxWidth));
        });
    }

    if (maxCellWidth == 0 || maxCellHeight == 0)
    {
        throw DX::exception_fmt<64>("The glyph sheet doesn't contain any glyphs.");
    }

    // Pack the glyphs into a square bitmap
    unsigned lengthInPixels = unsigned(ceil(sqrt(maxCellHeight * maxCellWidth * glyphCount)));
    unsigned tableColumns = lengthInPixels / maxCellWidth;
    unsigned tableRows = lengthInPixels / maxCellHeight ? lengthInPixels / maxCellHeight : 1;

    while (tableColumns * tableRows < glyphCount)
    {
        ++tableColumns;
    }

    unsigned horzPadding = 4;
    unsigned vertPadding = 4;

    unsigned w = tableColumns * maxCellWidth + (tableColumns + 1) * horzPadding;
    unsigned h = tableRows * maxCellHeight + (tableRows + 1) * vertPadding;

    HBITMAP fontBitmap = CreateCompatibleBitmap(hDC, w, h);
    SelectObject(memDC, fontBitmap);

    RECT r;
    r.top = 0;
    r.left = 0;
    r.right = w;
    r.bottom = h;

    FillRect(memDC, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255)); // TODO: Do we need this anymore?

    r.top = vertPadding;
    r.bottom = r.top + maxCellHeight;
    r.left = horzPadding;
    r.right = r.left + maxCellWidth;

    unsigned int col = 0;
    unsigned int row = 0;
    unsigned baseline = r.top + glyphSheet.GetEffectiveAscent();

    if (charsToDraw.empty())
    {
        for (auto itr = glyphSheet.begin(); itr != glyphSheet.end(); ++itr)
        {
            DrawOneGlyph(
                glyphSheet,
                *itr,
                memDC,
                r,
                baseline,
                col,
                row,
                tableColumns,
                maxCellHeight,
                maxCellWidth,
                vertPadding,
                horzPadding);
        }
    }
    else
    {
        for (auto itr = charsToDraw.begin(); itr != charsToDraw.end(); ++itr)
        {
            auto& glyph = *glyphSheet.FindGlyph(*itr);
            DrawOneGlyph(
                glyphSheet,
                glyph,
                memDC,
                r,
                baseline,
                col,
                row,
                tableColumns,
                maxCellHeight,
                maxCellWidth,
                vertPadding,
                horzPadding);

        }
    }

    DeleteObject(memDC);
    return fontBitmap;


}
