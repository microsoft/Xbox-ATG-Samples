//--------------------------------------------------------------------------------------
// RasterFont.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "RasterFont.h"
#include "FileHelpers.h"
#include "OSHelpers.h"
#include "Serialization.h"
#include "BufferDescriptor.h"

#include <algorithm>
#include <cstdarg>
#include <fstream>
#include <new>

#include <io.h>

using namespace ATG;
using namespace DX;

#pragma warning(disable : 4365)

//--------------------------------------------------------------------------------------
// RasterGlyphSheet implementation
//--------------------------------------------------------------------------------------
#pragma region RasterGlyphSheet

namespace ATG
{
    static inline bool operator<(const RasterGlyphSheet::RasterGlyph &left, const RasterGlyphSheet::RasterGlyph &right)
    {
        return left.character < right.character;
    }

    static inline bool operator< (const RasterGlyphSheet::RasterGlyph &left, wchar_t right)
    {
        return left.character < right;
    }

    static inline bool operator<(const RasterGlyphSheet::KerningPair &leftPair, const RasterGlyphSheet::KerningPair &rightPair)
    {
        return (leftPair.first < rightPair.first) || ((leftPair.first == rightPair.first) && (leftPair.second < rightPair.second));
    }
}

RasterGlyphSheet::RasterGlyphSheet(
    uint16_t                        effectiveAscent,
    uint16_t                        effectiveDescent,
    const std::vector<RasterGlyph> &glyphs,
    const std::vector<KerningPair> &kerns,
    uint32_t                        glyphPixelBytes,
    const uint8_t                  *glyphPixels,
    wchar_t                         defaultGlyph)
    : m_effectiveAscent(effectiveAscent)
    , m_effectiveDescent(effectiveDescent)
    , m_glyphs(glyphs)
    , m_kerns(kerns)
    , m_glyphPixelBytes(glyphPixelBytes)
    , m_defaultGlyph(nullptr)
{
    // Sort the glyphs
    std::sort(m_glyphs.begin(), m_glyphs.end());

    // Sort the kerning pairs
    std::sort(m_kerns.begin(), m_kerns.end());

    //assert(false); // NEED TO SORT THE GLYPHS AND KERNING PAIRS!
    m_glyphPixels.reset(new uint8_t[glyphPixelBytes]);
    memcpy_s(m_glyphPixels.get(), glyphPixelBytes, glyphPixels, glyphPixelBytes);
    SetDefaultGlyph(defaultGlyph);
}

RasterGlyphSheet::~RasterGlyphSheet()
{
}

void RasterGlyphSheet::SetDefaultGlyph(wchar_t character)
{
    m_defaultGlyph = FindGlyph(character);
}

const RasterGlyphSheet::RasterGlyph *RasterGlyphSheet::FindGlyph(wchar_t character) const
{
    auto glyph = std::lower_bound(m_glyphs.begin(), m_glyphs.end(), character);

    if (glyph != m_glyphs.end() && glyph->character == character)
    {
        return &*glyph;
    }

    return m_defaultGlyph;
}

const RasterGlyphSheet::KerningPair * RasterGlyphSheet::FindKerningPair(wchar_t first, wchar_t second) const
{
    KerningPair testPair = {};
    testPair.amount = 0;
    testPair.first = first;
    testPair.second = second;

    auto kPair = std::lower_bound(m_kerns.begin(), m_kerns.end(), testPair);

    if (kPair != m_kerns.end() && kPair->first == first && kPair->second == second)
    {
        return &*kPair;
    }

    return nullptr;
}

#pragma endregion

//--------------------------------------------------------------------------------------
// RasterFont implementation
//--------------------------------------------------------------------------------------
#pragma region RasterFont

RasterFont::RasterFont()
{
}

RasterFont::RasterFont(const wchar_t *filename)
    : m_glyphs(new RasterGlyphSheet)
{
    wchar_t inFile[MAX_PATH] = L"";
    {
        wcscpy_s(inFile, filename);

        // Add a file extension if it doesn't already have one
        wchar_t *dot = wcsrchr(inFile, '.');
        if (!dot)
        {
            wcscat_s(inFile, L".rasterfont");
        }
    }

    std::ifstream stream(inFile, std::ios::in | std::ios::binary);
    if (stream.bad() || stream.fail())
    {
        uint32_t err = GetLastError();
        char buffer[128];
        sprintf_s(buffer, "Unable to open file %ls. Error code: 0x%08X", inFile, err);
        throw std::exception(buffer);
    }

    StreamDeserializationBuffer sdb(stream);
    SerializationHeader header;

    try
    {
        Deserialize(header, sdb);
    }
    catch (std::bad_alloc &)
    {
        char buffer[128];
        sprintf_s(buffer, "File (%ls) does not contain a valid serialization header", inFile);
        throw std::exception(buffer);
    }

    using SHFlags = SerializationHeader::SerializationFlags;

    if (!header.CheckFlag(SHFlags::is_current_version))
    {
        char buffer[128];
        sprintf_s(buffer, "File (%ls) was serialized with an incompatible version", inFile);
        throw std::exception(buffer);
    }

    if (!header.CheckFlag(SHFlags::is_host_endian))
    {
        char buffer[128];
        sprintf_s(buffer, "File (%ls) was serialized with an incompatible byte-order", inFile);
        throw std::exception(buffer);
    }

    RasterFontHeader rfHeader;
    try
    {
        Deserialize(rfHeader, sdb);
    }
    catch (std::bad_alloc &)
    {
        char buffer[128];
        sprintf_s(buffer, "File (%ls) is not a valid Raster Font file", inFile);
        throw std::exception(buffer);
    }

    if (!rfHeader.IsValidHeaderName())
    {
        char buffer[128];
        sprintf_s(buffer, "File (%ls) is not a valid Raster Font file", inFile);
        throw std::exception(buffer);
    }
        
    if (!rfHeader.IsCompatibleVersion())
    {
        char buffer[128];
        sprintf_s(buffer, "File (%ls) is not a compatible Raster Font version", inFile);
        throw std::exception(buffer);
    }

    Deserialize(*m_glyphs.get(), sdb);
}

RasterFont::RasterFont(std::unique_ptr<RasterGlyphSheet>&& glyphs)
    : m_glyphs(std::move(glyphs))
{
}

RasterFont::RasterFont(RasterFont && other)
{
    m_glyphs = std::move(other.m_glyphs);
}

RasterFont & RasterFont::operator=(RasterFont && other)
{
    RasterFont temp(std::move(other));
    m_glyphs = std::move(temp.m_glyphs);
    return *this;
}

RasterFont::~RasterFont()
{}

void RasterFont::WriteToFile(const wchar_t * filename) const
{
    wchar_t outFile[MAX_PATH] = L"";
    {
        wcscpy_s(outFile, filename);

        // Add a file extension if it doesn't already have one
        wchar_t *dot = wcsrchr(outFile, '.');
        if (!dot)
        {
            wcscat_s(outFile, L".rasterfont");
        }
    }

    // Note: using a std::ofstream below, however we sitll use CreateFile2 to get a handle in order
    // to support the auto_delete_file functionality
    ScopedHandle hFile(safe_handle(CreateFile2(outFile, GENERIC_WRITE | DELETE, 0, CREATE_ALWAYS, nullptr)));

    if (!hFile)
    {
        char buffer[128];
        uint32_t err = GetLastError();
        sprintf_s(buffer, "Unable to create file %ls. Error code: 0x%08X\n", outFile, err);
        throw std::exception(buffer);
    }

    auto_delete_file delonFail(hFile.get());

    // Serialize the glyph sheet to a filestream
    {
        // Some gymnastics to get the file handle into the ofstream
        int fileDesc = _open_osfhandle((intptr_t)hFile.get(), 0);
        if (fileDesc == -1)
        {
            char buffer[128];
            uint32_t err = GetLastError();
            sprintf_s(buffer, "Unable to get file descriptor %ls. Error code: 0x%08X\n", outFile, err);
            throw std::exception(buffer);
        }

        FILE *file = _fdopen(fileDesc, "w");
        if (file != nullptr)
        {
            std::ofstream stream(file);
            hFile.reset();

            StreamSerializationBuffer ssb(stream);
            SerializationHeader header;
            Serialize(header, ssb);

            RasterFontHeader rfHeader;
            Serialize(rfHeader, ssb);

            Serialize(*m_glyphs.get(), ssb);

            // This will close the stream, fileDesc and the file handle
            stream.close();
        }
    }

    delonFail.clear();
}

const RasterGlyphSheet & RasterFont::GetGlyphs() const
{
    return *m_glyphs.get();
}

unsigned RasterFont::GetLineSpacing() const
{
    return m_glyphs->GetEffectiveAscent() + m_glyphs->GetEffectiveDescent();
}

RECT RasterFont::MeasureString(const wchar_t *text) const
{
    RECT r = {};

    if (!m_glyphs.get())
    {
        assert(m_glyphs.get());
        return r;
    }

    auto& glyphSheet = *m_glyphs.get();
   
    unsigned lineSpacing = GetLineSpacing();

    glyphSheet.ForEachGlyph(text, lineSpacing, [&](const RasterGlyphSheet::RasterGlyph &glyph, unsigned cellOriginX, unsigned cellOriginY) {
        RECT bbRect = {};
        
        bbRect.left = cellOriginX + glyph.blackBoxOriginX;
        bbRect.right = bbRect.left + glyph.blackBoxWidth;
        
        bbRect.top = cellOriginY - glyph.blackBoxOriginY;
        bbRect.bottom = bbRect.top + glyph.blackBoxHeight;

        r.top = std::min(r.top, bbRect.top);
        r.bottom = std::max(r.bottom, bbRect.bottom);

        r.left = std::min(r.left, bbRect.left);
        r.right = std::max(r.right, bbRect.right);
    });

    return r;
}

RECT RasterFont::MeasureStringFmt(const wchar_t *format, ...) const
{
    std::unique_ptr<wchar_t[]> buffer;
    {
        va_list args;
        va_start(args, format);

        auto count = _vscwprintf(format, args);
        buffer = std::make_unique<wchar_t[]>(count + 1);
        buffer.get()[count] = L'\0';

        vswprintf_s(buffer.get(), count + 1, format, args);

        va_end(args);
    }

    return MeasureString(buffer.get());
}

void RasterFont::DrawString(const BufferDesc &destBuffer, unsigned x, unsigned y, const wchar_t *text) const
{
    DrawString(destBuffer, x, y, 0xFF, text);
}

void RasterFont::DrawStringFmt(const BufferDesc & destBuffer, unsigned x, unsigned y, const wchar_t *format, ...) const
{
    std::unique_ptr<wchar_t[]> buffer;
    {
        va_list args;
        va_start(args, format);

        auto count = _vscwprintf(format, args);
        buffer = std::make_unique<wchar_t[]>(count + 1);
        buffer.get()[count] = L'\0';

        vswprintf_s(buffer.get(), count + 1, format, args);

        va_end(args);
    }

    return DrawString(destBuffer, x, y, buffer.get());
}

void RasterFont::DrawString(const BufferDesc & destBuffer, unsigned x, unsigned y, uint8_t shade, const wchar_t * text) const
{
    if (!m_glyphs.get())
    {
        assert(m_glyphs.get());
        return;
    }

    auto& glyphSheet = *m_glyphs.get();

    unsigned lineSpacing = GetLineSpacing();

    RECT r = MeasureString(text);
    int baseline = -r.top;

    unsigned w = r.right - r.left + x;
    unsigned h = baseline + r.bottom + y;

    r.top = 0; r.bottom = w;
    r.left = 0; r.right = h;

    glyphSheet.ForEachGlyph(text, lineSpacing, [&](const RasterGlyphSheet::RasterGlyph &glyph, unsigned cellOriginX, unsigned cellOriginY) {
        RECT bbRect = {};
        bbRect.left = x + cellOriginX + glyph.blackBoxOriginX;
        bbRect.right = bbRect.left + glyph.blackBoxWidth;
        bbRect.top = y + baseline + cellOriginY - glyph.blackBoxOriginY;
        bbRect.bottom = bbRect.top + glyph.blackBoxHeight;

        glyphSheet.ForEachGlyphPixel(glyph, bbRect.left, bbRect.top, [&](unsigned col, unsigned row, uint8_t clr) {
            uint8_t pxl = !!clr * shade;
            if (clr)
                SetPixel(destBuffer, col, row, pxl);
        });
    });
}

void RasterFont::DrawStringFmt(const BufferDesc & destBuffer, unsigned x, unsigned y, uint8_t shade, const wchar_t * format, ...) const
{
    std::unique_ptr<wchar_t[]> buffer;
    {
        va_list args;
        va_start(args, format);

        auto count = _vscwprintf(format, args);
        buffer = std::make_unique<wchar_t[]>(count + 1);
        buffer.get()[count] = L'\0';

        vswprintf_s(buffer.get(), count + 1, format, args);

        va_end(args);
    }

    return DrawString(destBuffer, x, y, shade, buffer.get());
}

RECT RasterFont::MeasureGlyph(wchar_t wch) const
{
    RECT r = {};

    if (!m_glyphs.get())
    {
        assert(m_glyphs.get());
        return r;
    }

    auto& glyphSheet = *m_glyphs.get();

    auto& glyph = *glyphSheet.FindGlyph(wch);
    r.right = glyph.blackBoxWidth;
    r.bottom = glyph.blackBoxHeight;

    return r;
}

void RasterFont::DrawGlyph(const BufferDesc & destBuffer, unsigned x, unsigned y, wchar_t wch, uint8_t shade) const
{
    assert(m_glyphs.get());

    auto& glyphSheet = *m_glyphs.get();

    auto& glyph = *glyphSheet.FindGlyph(wch);


    glyphSheet.ForEachGlyphPixel(glyph, x, y, [&](unsigned col, unsigned row, uint8_t clr)
    {
        uint8_t pxl = !!clr * shade;
        if (clr)
            SetPixel(destBuffer, col, row, pxl);
    });

}
#pragma endregion

//--------------------------------------------------------------------------------------
// RasterFontHeader implementation
//--------------------------------------------------------------------------------------
#pragma region RasterFontHeader

#define RASTER_FONT_HEADER_NAME "Raster Font"
RasterFontHeader::RasterFontHeader()
    : m_headerName(RASTER_FONT_HEADER_NAME)
    , m_majorVersion(0)
    , m_minorVersion(0)
{}

bool RasterFontHeader::IsValidHeaderName() const
{
    return (m_headerName == RASTER_FONT_HEADER_NAME);
}

#pragma endregion
