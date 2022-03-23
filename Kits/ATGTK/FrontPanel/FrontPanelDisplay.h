//--------------------------------------------------------------------------------------
// FrontPanelDisplay.h
//
// Microsoft Xbox One XDK
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#if _XDK_VER < 0x3AD703ED /* XDK Edition 170300 */
#error This code requires the March 2017 XDK or later
#endif

#include <memory>

#include <XboxFrontPanel.h>

#include "BufferDescriptor.h"


namespace ATG
{
    class FrontPanelDisplay
    {
    public:

        FrontPanelDisplay(_In_ IXboxFrontPanelControl *frontPanelControl);

        FrontPanelDisplay(FrontPanelDisplay &&) = default;
        FrontPanelDisplay& operator=(FrontPanelDisplay &&) = default;

        FrontPanelDisplay(FrontPanelDisplay const&) = delete;
        FrontPanelDisplay& operator=(FrontPanelDisplay const&) = delete;

        virtual ~FrontPanelDisplay();
        
        void Clear();

        void Present();
        
        // Low-level access to the buffer
        unsigned int GetDisplayWidth() const { return m_displayWidth; }
        unsigned int GetDisplayHeight() const { return m_displayHeight; }

        uint8_t *GetBuffer() const { return m_buffer.get(); }

        // Get a bufer descriptor
        BufferDesc GetBufferDescriptor() const;

        // Capture the screen
        void SaveDDSToFile(_In_z_ const wchar_t *filename) const;
        void SaveWICToFile(_In_z_ const wchar_t *filename, REFGUID guidContainerFormat) const;

        // Loads a buffer from a file
        BufferDesc LoadWICFromFile(_In_z_ const wchar_t* filename, std::unique_ptr<uint8_t[]>& data, unsigned int frameindex = 0);

        // Loads a buffer from a file directly into the display buffer
        BufferDesc LoadWICFromFile(_In_z_ const wchar_t *filename, unsigned int frameIndex = 0);

        // Determine whether the front panel is available
        bool IsAvailable() const { return m_frontPanelControl; }

        // Singleton
        static FrontPanelDisplay& Get();

    private:
        Microsoft::WRL::ComPtr<IXboxFrontPanelControl> m_frontPanelControl;
        unsigned int                                   m_displayWidth;
        unsigned int                                   m_displayHeight;
        std::unique_ptr<uint8_t[]>                     m_buffer;

        static FrontPanelDisplay                      *s_frontPanelDisplayInstance;
    };
}