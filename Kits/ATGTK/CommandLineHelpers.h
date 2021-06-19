//--------------------------------------------------------------------------------------
// CommandLineHelpers.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <vector>

namespace ATG
{
    struct SValue
    {
        const wchar_t *name;
        unsigned value;
    };


    template<size_t _SIZE>
    inline bool LookupByName(const wchar_t *name, const SValue(&table)[_SIZE], unsigned &result)
    {
        for (size_t i = 0; i < _SIZE; ++i)
        {
            if (!_wcsicmp(name, table[i].name))
            {
                result = table[i].value;
                return true;
            }
        }
        return false;
    }

    template<size_t _SIZE>
    inline const wchar_t *LookupByValue(unsigned value, const SValue(&table)[_SIZE])
    {
        for (size_t i = 0; i < _SIZE; ++i)
        {
            if (value == table[i].value)
                return table[i].name;
        }

        return L"";
    }

    template<size_t _SIZE>
    inline bool ParseTableValue(unsigned option, const wchar_t *name, const SValue(&table)[_SIZE], unsigned &result)
    {
        const wchar_t pleaseUseMsg[] = L"Please use one of the following: ";

        if (!LookupByName(name, table, result))
        {
            wprintf(L"Invalid value specified with -%ls (%ls)\n", LookupByValue(option, ::g_Options), name);
            wprintf(pleaseUseMsg);
            PrintTable(std::size(pleaseUseMsg) - 1, table);
            wprintf(L"\n\n");
            return false;
        }
        return true;
    }

    template<size_t _SIZE>
    inline void PrintTable(size_t indent, const SValue(&table)[_SIZE])
    {
        size_t idt = indent;
        for (size_t i = 0; i < _SIZE; ++i)
        {
            size_t cchName = wcslen(table[i].name);

            if (idt + cchName + 2 >= 80)
            {
                wprintf(L"\n");
                for (idt = 0; idt < indent; ++idt) wprintf(L" ");
            }

            wprintf(L"%ls ", table[i].name);
            idt += cchName + 2;
        }

        wprintf(L"\n");
    }

    const wchar_t *ConsumeDigits(const wchar_t *value);

#if defined(_XBOX_ONE) && defined(_TITLE)
    typedef struct tagWCRANGE
    {
        WCHAR  wcLow;
        USHORT cGlyphs;
} WCRANGE, *PWCRANGE, FAR *LPWCRANGE;
#endif
    WCRANGE MakeRange(wchar_t c0, wchar_t c1);

    bool ParseCharacterRegion(const wchar_t *rangeSpec, std::vector<WCRANGE> &regions);

} // namespace ATG
