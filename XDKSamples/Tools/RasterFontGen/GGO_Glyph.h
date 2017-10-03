//--------------------------------------------------------------------------------------
// GGO_Glyph.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <stdint.h>
#include <memory>
#include <Windows.h>

namespace ATG
{
    // --------------------------------------------------------------------------------
    // GGO_Glyph
    // a glyph helper class based on GetGlyphOutline
    // --------------------------------------------------------------------------------
    class GGO_Glyph
    {
    public:
        GGO_Glyph();

        GGO_Glyph(HDC hdc, wchar_t wc, uint32_t ggoFormat);
            

        GGO_Glyph(const GGO_Glyph&) = delete;
        GGO_Glyph &operator=(const GGO_Glyph&) = delete;

        GGO_Glyph(GGO_Glyph &&moveFrom);

        GGO_Glyph &operator=(GGO_Glyph &&moveFrom);
        

        uint32_t GetGGOFormat() const
        {
            return m_ggoFormat;
        }

        uint32_t GetCharacterCode() const
        {
            return m_character;
        }

        const GLYPHMETRICS &GetMetrics() const
        {
            return m_glyphMetrics;
        }

        uint8_t *GetPixels() const
        {
            return m_spritePixels.get();
        }

        unsigned GetBufferSize() const
        {
            return m_bufferSize;
        }

        template<typename ACTION_T>
        void ForEachPixel(ACTION_T fn) const
        {
            if (m_ggoFormat == GGO_BITMAP)
            {
                ForEach_GGO_Pixel(fn);
            }
            else
            {
                ForEach_GGO_GRAY_N_Pixel(fn);
            }
        }

    private:

        template <typename ACTION_T>
        void ForEach_GGO_Pixel(ACTION_T fn) const
        {
            const unsigned rows = m_glyphMetrics.gmBlackBoxY;
            const unsigned cols = m_glyphMetrics.gmBlackBoxX;
            const unsigned doubleWordsPerRow = GetStorageSize<uint32_t>(cols);
            const unsigned bytesPerRow = doubleWordsPerRow * 4;

            uint8_t *srcByte = m_spritePixels.get();
            if (srcByte == nullptr)
            {
                return;
            }

            for (unsigned row = 0; row < rows; ++row)
            {
                unsigned col = 0;
                for (unsigned b = 0; b < bytesPerRow; ++b)
                {
                    for (int shift = 7; shift >= 0; --shift)
                    {
                        if (col >= cols)
                            break;

                        uint8_t clr = (((srcByte[b]) >> shift) & 0x1) * 0xFF;
                        fn(col, row, clr);
                        ++col;
                    }
                    if (col >= cols)
                        break;
                }
                srcByte += bytesPerRow;
            }
        }

        template <typename ACTION_T>
        void ForEach_GGO_GRAY_N_Pixel(ACTION_T fn) const
        {
            unsigned rows = m_glyphMetrics.gmBlackBoxY;
            unsigned cols = m_glyphMetrics.gmBlackBoxX;
            unsigned stride = m_bufferSize / rows;

            uint8_t *srcPixels = m_spritePixels.get();

            if (srcPixels == nullptr)
            {
                return;
            }

            for (unsigned i = 0; i < cols; ++i)
            {
                for (unsigned j = 0; j < rows; ++j)
                {
                    uint8_t clr = srcPixels[i + j * stride];
                    fn(i, j, clr);
                }
            }
        }

        uint32_t                   m_ggoFormat;
        uint32_t                   m_character;
        GLYPHMETRICS               m_glyphMetrics;
        std::unique_ptr<uint8_t[]> m_spritePixels;
        unsigned                   m_bufferSize;
    };

} // namespace ATG
