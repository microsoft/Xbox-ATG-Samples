//--------------------------------------------------------------------------------------
// CommandLineHelpers.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"

#include "CommandLineHelpers.h"
#include <algorithm>

using namespace ATG;

const wchar_t *ATG::ConsumeDigits(const wchar_t *value)
{
    if (value == nullptr) return nullptr;
    const wchar_t *result = value;

    while (iswxdigit(*result))
    {
        ++result;
    }

    return result;
}

#if defined(_XBOX_ONE) && defined(_TITLE)
ATG::WCRANGE ATG::MakeRange(wchar_t c0, wchar_t c1)
#else
WCRANGE ATG::MakeRange(wchar_t c0, wchar_t c1)
#endif
{
    uint32_t u0 = std::min(c0, c1);
    uint32_t u1 = std::max(c0, c1);
    uint32_t count = u1 - u0 + 1;

    WCRANGE result = { c0, wchar_t(std::min(count, uint32_t(0xFFFF))) };

    return result;
}

bool ATG::ParseCharacterRegion(const wchar_t *rangeSpec, std::vector<WCRANGE> &regions)
{
    {
        const wchar_t *charRange = rangeSpec;

        if (!*charRange) return false;

        wchar_t c0 = *charRange++;
        if (!*charRange)
        {
            // Only one character specified -- but that's OK
            WCRANGE range = { c0, 1 };
            regions.push_back(range);
            return true;
        }

        if (*charRange++ == '-')
        {
            wchar_t c1 = *charRange++;
            if (!c1)
            {
                wprintf(L"Expected another character after '-' when parsing range\n");
                return false;
            }

            if (*charRange)
            {
                wprintf(L"Unexpected character (%c) after parsing range\n", *charRange);
                return false;
            }

            regions.push_back(MakeRange(c0, c1));
            return true;
        }
    }

    {
        const wchar_t *hexRange = rangeSpec;

        // Match and consume the hex prefix '0x'
        if (_wcsnicmp(L"0x", hexRange, 2) != 0)
        {
            wprintf(L"Expected a hexidecimal range specification\n");
            return false;
        }
        hexRange += 2;

        unsigned c0 = 0;
        // Scan the hex value
        if (swscanf_s(hexRange, L"%lx", &c0) != 1)
        {
            wprintf(L"Expected a hexidecimal value following the prefix ('0x')\n");
            return false;
        }
        hexRange = ConsumeDigits(hexRange);

        if (c0 > 0xFFFF)
        {
            wprintf(L"The first hexidecimal value in the range specification is too large (0x%X)\n", c0);
            return false;
        }

        if (!*hexRange)
        {
            // Only one hex value specified -- but that's OK
            WCRANGE range = { wchar_t(c0), 1 };
            regions.push_back(range);
            return true;
        }

        if (*hexRange++ == '-')
        {
            if (!*hexRange)
            {
                wprintf(L"Expected another hexidecimal value after '-' when parsing range\n");
                return false;
            }

            // Match and consume the hex prefix '0x'
            if (_wcsnicmp(L"0x", hexRange, 2) != 0)
            {
                wprintf(L"Second part of the hexidecimal range specification doesn't doesn't start with the '0x' hexidecimal prefix (%s)\n", hexRange);
                return false;
            }
            hexRange += 2;

            unsigned c1 = 0;
            // Scan the hex value
            if (swscanf_s(hexRange, L"%lx", &c1) != 1)
            {
                wprintf(L"Expected a hexidecimal value following the prefix ('0x')\n");
                return false;
            }
            hexRange = ConsumeDigits(hexRange);

            if (c1 > 0xFFFF)
            {
                wprintf(L"The second hexidecimal value in the range specification is too large (0x%X)\n", c1);
                return false;
            }

            if (*hexRange)
            {
                wprintf(L"Unexpected character (%c) after parsing the second value in the range specification\n", *hexRange);
                return false;
            }

            regions.push_back(MakeRange(wchar_t(c0), wchar_t(c1)));
            return true;

        }
        else
        {
            wprintf(L"Expecting one or two hexidecimal values (4 digits or less) separated by a dash\n");
            return false;
        }
    }
}
