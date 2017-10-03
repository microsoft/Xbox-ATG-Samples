//--------------------------------------------------------------------------------------
// SimpleFrontPanel.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleFrontPanel.h"

#include "ATGColors.h"

#include "FileHelpers.h"
#include "OSHelpers.h"

namespace
{
    // For more information, see DirectX Tool Kit's dds.h
    const uint32_t DDS_MAGIC = 0x20534444; // "DDS "

#define DDS_LUMINANCE                   0x00020000  // DDPF_LUMINANCE
#define DDS_HEADER_FLAGS_TEXTURE        0x00001007  // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT 
#define DDS_HEADER_FLAGS_MIPMAP         0x00020000  // DDSD_MIPMAPCOUNT
#define DDS_HEADER_FLAGS_PITCH          0x00000008  // DDSD_PITCH
#define DDS_SURFACE_FLAGS_TEXTURE       0x00001000 // DDSCAPS_TEXTURE

#pragma pack(push, 1)
    struct DDS_PIXELFORMAT
    {
        uint32_t    size;
        uint32_t    flags;
        uint32_t    fourCC;
        uint32_t    RGBBitCount;
        uint32_t    RBitMask;
        uint32_t    GBitMask;
        uint32_t    BBitMask;
        uint32_t    ABitMask;
    };

    struct DDS_HEADER
    {
        uint32_t        size;
        uint32_t        flags;
        uint32_t        height;
        uint32_t        width;
        uint32_t        pitchOrLinearSize;
        uint32_t        depth; // only if DDS_HEADER_FLAGS_VOLUME is set in flags
        uint32_t        mipMapCount;
        uint32_t        reserved1[11];
        DDS_PIXELFORMAT ddspf;
        uint32_t        caps;
        uint32_t        caps2;
        uint32_t        caps3;
        uint32_t        caps4;
        uint32_t        reserved2;
    };
#pragma pack(pop)

    const DDS_PIXELFORMAT DDSPF_L8 = { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE, 0,  8, 0xff, 0x00, 0x00, 0x00 };
}

extern void ExitSample();

using namespace ATG;
using namespace DirectX;

using Microsoft::WRL::ComPtr;

Sample::Sample()
    : m_frame(0)
    , m_screenWidth(0)
    , m_screenHeight(0)
    , m_rememberedButtons(XBOX_FRONT_PANEL_BUTTONS_NONE)
    , m_dirty(false)
    , m_checkerboard(false)
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);

    if (IsXboxFrontPanelAvailable())
    {
        // Get the default front panel
        DX::ThrowIfFailed(GetDefaultXboxFrontPanel(m_frontPanelControl.ReleaseAndGetAddressOf()));

        // Get the screen width and height and allocate a panel buffer
        DX::ThrowIfFailed(m_frontPanelControl->GetScreenWidth(&m_screenWidth));
        DX::ThrowIfFailed(m_frontPanelControl->GetScreenHeight(&m_screenHeight));
        m_panelBuffer = std::unique_ptr<uint8_t>(new uint8_t[m_screenWidth * m_screenHeight]);

        // Fill the panel buffer with a checkerboard pattern
        CheckerboardFillPanelBuffer();

        XBOX_FRONT_PANEL_LIGHTS lights = XBOX_FRONT_PANEL_LIGHTS_NONE;
        DX::ThrowIfFailed(m_frontPanelControl->SetLightStates(lights));
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

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    if(m_frontPanelControl)
    {
        XBOX_FRONT_PANEL_BUTTONS previousButtons = m_rememberedButtons;
        DX::ThrowIfFailed(m_frontPanelControl->GetButtonStates(&m_rememberedButtons));
        XBOX_FRONT_PANEL_BUTTONS pressedButtons = XBOX_FRONT_PANEL_BUTTONS((previousButtons ^ m_rememberedButtons) & m_rememberedButtons);

        // Use DPAD left and right to toggle between checkerboard and gradient
        if ((pressedButtons & XBOX_FRONT_PANEL_BUTTONS_LEFT) || (pressedButtons & XBOX_FRONT_PANEL_BUTTONS_RIGHT))
        {
            m_checkerboard = !m_checkerboard;
            if (m_checkerboard)
            {
                CheckerboardFillPanelBuffer();
            }
            else
            {
                GradientFillPanelBuffer();
            }
        }

        if (pressedButtons & XBOX_FRONT_PANEL_BUTTONS_UP)
        {
            BrightenPanelBuffer();
        }

        if (pressedButtons & XBOX_FRONT_PANEL_BUTTONS_DOWN)
        {
            DimPanelBuffer();
        }

        unsigned whichButton = XBOX_FRONT_PANEL_BUTTONS_BUTTON1;

        for (; whichButton <= XBOX_FRONT_PANEL_BUTTONS_BUTTON5; whichButton = whichButton << 1)
        {
            if (pressedButtons & whichButton)
            {
                ToggleButtonLight(XBOX_FRONT_PANEL_BUTTONS(whichButton));
            }
        }

        if (pressedButtons & XBOX_FRONT_PANEL_BUTTONS_SELECT)
        {
            CaptureFrontPanelScreen(L"D:\\FrontPanelScreen.dds");
#ifdef _DEBUG
            OutputDebugStringA("Screenshot of front panel display written to development drive.\n");
#endif
        }

        PresentFrontPanel();
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

void Sample::CheckerboardFillPanelBuffer()
{
    uint8_t *data = m_panelBuffer.get();
    for (unsigned i = 0; i < m_screenWidth; ++i)
    {
        for (unsigned j = 0; j < m_screenHeight; ++j)
        {
            uint8_t color = ((i / 16 & 1) ^ (j / 16 & 1)) * 0xFF;
            data[i + j * m_screenWidth] = color;
        }
    }
    m_dirty = true;
    m_checkerboard = true;
}

void Sample::GradientFillPanelBuffer()
{
    uint8_t *data = m_panelBuffer.get();
    uint8_t colorBand = uint8_t(m_screenWidth / 16);
    for (unsigned i = 0; i < m_screenWidth; ++i)
    {
        for (unsigned j = 0; j < m_screenHeight; ++j)
        {
            uint8_t color = uint8_t((i / colorBand) << 4);
            data[i + j * m_screenWidth] = color;
        }
    }
    m_dirty = true;
    m_checkerboard = false;
}

void Sample::DimPanelBuffer()
{
    uint8_t *data = m_panelBuffer.get();
    for (unsigned i = 0; i < m_screenWidth; ++i)
    {
        for (unsigned j = 0; j < m_screenHeight; ++j)
        {
            uint8_t color = data[i + j * m_screenWidth];
            if (color >= 0x10)
            {
                color -= 0x10;
                data[i + j * m_screenWidth] = color;
            }
        }
    }
    m_dirty = true;
}

void Sample::BrightenPanelBuffer()
{
    uint8_t *data = m_panelBuffer.get();
    for (unsigned i = 0; i < m_screenWidth; ++i)
    {
        for (unsigned j = 0; j < m_screenHeight; ++j)
        {
            uint8_t color = data[i + j * m_screenWidth];
            if (color < 0xF0)
            {
                color += 0x10;
                data[i + j * m_screenWidth] = color;
            }
        }
    }
    m_dirty = true;
}

void Sample::ToggleButtonLight(XBOX_FRONT_PANEL_BUTTONS whichButton)
{
    XBOX_FRONT_PANEL_LIGHTS lights = XBOX_FRONT_PANEL_LIGHTS_NONE;
    DX::ThrowIfFailed(m_frontPanelControl->GetLightStates(&lights));
    lights = XBOX_FRONT_PANEL_LIGHTS(lights ^ (XBOX_FRONT_PANEL_LIGHTS(whichButton)));
    DX::ThrowIfFailed(m_frontPanelControl->SetLightStates(lights));
}

void Sample::PresentFrontPanel()
{
    // It is only necessary to present to the front panel when pixels have changed.
    if (m_dirty)
    {
        DX::ThrowIfFailed(m_frontPanelControl->PresentBuffer(m_screenWidth * m_screenHeight, m_panelBuffer.get()));
        m_dirty = false;
    }
}

void Sample::CaptureFrontPanelScreen(const wchar_t * fileName)
{
    if (!fileName)
    {
        throw std::invalid_argument("Invalid filename");
    }

    // Create file
    ScopedHandle hFile(safe_handle(CreateFile2(fileName, GENERIC_WRITE | DELETE, 0, CREATE_ALWAYS, nullptr)));

    if (!hFile)
    {
        DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    auto_delete_file delonfail(hFile.get());

    // Setup header
    const size_t HEADER_SIZE = sizeof(uint32_t) + sizeof(DDS_HEADER);
    uint8_t fileHeader[HEADER_SIZE] = {};

    *reinterpret_cast<uint32_t*>(&fileHeader[0]) = DDS_MAGIC;

    auto header = reinterpret_cast<DDS_HEADER*>(&fileHeader[0] + sizeof(uint32_t));
    header->size = sizeof(DDS_HEADER);
    header->flags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
    header->height = m_screenHeight;
    header->width = m_screenWidth;
    header->mipMapCount = 1;
    header->caps = DDS_SURFACE_FLAGS_TEXTURE;
    memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_L8, sizeof(DDS_PIXELFORMAT));

    UINT rowPitch = m_screenWidth;
    UINT slicePitch = m_screenWidth * m_screenHeight;
    
    header->flags |= DDS_HEADER_FLAGS_PITCH;
    header->pitchOrLinearSize = static_cast<uint32_t>(rowPitch);

    // Write header & pixels
    DWORD bytesWritten = 0;
    if (!WriteFile(hFile.get(), fileHeader, static_cast<DWORD>(HEADER_SIZE), &bytesWritten, nullptr))
    {
        DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    if (bytesWritten != HEADER_SIZE)
    {
        throw std::exception("WriteFile");
    }

    if (!WriteFile(hFile.get(), m_panelBuffer.get(), static_cast<DWORD>(slicePitch), &bytesWritten, nullptr))
    {
        DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    if (bytesWritten != slicePitch)
    {
        throw std::exception("WriteFile");
    }

    delonfail.clear();
}
