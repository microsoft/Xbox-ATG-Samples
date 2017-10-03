//--------------------------------------------------------------------------------------
// BufferDescriptor.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "BufferDescriptor.h"

using namespace ATG;


void ATG::SetPixel(const BufferDesc & destBuffer, unsigned col, unsigned row, uint8_t clr)
{
    if (col >= destBuffer.width || row >= destBuffer.height)
        return;

    unsigned index = row * (destBuffer.width) + col;
    if (index < destBuffer.size)
    {
        destBuffer.data[index] = clr;
    }
}