//--------------------------------------------------------------------------------------
// BufferDescriptor.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <stdint.h>


namespace ATG
{
    struct BufferDesc
    {
        uint8_t *data;
        size_t   size;
        unsigned width;
        unsigned height;
    };

    void SetPixel(const BufferDesc &destBuffer, unsigned col, unsigned row, uint8_t clr);

} // namespace ATG