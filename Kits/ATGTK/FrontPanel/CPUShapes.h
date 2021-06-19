//--------------------------------------------------------------------------------------
// CPUShapes.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <cstdint>


namespace ATG
{

    class CPUShapes
    {
    public:
        CPUShapes(unsigned int bufferWidth, unsigned int bufferHeight, uint8_t *buffer)
            : m_bufferWidth(bufferWidth)
            , m_bufferHeight(bufferHeight)
            , m_buffer(buffer)
        {
        }

        enum class LineOrientation
        {
            Horizontal,
            Vertical,
        };
        
        void RenderRect(int left, int top, int width, int height, uint8_t color = 0xFF, bool filled = true);

        void RenderLine(int x, int y, LineOrientation orientation, int length, uint8_t color = 0xFF);

        void RenderPoint(int x, int y, uint8_t color = 0xFF);

    private:
        unsigned int m_bufferWidth;
        unsigned int m_bufferHeight;
        uint8_t     *m_buffer;
    };
     
} // namespace ATG