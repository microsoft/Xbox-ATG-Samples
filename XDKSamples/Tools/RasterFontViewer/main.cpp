//--------------------------------------------------------------------------------------
// main.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"

#include <conio.h>
#include <fstream>

#include "FrontPanel\RasterFont.h"

#include "CommandLineHelpers.h"
#include "TestWindow.h"
#include "Visualization.h"

using namespace ATG;

namespace
{
    enum OPTIONS : uint64_t
    {
        OPT_CHARACTER_REGION = 1,
        OPT_MAX
    };

    static_assert(OPT_MAX <= 64, "Options must fit into a uint64_t bitfield");


    const SValue g_Options[] =
    {
        { L"cr", OPT_CHARACTER_REGION },
    };

    void PrintLogo()
    {
        wprintf(L"Microsoft (R) Raster Font Viewer for for Xbox One\n");
        wprintf(L"Copyright (C) Microsoft Corp. All rights reserved.\n");
#ifdef _DEBUG
        wprintf(L"*** Debug build ***\n");
#endif
        wprintf(L"\n");
    }

    void PrintUsage()
    {
        PrintLogo();

        wprintf(L"Usage: rasterfontgen [options] <filename>\n\n");
        wprintf(L"   -cr:<range>         Character region. Specifies a range of unicode code points to include in the output.\n");
        wprintf(L"                       Examples: -cr:a-z -cr:0x1200-0x1250 -cr:0x1234\n");
    }
} // ANONYMOUS namespace

int wmain(int argc, wchar_t *argv[])
{
    // Parameters and defaults
    const wchar_t       *outputFilename = nullptr;
    std::vector<WCRANGE> regions;

    unsigned individualOptions = 0;

    for (int argIdx = 1; argIdx < argc; ++argIdx)
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
            if (!LookupByName(arg, g_Options, option) || (individualOptions & (1 << option)))
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
            case OPT_CHARACTER_REGION:
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
            case OPT_CHARACTER_REGION:
                if (!ParseCharacterRegion(value, regions))
                {
                    wprintf(L"Invalid character region value specified with -%ls: (%ls)\n", LookupByValue(OPT_CHARACTER_REGION, g_Options), value);
                    wprintf(L"\n");
                    PrintUsage();
                    return 1;
                }
                break;
            }

        }
        else if(outputFilename)
        {
            wprintf(L"Output file already specified: %ls.\n(Found something else: %ls\n", outputFilename, arg);
            PrintUsage();
            return 1;
        }
        else
        {
            outputFilename = arg;
        }
    }

    if (!outputFilename)
    {
        PrintUsage();
        return 1;
    }

    try
    {
        RasterFont rf(outputFilename);

        HBITMAP fontBitmap = DrawRasterGlyphSheet(rf.GetGlyphs(), regions);

        TestWindow([&fontBitmap](HDC hDC)
        {
            BITMAP bmData = {};
            GetObject(fontBitmap, sizeof(BITMAP), &bmData);

            HDC memDC = CreateCompatibleDC(hDC);
            SelectObject(memDC, fontBitmap);

            BitBlt(hDC, 0, 0, bmData.bmWidth, bmData.bmHeight, memDC, 0, 0, SRCCOPY);

            DeleteObject(memDC);
        });

        DeleteObject(fontBitmap);

    }
    catch (std::exception &exn)
    {
        printf("Encountered an excption when trying to load the font: \n%s", exn.what());
    }

    return 0;
}