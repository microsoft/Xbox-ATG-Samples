//--------------------------------------------------------------------------------------
// main.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"

#include "GGO_Glyph.h"
#include "FrontPanel\RasterFont.h"
#include "CommandLineHelpers.h"

#include <conio.h>

using namespace ATG;

namespace
{
    enum OPTIONS : uint32_t
    {
        OPT_TYPEFACE = 1,
        OPT_HEIGHT,
        OPT_WEIGHT,
        OPT_ITALIC,
        OPT_UNDERLINE,
        OPT_STRIKEOUT,
        OPT_CHARSET,
        OPT_QUALITY,
        OPT_PITCH,
        OPT_FAMILY,
        OPT_GGO_DEPTH,
        OPT_CHARACTER_REGION,
        OPT_OUTPUT_FILENAME,
        OPT_DEFAULT_GLYPH,
        OPT_OVERWRITE,
        OPT_MAX
    };

    static_assert(OPT_MAX <= 64, "Options must fit into a uint64_t bitfield");


    const SValue g_Options[] =
    {
        { L"tf", OPT_TYPEFACE },
        { L"h",  OPT_HEIGHT },
        { L"w",  OPT_WEIGHT },
        { L"it", OPT_ITALIC },
        { L"ul", OPT_UNDERLINE },
        { L"so", OPT_STRIKEOUT },
        { L"cs", OPT_CHARSET },
        { L"q",  OPT_QUALITY },
        { L"p",  OPT_PITCH },
        { L"fa", OPT_FAMILY },
        { L"d",  OPT_GGO_DEPTH },
        { L"cr", OPT_CHARACTER_REGION },
        { L"of", OPT_OUTPUT_FILENAME },
        { L"ow", OPT_OVERWRITE },
        { L"dg", OPT_DEFAULT_GLYPH },
    };

#define DEF_WEIGHT(wght) { L#wght, FW_ ## wght }
    const SValue g_Weights[] =
    {
        DEF_WEIGHT(DONTCARE),
        DEF_WEIGHT(THIN),
        DEF_WEIGHT(EXTRALIGHT),
        DEF_WEIGHT(LIGHT),
        DEF_WEIGHT(NORMAL),
        DEF_WEIGHT(MEDIUM),
        DEF_WEIGHT(SEMIBOLD),
        DEF_WEIGHT(BOLD),
        DEF_WEIGHT(EXTRABOLD),
        DEF_WEIGHT(HEAVY),
        DEF_WEIGHT(ULTRALIGHT),
        DEF_WEIGHT(REGULAR),
        DEF_WEIGHT(DEMIBOLD),
        DEF_WEIGHT(ULTRABOLD),
        DEF_WEIGHT(BLACK)
    };
#undef DEF_WEIGHT

#define DEF_CHARSET(chrst) { L#chrst, chrst ## _CHARSET }
    const SValue g_Charsets[] =
    {
        DEF_CHARSET(ANSI),
        DEF_CHARSET(DEFAULT),
        DEF_CHARSET(SYMBOL),
        DEF_CHARSET(SHIFTJIS),
        DEF_CHARSET(HANGEUL),
        DEF_CHARSET(HANGUL),
        DEF_CHARSET(GB2312),
        DEF_CHARSET(CHINESEBIG5),
        DEF_CHARSET(OEM),
        DEF_CHARSET(JOHAB),
        DEF_CHARSET(HEBREW),
        DEF_CHARSET(ARABIC),
        DEF_CHARSET(GREEK),
        DEF_CHARSET(TURKISH),
        DEF_CHARSET(VIETNAMESE),
        DEF_CHARSET(THAI),
        DEF_CHARSET(EASTEUROPE),
        DEF_CHARSET(RUSSIAN),
        DEF_CHARSET(MAC),
        DEF_CHARSET(BALTIC)
    };
#undef DEF_CHARSET

#define DEF_QUALITY(qlty) { L#qlty, qlty ## _QUALITY }
    const SValue g_Qualities[] =
    {
        DEF_QUALITY(ANTIALIASED),
        DEF_QUALITY(CLEARTYPE),
        DEF_QUALITY(DEFAULT),
        DEF_QUALITY(DRAFT),
        DEF_QUALITY(NONANTIALIASED),
        DEF_QUALITY(PROOF)
    };
#undef DEF_QUALITY

#define DEF_PITCH(ptch) { L#ptch, ptch ## _PITCH}
    const SValue g_Pitches[] =
    {
        DEF_PITCH(DEFAULT),
        DEF_PITCH(FIXED),
        DEF_PITCH(VARIABLE)
    };
#undef DEF_PITCH

#define DEF_FAMILY(fam) { L#fam, FF_ ## fam }
    const SValue g_Families[] =
    {
        DEF_FAMILY(DECORATIVE),
        DEF_FAMILY(DONTCARE),
        DEF_FAMILY(MODERN),
        DEF_FAMILY(ROMAN),
        DEF_FAMILY(SCRIPT),
        DEF_FAMILY(SWISS)
    };
#undef DEF_FAMILY

#define DEF_GGO_DEPTH(ggo) { L#ggo, GGO_ ## ggo ## _BITMAP }
    const SValue g_GGO_Depths[] =
    {
        { L"1BPP", GGO_BITMAP },
        DEF_GGO_DEPTH(GRAY2),
        DEF_GGO_DEPTH(GRAY4),
        DEF_GGO_DEPTH(GRAY8)
    };
#undef DEF_GGO_DEPTH

    bool ParseDefaultGlyph(const wchar_t *charCode, wchar_t &defaultGlyph, std::vector<WCRANGE> &regions)
    {
        const wchar_t *hexCode = charCode;

        // Match and consume the hex prefix '0x'
        if (_wcsnicmp(L"0x", hexCode, 2) != 0)
        {
            wprintf(L"Expected a hexidecimal character code specification\n");
            return false;
        }
        hexCode += 2;

        unsigned code = 0;
        // Scan the hex value
        if (swscanf_s(hexCode, L"%lx", &code) != 1)
        {
            wprintf(L"Expected a hexidecimal value following the prefix ('0x')\n");
            return false;
        }
        hexCode = ConsumeDigits(hexCode);

        if (code > 0xFFFF)
        {
            wprintf(L"The hexidecimal value of the character code specification is too large (0x%X)\n", code);
            return false;
        }

        if (*hexCode)
        {
            wprintf(L"Unexpected characters following the character code specification (%ls).\n", hexCode);
            return false;
        }

        defaultGlyph = code;
        WCRANGE range = { wchar_t(code), 1 };
        regions.push_back(range);
        return true;
    }

    void PrintLogo()
    {
        wprintf(L"Microsoft (R) Raster Font Generator for for Xbox One\n");
        wprintf(L"Copyright (C) Microsoft Corp. All rights reserved.\n");
#ifdef _DEBUG
        wprintf(L"*** Debug build ***\n");
#endif
        wprintf(L"\n");
    }

    void PrintUsage()
    {
        PrintLogo();

        wprintf(L"Usage: rasterfontgen <options> \n\n");
        wprintf(L"   -tf <string>        Name of the font typeface\n");
        wprintf(L"   -h <number>         [REQUIRED] Height of the font's character cell in logical units\n");
        wprintf(L"   -w <weight>         Font weight\n");
        wprintf(L"   -it                 Specifies an italic font\n");
        wprintf(L"   -ul                 Specifies an underlined font\n");
        wprintf(L"   -so                 Specifies a strikeout font\n");
        wprintf(L"   -cs <charset>       The character set\n");
        wprintf(L"   -q <quality>        The output quality\n");
        wprintf(L"   -p <pitch>          The pitch of the font\n");
        wprintf(L"   -fa <family>        The family of the font\n");
        wprintf(L"   -d <depth>          The GetGlyphOutline (GGO) pixel depth\n");
        wprintf(L"   -cr:<range>         Character region. Specifies a range of unicode code points to include in the font.\n");
        wprintf(L"                       Examples: -cr:a-z -cr:0x1200-0x1250 -cr:0x1234\n");
        wprintf(L"   -of <filename>      [REQUIRED] Name of the output file\n");
        wprintf(L"   -ow                 Overwrite the output file if it already exists\n");
        wprintf(L"   -dg <caracter code> Specifies the default glyph to use when the font does not support a particular character code\n");
        wprintf(L"                       Examples: -dg:0x1234\n");
        
        const wchar_t wgts[] = L"\n   <weight>: ";
        wprintf(wgts);
        PrintTable(_countof(wgts) - 2, g_Weights);

        const wchar_t chsts[] = L"\n   <charset>: ";
        wprintf(chsts);
        PrintTable(_countof(chsts) - 2, g_Charsets);

        const wchar_t qlties[] = L"\n   <quality>: ";
        wprintf(qlties);
        PrintTable(_countof(qlties) - 2, g_Qualities);

        const wchar_t ptchs[] = L"\n   <pitch>: ";
        wprintf(ptchs);
        PrintTable(_countof(ptchs) - 2, g_Pitches);

        const wchar_t fams[] = L"\n   <family>: ";
        wprintf(fams);
        PrintTable(_countof(fams) - 2, g_Families);

        const wchar_t dpths[] = L"\n   <depth>: ";
        wprintf(dpths);
        PrintTable(_countof(dpths) - 2, g_GGO_Depths);
    }

    void GetGlyphRanges(HDC hDC, std::vector<WCRANGE> &ranges)
    {
        std::unique_ptr<uint8_t[]> glyphSetBytes;
        GLYPHSET *glyphSet = nullptr;
        {
            auto size = GetFontUnicodeRanges(hDC, nullptr);
            glyphSetBytes = std::make_unique<uint8_t[]>(size);

            glyphSet = reinterpret_cast<GLYPHSET*>(glyphSetBytes.get());

            if (!GetFontUnicodeRanges(hDC, glyphSet))
            {
                throw std::exception("Unable to get unicode ranges for font.");
            }

            for (unsigned r = 0; r < glyphSet->cRanges; ++r)
            {
                ranges.push_back(glyphSet->ranges[r]);
            }
        }
    }
        

    bool IsCodePointInRange(wchar_t codePoint, const WCRANGE &range)
    {
        wchar_t firstCharacter = range.wcLow;
        wchar_t lastCharacter = firstCharacter + range.cGlyphs - 1;

        return (codePoint >= firstCharacter && codePoint <= lastCharacter);
    }

    bool FilterKerningPair(const KERNINGPAIR &pair, const std::vector<WCRANGE> &ranges)
    {
        bool firstInRange = false;
        bool secondInRange = false;

        for (unsigned r = 0; r < ranges.size(); ++r)
        {
            firstInRange = firstInRange || IsCodePointInRange(pair.wFirst, ranges[r]);
            secondInRange = secondInRange || IsCodePointInRange(pair.wSecond, ranges[r]);

            if (firstInRange && secondInRange)
            {
                return true;
            }
        }

        return false;
    }

    bool FilterCodePoint(wchar_t codePoint, const std::vector<WCRANGE> &ranges)
    {
        for(unsigned r = 0; r < ranges.size(); ++r)
        {
            if (IsCodePointInRange(codePoint, ranges[r]))
            {
                return true;
            }
        }
        return false;
    }

    void Create_GGO_Glyphs(
        LOGFONT                    *logfont,
        unsigned                    ggoDepth,
        const std::vector<WCRANGE> &requestedRanges,
        std::vector<KERNINGPAIR>   &kerningPairs, 
        std::vector<GGO_Glyph>     &ggoGlyphs,
        wchar_t                     requestedDefaultGlyph)
    {
        // Set up a memory device context for laying out the glyph sheet
        HDC hDC = GetDC(nullptr);
        HDC memDC = CreateCompatibleDC(hDC);

        HFONT font = CreateFontIndirect(logfont);

        if (!font)
        {
            throw std::exception("Could not create font.");
        }

        SelectObject(memDC, font);

        wchar_t defaultGlyph = requestedDefaultGlyph;

        // Check to see if it is a TrueType font and early out if not
        {
            TEXTMETRIC metrics = {};
            if (!GetTextMetrics(memDC, &metrics))
            {
                throw std::exception("Could not get text metrics.");
            }

            bool isTrueType = (metrics.tmPitchAndFamily & TMPF_TRUETYPE) == TMPF_TRUETYPE;
            if (!isTrueType)
            {
                throw std::exception("Requested font is not a TrueType font");
            }

            if (requestedDefaultGlyph == '\0')
            {
                requestedDefaultGlyph = metrics.tmDefaultChar;
            }
        }

        // Get the supported Unicode ranges for the font
        std::vector<WCRANGE> supportedRanges;
        GetGlyphRanges(memDC, supportedRanges);

        // Get the kerning pairs for the font
        {
            unsigned numPairs = GetKerningPairs(memDC, 0, nullptr);
            std::unique_ptr<KERNINGPAIR[]> kerns = std::make_unique<KERNINGPAIR[]>(numPairs);

            auto test = GetKerningPairs(memDC, numPairs, kerns.get());
            if (numPairs != test)
            {
                throw std::exception("Unable to get kerning pairs for the font.");
            }

            for (unsigned i = 0; i < numPairs; ++i)
            {
                KERNINGPAIR &pair = kerns[i];
                if (requestedRanges.size() == 0)
                {
                    if (FilterKerningPair(pair, supportedRanges))
                    {
                        kerningPairs.push_back(pair);
                    }
                }
                else
                {
                    if (FilterKerningPair(pair, requestedRanges) && FilterKerningPair(pair, supportedRanges))
                    {
                        kerningPairs.push_back(pair);
                    }
                }
            }
        }

        // Create the GGO_Glyphs for the font
        {
            std::set<wchar_t> includedCodePoints;
            includedCodePoints.insert(defaultGlyph);
            if (requestedRanges.size() == 0)
            {
                for (int r = 0; r < supportedRanges.size(); ++r)
                {
                    const WCRANGE &range = supportedRanges[r];
                    wchar_t firstCharacter = range.wcLow;
                    wchar_t lastCharacter = range.wcLow + range.cGlyphs - 1;

                    for (wchar_t wc = firstCharacter; wc <= lastCharacter; ++wc)
                    {
                        includedCodePoints.insert(wc);
                    }
                }
            }
            else
            {
                for (int rr = 0; rr < requestedRanges.size(); ++rr)
                {
                    const WCRANGE &range = requestedRanges[rr];
                    wchar_t firstCharacter = range.wcLow;
                    wchar_t lastCharacter = range.wcLow + range.cGlyphs - 1;

                    for (wchar_t wc = firstCharacter; wc <= lastCharacter; ++wc)
                    {
                        if (FilterCodePoint(wc, supportedRanges))
                        {
                            includedCodePoints.insert(wc);
                        }
                        else
                        {
                            wprintf(L"The code point, 0x%04x, is not supported by the font and will not be included.\n", wc);
                        }
                    }
                }
            }

            for (auto itr = includedCodePoints.begin(); itr != includedCodePoints.end(); ++itr)
            {
                ggoGlyphs.emplace_back(memDC, *itr, ggoDepth);
            }
        }

        DeleteObject(font);
        DeleteObject(memDC);
    }

    size_t CreatePixelBuffer(const std::vector<GGO_Glyph> &ggoGlyphs, uint8_t *pixels, size_t pixelBufferSize)
    {
        
        size_t pixelByteCount = 0;
        // Allocate the buffer for the glyph pixels
        {
            for (int i = 0; i < ggoGlyphs.size(); ++i)
            {
                auto metrics = ggoGlyphs[i].GetMetrics();
                pixelByteCount += GetStorageSize<uint8_t>(metrics.gmBlackBoxX * metrics.gmBlackBoxY);
            }

            if (pixels == nullptr || pixelByteCount > pixelBufferSize)
            {
                return pixelByteCount;
            }

            memset(pixels, 0, pixelBufferSize);
        }

        // Copy the glyphs into the pixel buffer
        {
            uint8_t *pxls = pixels;
            for (int i = 0; i < ggoGlyphs.size(); ++i)
            {
                unsigned pixelIdx = 0;
                ggoGlyphs[i].ForEachPixel([&](unsigned col, unsigned row, uint8_t clr) {
                    unsigned shift = 7 - (pixelIdx % 8);
                    pxls[pixelIdx / 8] |= (!!clr) << shift;
                    pixelIdx++;
                });

                auto metrics = ggoGlyphs[i].GetMetrics();

                pxls += GetStorageSize<uint8_t>(metrics.gmBlackBoxX * metrics.gmBlackBoxY);
            }
        }

        return pixelByteCount;
    }

    void ComputeAscentDescent(
        const std::vector<GGO_Glyph> &ggoGlyphs,
        unsigned &effectiveAscent,
        unsigned &effectiveDescent)
    {
        effectiveAscent = 0;
        effectiveDescent = 0;

        // Compute the effective ascent and effective descent based on the actual glyphs
        {
            for (int i = 0; i < ggoGlyphs.size(); ++i)
            {
                auto metrics = ggoGlyphs[i].GetMetrics();

                int ascent = metrics.gmptGlyphOrigin.y;
                if (ascent > 0)
                {
                    // Positive vertical coordinate places the upper left corner of the glyph's 
                    // "black box" above the glyph base-line.
                    effectiveAscent = std::max(effectiveAscent, unsigned(ascent));
                }

                // Compute the descent for the character as a signed vertical distance from the
                // lower-left corner of the glyph's "black box" to the glyph base-line.
                // A negative value indicates that nothing is drawn below the base-line.
                int descent = metrics.gmBlackBoxY - metrics.gmptGlyphOrigin.y;
                if (descent > 0)
                {
                    effectiveDescent = std::max(effectiveDescent, unsigned(descent));
                }
            }

            ++effectiveAscent;
        }
    }
    
} // ANONYMOUS namespace





int wmain(int argc, wchar_t *argv[])
{
    // Parameters and defaults
    const wchar_t       *typeFace = nullptr;
    unsigned             fontHeight = 0;
    unsigned             fontWeight = FW_DONTCARE;
    bool                 useItalic = false;
    bool                 useUnderline = false;
    bool                 useStrikeOut = false;
    unsigned             charset = DEFAULT_CHARSET;
    unsigned             quality = DEFAULT_QUALITY;
    unsigned             pitch = DEFAULT_PITCH;
    unsigned             family = FF_DONTCARE;
    unsigned             ggoDepth = GGO_BITMAP;
    const wchar_t       *outputFilename = nullptr;
    bool                 overwriteOutputFile = false;
    wchar_t              defaultGlyph = '\0';
    std::vector<WCRANGE> regions;

    unsigned individualOptions = 0;
    
    // Parse the command line
    for (int argIdx = 0; argIdx < argc; ++argIdx)
    {
        wchar_t *arg = argv[argIdx];

        if (('-' == arg[0]) || ('/' == arg[0]))
        {
            arg++;

            wchar_t *value = nullptr;
            for (value = arg; *value && (':' != *value); ++value);
            if (*value)
                *value++ = 0;

            unsigned option = 0;
            if( !LookupByName(arg, g_Options, option) || (individualOptions & (1 << option)))
            {
                PrintUsage();
                return 1;
            }

            if (option != OPT_CHARACTER_REGION)
            {
                individualOptions |= (1 << option);
            }

            // Handle options with an additional value parameter
            switch (option)
            {
            case OPT_TYPEFACE:
            case OPT_HEIGHT:
            case OPT_WEIGHT:            
            case OPT_CHARSET:
            case OPT_QUALITY:
            case OPT_PITCH:
            case OPT_FAMILY:
            case OPT_GGO_DEPTH:
            case OPT_CHARACTER_REGION:
            case OPT_OUTPUT_FILENAME:
            case OPT_DEFAULT_GLYPH:
                if (!*value)
                {
                    if (argIdx + 1 >= argc)
                    {
                        PrintUsage();
                        return 1;
                    }

                    ++argIdx;
                    value = argv[argIdx];
                }
                break;
            }

            switch (option)
            {
            case OPT_TYPEFACE:
                typeFace = value;
                break;

            case OPT_HEIGHT:
                if (swscanf_s(value, L"%lu", &fontHeight) != 1)
                {
                    wprintf(L"Invalid value specified with -h (%ls)\n", value);
                    wprintf(L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_WEIGHT:
                if (!ParseTableValue(option, value, g_Weights, fontWeight))
                {
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_ITALIC:
                useItalic = true;
                break;

            case OPT_UNDERLINE:
                useUnderline = true;
                break;

            case OPT_STRIKEOUT:
                useStrikeOut = true;
                break;

            case OPT_CHARSET:
                if (!ParseTableValue(option, value, g_Charsets, charset))
                {
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_QUALITY:
                if (!ParseTableValue(option, value, g_Qualities, quality))
                {
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_PITCH:
                if (!ParseTableValue(option, value, g_Pitches, pitch))
                {
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_FAMILY:
                if (!ParseTableValue(option, value, g_Families, family))
                {
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_GGO_DEPTH:
                 if (!ParseTableValue(option, value, g_GGO_Depths, ggoDepth))
                {
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_CHARACTER_REGION:
                if (!ParseCharacterRegion(value, regions))
                {
                    wprintf(L"Invalid character region value specified with -%ls: (%ls)\n", LookupByValue(OPT_CHARACTER_REGION, g_Options), value);
                    wprintf(L"\n");
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_OUTPUT_FILENAME:
                outputFilename = value;
                break;

            case OPT_OVERWRITE:
                overwriteOutputFile = true;
                break;

            case OPT_DEFAULT_GLYPH:
                if (!ParseDefaultGlyph(value, defaultGlyph, regions))
                {
                    wprintf(L"Invalid character code specified with -%ls: (%ls)\n", LookupByValue(OPT_DEFAULT_GLYPH, g_Options), value);
                    wprintf(L"\n");
                    PrintUsage();
                    return 1;
                }
            }
        }
    }

    if (fontHeight == 0)
    {
        wprintf(L"No value for required option -%ls\n", LookupByValue(OPT_HEIGHT, g_Options));
        PrintUsage();
        return 1;
    }

    if (outputFilename == nullptr)
    {
        wprintf(L"No value for required options -%ls\n", LookupByValue(OPT_OUTPUT_FILENAME, g_Options));
        PrintUsage();
        return 1;
    }

    if (!overwriteOutputFile)
    {
        if (GetFileAttributesW(outputFilename) != INVALID_FILE_ATTRIBUTES)
        {
            wprintf(L"\nERROR: Output file already exists, use -%ls to overwrite.\n", LookupByValue(OPT_OVERWRITE, g_Options));
            PrintUsage();
            return 1;
        }
    }
    
    try
    {
        LOGFONT logfont = {};
        logfont.lfHeight = fontHeight;
        logfont.lfWidth = 0;
        logfont.lfEscapement = 0;
        logfont.lfOrientation = 0;
        logfont.lfWeight = fontWeight;
        logfont.lfItalic = useItalic;
        logfont.lfUnderline = useUnderline;
        logfont.lfStrikeOut = useStrikeOut;
        logfont.lfCharSet = charset;
        logfont.lfOutPrecision = OUT_TT_ONLY_PRECIS;
        logfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        logfont.lfQuality = quality;
        logfont.lfPitchAndFamily = uint8_t(pitch) | uint8_t(family);
        logfont.lfFaceName[LF_FACESIZE];

        if(typeFace)
            wcscpy_s(logfont.lfFaceName, typeFace);

        // Create the glyphs
        std::vector<GGO_Glyph> ggoGlyphs;
        std::vector<KERNINGPAIR> kerns;
        Create_GGO_Glyphs(
            &logfont,
            ggoDepth,
            regions,
            kerns,
            ggoGlyphs,
            defaultGlyph);

        // Create the pixel buffer
        size_t pixelBufferSize = 0;
        std::unique_ptr<uint8_t[]> pixels;
        {
            pixelBufferSize = CreatePixelBuffer(ggoGlyphs, nullptr, 0);
            pixels = std::make_unique<uint8_t[]>(pixelBufferSize);
            CreatePixelBuffer(ggoGlyphs, pixels.get(), pixelBufferSize);
        }

        // Compute the ascent and descent for the font
        unsigned effectiveAscent = 0;
        unsigned effectiveDescent = 0;
        ComputeAscentDescent(ggoGlyphs, effectiveAscent, effectiveDescent);

        // Create the raster glyphs
        std::vector<RasterGlyphSheet::RasterGlyph> rasterGlyphs;
        rasterGlyphs.resize(ggoGlyphs.size());
        uint32_t pixelIndex = 0;
        for (int i = 0; i < ggoGlyphs.size(); ++i)
        {
            rasterGlyphs[i].character = ggoGlyphs[i].GetCharacterCode();

            auto& metrics = ggoGlyphs[i].GetMetrics();
            rasterGlyphs[i].blackBoxOriginX = int16_t(metrics.gmptGlyphOrigin.x);
            rasterGlyphs[i].blackBoxOriginY = int16_t(metrics.gmptGlyphOrigin.y);
            rasterGlyphs[i].blackBoxWidth = metrics.gmBlackBoxX;
            rasterGlyphs[i].blackBoxHeight = metrics.gmBlackBoxY;
            rasterGlyphs[i].cellIncX = metrics.gmCellIncX;
            rasterGlyphs[i].cellIncY = metrics.gmCellIncY;
            rasterGlyphs[i].pixelIndex = pixelIndex;
            

            pixelIndex += GetStorageSize<uint8_t>(metrics.gmBlackBoxX * metrics.gmBlackBoxY);
        }

        // Create the kerning pairs
        std::vector<RasterGlyphSheet::KerningPair> rasterKerns;
        rasterKerns.resize(kerns.size());
        for (int i = 0; i < kerns.size(); ++i)
        {
            rasterKerns[i].first = kerns[i].wFirst;
            rasterKerns[i].second = kerns[i].wSecond;
            rasterKerns[i].amount = kerns[i].iKernAmount;
        }

        // Write the RasterFont to the file
        {
            // Create the RasterFont
            RasterFont rf(std::make_unique<RasterGlyphSheet>(
                effectiveAscent,
                effectiveDescent,
                rasterGlyphs,
                rasterKerns,
                uint32_t(pixelBufferSize),
                pixels.get(),
                '\0'));
            
            rf.WriteToFile(outputFilename);
        }
    }
    catch (std::exception &exn)
    {
        printf("Encountered an exception when trying to generate the font: \n%s", exn.what());
        return 1;
    }

    return 0;
}

