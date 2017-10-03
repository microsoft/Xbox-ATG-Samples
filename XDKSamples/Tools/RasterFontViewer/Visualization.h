//--------------------------------------------------------------------------------------
// Visualization.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <Windows.h>
#include <stdint.h>
#include <vector>

namespace ATG
{
    class RasterGlyphSheet;

    void DrawDebugBox(HDC hDC, const RECT &r, COLORREF color);

    HBITMAP DrawRasterGlyphSheet(const RasterGlyphSheet &glyphSheet, const std::vector<WCRANGE> &regions);

} // namespace ATG