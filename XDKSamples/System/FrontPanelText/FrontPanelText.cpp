//--------------------------------------------------------------------------------------
// FrontPanelText.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "FrontPanelText.h"

#include "ATGColors.h"

namespace
{
    const wchar_t *g_fontNames[] = {
        L"Courier",
        L"LucidaConsole",
        L"SegoeUI",
        L"ArialBlack",
        L"ArialBold",
        L"Consolas",
        L"MSSansSerif",
        L"Xbox"
    };

    unsigned FindIndexForName(const wchar_t *name)
    {
        unsigned i = 0;
        for (; i < _countof(g_fontNames); ++i)
        {
            if (wcscmp(name, g_fontNames[i]) == 0)
                return i;
        }
        return i;
    }

    const unsigned g_fontSizes[] = { 12, 16, 18, 24, 32, 64 };

    bool FileExists(const wchar_t *fileName)
    {
        uint32_t attrs = ::GetFileAttributesW(fileName);

        return (attrs != INVALID_FILE_ATTRIBUTES
            && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
    }
} // ANONYMOUS namespace

extern void ExitSample();

using namespace DirectX;
using namespace ATG;
using ButtonState = FrontPanelInput::ButtonStateTracker::ButtonState;
using Microsoft::WRL::ComPtr;


Sample::Sample()
    : m_frame(0)
    , m_currentEntry(0)
    , m_dirty(true)
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    
    if (IsXboxFrontPanelAvailable())
    {
        // Get the default front panel
        DX::ThrowIfFailed(GetDefaultXboxFrontPanel(m_frontPanelControl.ReleaseAndGetAddressOf()));

        // Initialize the FrontPanelDisplay object
        m_frontPanelDisplay = std::make_unique<FrontPanelDisplay>(m_frontPanelControl.Get());

        // Intiailize the FrontPanelInput object
        m_frontPanelInput = std::make_unique<FrontPanelInput>(m_frontPanelControl.Get());
    }
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_gamePad = std::make_unique<GamePad>();

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // Create all the fonts
    {
        wchar_t buf[MAX_PATH];
        wchar_t fontName[64];

        for (int f = 0; f < _countof(g_fontNames); ++f)
        {
            for (int s = 0; s < _countof(g_fontSizes); ++s)
            {
                swprintf_s(fontName, L"%s%i", g_fontNames[f], g_fontSizes[s]);
                swprintf_s(buf, L"Assets\\%s.rasterfont", fontName);

                if (FileExists(buf))
                {
                    size_t size = m_fontEntries.size();
                    m_fontEntries.emplace_back();
                    auto& entry = m_fontEntries[size];
                    entry.size = g_fontSizes[s];
                    entry.name = g_fontNames[f];
                    entry.font = RasterFont(buf);
                }
            }
        }
    }
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %I64u", m_frame);

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
    m_frame++;
}

// Updates the world.
void Sample::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    if (m_frontPanelControl)
    {
        auto fpInput = m_frontPanelInput->GetState();
        m_frontPanelInputButtons.Update(fpInput);

        // Change the selected font based on input from the dpad
        {

            if (m_frontPanelInputButtons.dpadUp == ButtonState::PRESSED)
            {
                auto& entry = m_fontEntries[m_currentEntry];
                unsigned idx = FindFontEntry(entry.name, entry.size + 1);
                if (idx < m_fontEntries.size())
                    m_currentEntry = idx;
                m_dirty = true;
            }

            if (m_frontPanelInputButtons.dpadDown == ButtonState::PRESSED)
            {
                auto& entry = m_fontEntries[m_currentEntry];
                unsigned idx = FindFontEntry(entry.name, entry.size - 1, false);
                if (idx < m_fontEntries.size())
                    m_currentEntry = idx;
                m_dirty = true;
            }

            if (m_frontPanelInputButtons.dpadRight == ButtonState::PRESSED)
            {
                auto& entry = m_fontEntries[m_currentEntry];
                unsigned i = (FindIndexForName(entry.name) + 1) % _countof(g_fontNames);
                unsigned idx = FindFontEntry(g_fontNames[i], entry.size);
                if (idx < m_fontEntries.size())
                    m_currentEntry = idx;
                m_dirty = true;
            }

            if (m_frontPanelInputButtons.dpadLeft == ButtonState::PRESSED)
            {
                auto& entry = m_fontEntries[m_currentEntry];
                unsigned i = (FindIndexForName(entry.name) + _countof(g_fontNames) - 1) % _countof(g_fontNames);
                unsigned idx = FindFontEntry(g_fontNames[i], entry.size);
                if (idx < m_fontEntries.size())
                    m_currentEntry = idx;
                m_dirty = true;
            }

            if (m_frontPanelInputButtons.buttonSelect == ButtonState::PRESSED)
            {
                m_frontPanelDisplay->SaveDDSToFile(L"D:\\FrontPanelScreen.dds");
            }
        }

        if (m_dirty)
        {
            // Clear the front panel display
            m_frontPanelDisplay->Clear();

            // Render text to the front panel display
            BufferDesc fpDesc = m_frontPanelDisplay->GetBufferDescriptor();

            auto &entry = m_fontEntries[m_currentEntry];

            if (wcscmp(entry.name, L"Xbox") == 0)
            {
                unsigned idx = FindFontEntry(L"LucidaConsole", 12);
                auto &lcEntry = m_fontEntries[idx];
                RECT r = lcEntry.font.MeasureStringFmt(L"%s%i", entry.name, entry.size);
                lcEntry.font.DrawStringFmt(fpDesc, 0, 0, L"%s%i", entry.name, entry.size);
                const wchar_t symbols[] = { 0xE3E3, 0xE334, 0xE37C, 0xE386, 0xE3AB, 0xE3AC, 0xE3AD, 0xE3AE, 0x0000 };
                entry.font.DrawString(fpDesc, r.right - r.left + 1, 0, symbols);
            }
            else
            {
                entry.font.DrawStringFmt(fpDesc, 0, 0,
                    L"%s%i\n0123456789\n"
                    L"abcdefghijklmnopqrstuvwxyz\n"
                    L"ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
                    L"The quick brown fox jumped over the lazy dog",
                    entry.name, entry.size);
            }

            // Present the front pannel buffer
            m_frontPanelDisplay->Present();

            m_dirty = false;
        }
    }

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Sample::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    auto output = m_deviceResources->GetOutputSize();

    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(output.right, output.bottom);

    m_batch->Begin();
    m_batch->Draw(m_background.Get(), output);
    m_batch->End();

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(context);
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto renderTarget = m_deviceResources->GetRenderTargetView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);

    context->OMSetRenderTargets(1, &renderTarget, nullptr);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Suspend(0);
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    auto context = m_deviceResources->GetD3DDeviceContext();
    m_batch = std::make_unique<SpriteBatch>(context);

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device,
            IsXboxFrontPanelAvailable() ? L"FrontPanelPresent.png" : L"NoFrontPanel.png",
            nullptr, m_background.ReleaseAndGetAddressOf())
    );
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion

unsigned Sample::FindFontEntry(const wchar_t *name, unsigned size, bool larger) const
{
    unsigned i = 0;
    unsigned prevCandidate = unsigned(m_fontEntries.size());

    for (; i < m_fontEntries.size(); ++i)
    {
        const FontEntry &e = m_fontEntries[i];
        if (wcscmp(name, e.name) == 0)
        {
            if (e.size == size)
                return i;
            if (e.size > size)
                return larger ? i : prevCandidate;

            prevCandidate = i;
        }
        else if (prevCandidate < m_fontEntries.size())
            return prevCandidate;
    }
    return prevCandidate;
}
