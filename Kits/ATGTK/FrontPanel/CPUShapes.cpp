//--------------------------------------------------------------------------------------
// CPUShapes.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"
#include "CPUShapes.h"
#include <algorithm>

using namespace ATG;

void CPUShapes::RenderRect(int left, int top, int width, int height, uint8_t color, bool filled)
{
    int right = left + width - 1;
    int bottom = top + height - 1;

    int x_min = std::max(0, left);
    int x_max = std::min(static_cast<int>(m_bufferWidth) - 1, right);
    for (int x = x_min; x <= x_max; x++)
    {
        int y_min = std::max(0, top);
        int y_max = std::min(static_cast<int>(m_bufferHeight) - 1, bottom);
        for (int y = y_min; y <= y_max; y++)
        {
            if (filled || (x == left || x == right || y == top || y == bottom))
            {
                m_buffer[y * m_bufferWidth + x] = color;
            }
        }
    }
}

void CPUShapes::RenderLine(int x, int y, LineOrientation orientation, int length, uint8_t color)
{
    switch (orientation)
    {
    case LineOrientation::Horizontal:
        if (y >= 0 && y < static_cast<int>(m_bufferHeight))
        {
            int x_min = std::max(0, x);
            int x_max = std::min(static_cast<int>(m_bufferWidth) - 1, x + length - 1);
            for (int column = x_min; column <= x_max; column++)
            {
                m_buffer[y * m_bufferWidth + column] = color;
            }
        }
        break;


    case LineOrientation::Vertical:
        if (x >= 0 && x < static_cast<int>(m_bufferWidth))
        {
            int y_min = std::max(0, y);
            int y_max = std::min(static_cast<int>(m_bufferHeight) - 1, y + length - 1);
            for (int row = y_min; row <= y_max; row++)
            {
                m_buffer[row * m_bufferWidth + x] = color;
            }
        }
        break;
    }
}

void CPUShapes::RenderPoint(int x, int y, uint8_t color)
{
    if (x >= 0 && x < static_cast<int>(m_bufferWidth) && y >= 0 && y < static_cast<int>(m_bufferHeight))
    {
        m_buffer[y * m_bufferWidth + x] = color;
    }
}