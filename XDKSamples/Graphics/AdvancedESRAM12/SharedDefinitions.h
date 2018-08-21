//--------------------------------------------------------------------------------------
// SharedDefinitions.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

namespace
{
    // Conversion functions
    template <typename T> inline constexpr T Kibibytes(T val) { return val * 1024; }
    template <typename T> inline constexpr T Mebibytes(T val) { return Kibibytes(val * 1024); }
    template <typename T> inline constexpr T Gibibytes(T val) { return Mebibytes(val * 1024); }


    // Common Constants
    constexpr int c_pageSizeBytes = Kibibytes(64);                          // Use MEM_LARGE_PAGES; 64 KiB per page.

                                                                            // ESRAM Constants
    constexpr int c_esramSizeBytes = Mebibytes(32);                         // 32 MiB
    constexpr int c_esramPageCount = c_esramSizeBytes / c_pageSizeBytes;

    // DRAM Constants
    constexpr int c_dramBlockPageCount = 64;                                // 64 pages per DRAM block; somewhat arbitrarily chosen.
    constexpr int c_dramBlockSize = c_dramBlockPageCount * c_pageSizeBytes; // 4 MiB per DRAM block

    template <typename T>
    inline constexpr T DivRoundUp(T num, T denom)
    {
        return (num + denom - 1) / denom;
    }

    template <typename T>
    constexpr T PageCount(T byteSize)
    {
        return DivRoundUp(byteSize, T(c_pageSize));
    }
}

namespace ATG
{
    struct VMemDeleter
    {
        void operator()(void* mem) { VirtualFree(mem, 0, MEM_RELEASE); }
    };

    // Small helper type to perform virtual memory cleanup on destruction.
    using VirtualMemPtr = std::unique_ptr<void, VMemDeleter>;
}
