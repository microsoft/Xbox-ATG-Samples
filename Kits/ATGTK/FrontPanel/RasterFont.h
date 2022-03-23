//--------------------------------------------------------------------------------------
// RasterFont.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "BufferDescriptor.h"
#include "Serialization.h"

namespace ATG
{
    template <typename STORAGE_UNIT_T>
    inline unsigned GetStorageSize(unsigned bitCount)
    {
        constexpr size_t unitSize = sizeof(STORAGE_UNIT_T) * 8;
        return unsigned(bitCount / unitSize + !!(bitCount % unitSize));
    }

    class RasterGlyphSheet
    {
    public:

        struct RasterGlyph
        {
            uint32_t  character;
            int16_t   blackBoxOriginX;
            int16_t   blackBoxOriginY;
            uint16_t  blackBoxWidth;
            uint16_t  blackBoxHeight;
            uint16_t  cellIncX;
            uint16_t  cellIncY;
            uint32_t  pixelIndex;

            static ClassVisitorActions<RasterGlyph> CreateClassVisitor()
            {
                ClassVisitorActions<RasterGlyph> actions;
                VisitMember(actions, &RasterGlyph::character);
                VisitMember(actions, &RasterGlyph::blackBoxOriginX);
                VisitMember(actions, &RasterGlyph::blackBoxOriginY);
                VisitMember(actions, &RasterGlyph::blackBoxWidth);
                VisitMember(actions, &RasterGlyph::blackBoxHeight);
                VisitMember(actions, &RasterGlyph::cellIncX);
                VisitMember(actions, &RasterGlyph::cellIncY);
                VisitMember(actions, &RasterGlyph::pixelIndex);
                return actions;
            }
        };

        struct KerningPair
        {
            wchar_t first;
            wchar_t second;
            int32_t amount;

            static ClassVisitorActions<KerningPair> CreateClassVisitor()
            {
                ClassVisitorActions<KerningPair> actions;
                VisitMember(actions, &KerningPair::first);
                VisitMember(actions, &KerningPair::second);
                VisitMember(actions, &KerningPair::amount);
                return actions;
            }
        };

        RasterGlyphSheet()
            : m_effectiveAscent(0)
            , m_effectiveDescent(0)
            , m_glyphPixelBytes(0)
            , m_defaultGlyph(nullptr)
        {
        }

        RasterGlyphSheet(
            uint16_t                         effectiveAscent,
            uint16_t                         effectiveDescent,
            const std::vector<RasterGlyph>  &glyphs,
            const std::vector<KerningPair>  &kerns,
            uint32_t                         glyphPixelBytes,
            const uint8_t                   *glyhpPixels,
            wchar_t                          defaultGlyph);

        using GlyphIterator = std::vector<RasterGlyph>::const_iterator;

        virtual ~RasterGlyphSheet();

        int GetEffectiveAscent() const { return m_effectiveAscent; }
        int GetEffectiveDescent() const { return m_effectiveDescent; }
        size_t GetGlyphCount() const { return m_glyphs.size(); }
        GlyphIterator begin() const { return m_glyphs.begin(); }
        GlyphIterator end() const { return m_glyphs.end(); }
        const RasterGlyph *GetDefaultGlyph() const { return m_defaultGlyph; }
        void SetDefaultGlyph(wchar_t character);
        const RasterGlyph *FindGlyph(wchar_t character) const;
        const KerningPair *FindKerningPair(wchar_t first, wchar_t second) const;

        template<typename ACTION_T>
        void ForEachGlyphPixel(const RasterGlyph &glyph, unsigned bbOriginX, unsigned bbOriginY, ACTION_T fn) const
        {
            const unsigned rows = glyph.blackBoxHeight;
            const unsigned cols = glyph.blackBoxWidth;
            const unsigned glyphByteCount = GetStorageSize<uint8_t>(cols * rows);

            uint8_t *srcByte = &m_glyphPixels.get()[glyph.pixelIndex];
            unsigned col = 0;
            unsigned row = 0;
            for (unsigned i = 0; i < glyphByteCount; ++i)
            {
                for (int shift = 7; shift >= 0; --shift)
                {
                    if (row == rows)
                        return;

                    uint8_t clr = (((srcByte[i]) >> shift) & 0x1) * 0xFF;
                    fn(col + bbOriginX, row + bbOriginY, clr);
                    ++col;

                    if (col == cols)
                    {
                        ++row;
                        col = 0;
                    }
                }
            }
        }

        template<typename ACTION_T>
        void ForEachGlyph(const wchar_t *text, unsigned lineSpacing, ACTION_T fn) const
        {
            unsigned cellOriginX = 0;
            unsigned cellOriginY = 0;

            for (const wchar_t *txt = text; *txt; ++txt)
            {
                wchar_t character = *txt;

                switch (character)
                {
                case '\r':
                    // Skip carriage returns
                    continue;

                case '\n':
                    // New line
                    cellOriginX = 0;
                    cellOriginY += lineSpacing;
                    break;

                default:
                    // Output this character
                    auto glyph = FindGlyph(character);

                    if (!iswspace(character)
                        || glyph->blackBoxWidth > 1
                        || glyph->blackBoxHeight > 1)
                    {
                        fn(*glyph, cellOriginX, cellOriginY);
                    }

                    int x = int(cellOriginX) + glyph->cellIncX;

                    // do kerning
                    const wchar_t *next = txt + 1;
                    if (*next)
                    {
                        auto kpair = FindKerningPair(*txt, *next);
                        if (kpair)
                        {
                            x += kpair->amount;
                        }

                    }
                    cellOriginX = (x < 0) ? 0 : x;

                    cellOriginY += glyph->cellIncY;
                }
            }
        }

        static ClassVisitorActions<RasterGlyphSheet> CreateClassVisitor()
        {
            ClassVisitorActions<RasterGlyphSheet> actions;
            VisitMember(actions, &RasterGlyphSheet::m_effectiveAscent);
            VisitMember(actions, &RasterGlyphSheet::m_effectiveDescent);
            VisitVectorCollection(actions, &RasterGlyphSheet::m_glyphs);
            VisitVectorCollection(actions, &RasterGlyphSheet::m_kerns);
            VisitUniquePointerCollection(actions, &RasterGlyphSheet::m_glyphPixels, &RasterGlyphSheet::m_glyphPixelBytes);
            VisitGetterSetter<RasterGlyphSheet, wchar_t>(
                actions,
                [](const RasterGlyphSheet &rgs) {
                return wchar_t(rgs.m_defaultGlyph ? rgs.m_defaultGlyph->character : '\0');
            },
                [](RasterGlyphSheet &rgs, wchar_t defaultGlyph) {
                rgs.SetDefaultGlyph(defaultGlyph);
            });
            return actions;
        }

    private:
        uint16_t                  m_effectiveAscent;
        uint16_t                  m_effectiveDescent;
        std::vector<RasterGlyph>  m_glyphs;
        std::vector<KerningPair>  m_kerns;
        uint32_t                  m_glyphPixelBytes;
        std::unique_ptr<uint8_t>  m_glyphPixels;
        const RasterGlyph        *m_defaultGlyph;
    };

    class RasterFont
    {
    public:
        RasterFont();
        RasterFont(const wchar_t *filename);
        RasterFont(std::unique_ptr<RasterGlyphSheet> &&glyphs);

        RasterFont(RasterFont &&other);
        RasterFont(const RasterFont &) = delete;
        RasterFont &operator=(RasterFont &&other);
        RasterFont &operator=(RasterFont&) = delete;

        ~RasterFont();

        void WriteToFile(const wchar_t *filename) const;

        const RasterGlyphSheet &GetGlyphs() const;

        unsigned GetLineSpacing() const;

        // MeastureString and MeasureStringFMt are useful for computing text bounds for layout purposes
        RECT MeasureString(const wchar_t *text) const;
        RECT MeasureStringFmt(const wchar_t *format, ...) const;

        // Basic text rendering to a buffer
        void DrawString(const struct BufferDesc &destBuffer, unsigned x, unsigned y, const wchar_t *text) const;

        // Formatted text rendering to a buffer
        void DrawStringFmt(const struct BufferDesc &destBuffer, unsigned x, unsigned y, const wchar_t *format, ...) const;

        // The following DrawString variants provide a shade parameter that lets you specify different shades of gray
        void DrawString(const struct BufferDesc &destBuffer, unsigned x, unsigned y, uint8_t shade, const wchar_t *text) const;
        void DrawStringFmt(const struct BufferDesc &destBuffer, unsigned x, unsigned y, uint8_t shade, const wchar_t *format, ...) const;

        // The glyph-specific methods are used for precisely positioning a single glyph
        RECT MeasureGlyph(wchar_t wch) const;
        void DrawGlyph(const struct BufferDesc &destBuffer, unsigned x, unsigned y, wchar_t wch, uint8_t shade = 0xFF) const;

    private:
        std::unique_ptr<RasterGlyphSheet> m_glyphs;
    };



    class RasterFontHeader
    {
    public:
        static constexpr uint16_t CURRENT_MAJOR_VERSION = 0;
        static constexpr uint16_t CURRENT_MINOR_VERSION = 1;

        RasterFontHeader();

        uint16_t GetMajorVersion() const { return m_majorVersion; }
        uint16_t GetMinorVersion() const { return m_minorVersion; }

        static ClassVisitorActions<RasterFontHeader> CreateClassVisitor()
        {
            ClassVisitorActions<RasterFontHeader> actions;

            // Serialize/Deserialize the header name
            VisitString(actions, &RasterFontHeader::m_headerName);

            // Serialize/Deserialize the major version
            VisitGetterSetter<RasterFontHeader, uint16_t>(
                actions,
                [](const RasterFontHeader &)
            {
                // Deliver the current major version to the serializer
                return CURRENT_MAJOR_VERSION;
            },
                [](RasterFontHeader &header, uint16_t majorVer)
            {
                // Consume the major version from the deserialization stream and set the major version
                header.m_majorVersion = majorVer;
            });

            // Serialize/Deserialize the minor version
            VisitGetterSetter<RasterFontHeader, uint16_t>(
                actions,
                [](const RasterFontHeader &)
            {
                // Deliver the current minor version to the serializer
                return CURRENT_MINOR_VERSION;
            },
                [](RasterFontHeader &header, uint16_t minorVer)
            {
                // Consume the minor version from the deserialization stream and set the minor version
                header.m_minorVersion = minorVer;
            });

            return actions;
        }

        bool IsValidHeaderName() const;

        bool IsCompatibleVersion() const
        {
            // Currently only accepting a strict match for both version numbers
            return (m_majorVersion == CURRENT_MAJOR_VERSION) && (m_minorVersion == CURRENT_MINOR_VERSION);
        }

    private:
        std::string m_headerName;
        uint16_t    m_majorVersion;
        uint16_t    m_minorVersion;
    };

} // namespace ATG
