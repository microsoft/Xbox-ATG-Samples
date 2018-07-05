//--------------------------------------------------------------------------------------
// SystemInfo.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SystemInfo.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    inline float XM_CALLCONV DrawStringCenter(SpriteBatch* batch, SpriteFont* font, const wchar_t* text, float mid, float y, FXMVECTOR color, float scale)
    {
        XMVECTOR size = font->MeasureString(text);
        XMFLOAT2 pos(mid - XMVectorGetX(size)*scale / 2.f, y);
        font->DrawString(batch, text, pos, color, 0.f, Vector2::Zero, scale);
        return font->GetLineSpacing() * scale;
    }

    inline void DrawStringLeft(SpriteBatch* batch, SpriteFont* font, const wchar_t* text, float mid, float y, float scale)
    {
        XMVECTOR size = font->MeasureString(text);
        XMFLOAT2 pos(mid - XMVectorGetX(size)*scale, y);
        font->DrawString(batch, text, pos, ATG::Colors::Blue, 0.f, Vector2::Zero, scale);
    }

    inline float DrawStringRight(SpriteBatch* batch, SpriteFont* font, const wchar_t* text, float mid, float y, float scale)
    {
        XMFLOAT2 pos(mid, y);
        font->DrawString(batch, text, pos, ATG::Colors::White, 0.f, Vector2::Zero, scale);
        return font->GetLineSpacing()*scale;
    }
}

Sample::Sample() :
    m_frame(0),
    m_scale(1.25f),
    m_current(0)
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
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

    if (m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED
        || m_gamePadButtons.dpadRight == GamePad::ButtonStateTracker::PRESSED)
    {
        ++m_current;
        if (m_current >= int(InfoPage::MAX))
            m_current = 0;
    }

    if (m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED
        || m_gamePadButtons.dpadLeft == GamePad::ButtonStateTracker::PRESSED)
    {
        --m_current;
        if (m_current < 0)
            m_current = int(InfoPage::MAX) - 1;
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

    auto fullscreen = m_deviceResources->GetOutputSize();

    auto safeRect = Viewport::ComputeTitleSafeArea(fullscreen.right - fullscreen.left, fullscreen.bottom - fullscreen.top);

    float mid = float(safeRect.left) + float(safeRect.right - safeRect.left) / 2.f;

    m_batch->Begin();
    m_batch->Draw(m_background.Get(), fullscreen);

    float y = float(safeRect.top);

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.bottom) - m_smallFont->GetLineSpacing());

    DX::DrawControllerString(m_batch.get(), m_smallFont.get(), m_ctrlFont.get(), L"Use [A], [B], or [DPad] to cycle pages", pos, ATG::Colors::LightGrey, m_scale);

    float spacer = XMVectorGetX(m_smallFont->MeasureString(L"X")*m_scale);

    float left = mid - spacer;
    float right = mid + spacer;

    switch (static_cast<InfoPage>(m_current))
    {
        case InfoPage::SYSTEMINFO:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"GetNativeSystemInfo", mid, y, ATG::Colors::LightGrey, m_scale);

            SYSTEM_INFO info = {};
            GetNativeSystemInfo(&info);

            wchar_t buff[128] = {};
            swprintf_s(buff, L"%zX", info.dwActiveProcessorMask);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"dwActiveProcessorMask", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            swprintf_s(buff, L"%u", info.dwNumberOfProcessors);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"dwNumberOfProcessors", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            if (info.dwNumberOfProcessors > 6)
            {
                y += m_smallFont->GetLineSpacing();

                DrawStringCenter(m_batch.get(), m_smallFont.get(), L"7th Core enabled", mid, y, ATG::Colors::Orange, m_scale);
            }
        }
        break;

    case InfoPage::GLOBALMEMORYSTATUS:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"GlobalMemoryStatusEx", mid, y, ATG::Colors::LightGrey, m_scale);

            MEMORYSTATUSEX info = {};
            info.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&info))
            {
                auto tphys = static_cast<uint32_t>(info.ullTotalPhys / (1024 * 1024));
                auto aphys = static_cast<uint32_t>(info.ullAvailPhys / (1024 * 1024));
                auto tvirt = static_cast<uint32_t>(info.ullTotalVirtual / (1024 * 1024));
                auto avirt = static_cast<uint32_t>(info.ullAvailVirtual / (1024 * 1024));

                wchar_t buff[128] = {};
                swprintf_s(buff, L"%u / %u (MB)", aphys, tphys);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Physical Memory", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u (MB)", tvirt);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Total Virtual Memory", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u (MB)", avirt);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Available VM", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
            }
        }
        break;

    case InfoPage::ANALYTICSINFO:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"AnalyticsInfo", mid, y, ATG::Colors::LightGrey, m_scale);

#if _XDK_VER < 0x3F6803F3 /* XDK Edition 170600 */
            auto deviceForm = Windows::System::Profile::AnalyticsInfo::DeviceForm;

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DeviceForm", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), deviceForm->Data(), right, y, m_scale);
#endif

            auto versionInfo = Windows::System::Profile::AnalyticsInfo::VersionInfo;

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DeviceFamily", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), versionInfo->DeviceFamily->Data(), right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DeviceFamilyVersion (Title OS)", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), versionInfo->DeviceFamilyVersion->Data(), right, y, m_scale);

            // For real-world use just log it as an opaque string, and do the decode in the reader instead
            LARGE_INTEGER li;
            li.QuadPart = _wtoi64(versionInfo->DeviceFamilyVersion->Data());

            wchar_t buff[16] = {};
            swprintf_s(buff, L"%u.%u.%u.%u", HIWORD(li.HighPart), LOWORD(li.HighPart), HIWORD(li.LowPart), LOWORD(li.LowPart));
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            // For logging the Host OS/recovery version use this Xbox One XDK specific API
            y += m_smallFont->GetLineSpacing() * 2;

            SYSTEMOSVERSIONINFO systemOSver = {};
            GetSystemOSVersion(&systemOSver);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"GetSystemOSVersion (Host OS)", left, y, m_scale);

            swprintf_s(buff, L"%u.%u.%u.%u", systemOSver.MajorVersion, systemOSver.MinorVersion, systemOSver.BuildNumber, systemOSver.Revision);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            // The _XDK_VER captures at compile-time the version of the Xbox One XDK used to build the application
            y += m_smallFont->GetLineSpacing() * 2;

            swprintf_s(buff, L"%08X (%ls)", _XDK_VER, _XDK_VER_STRING_COMPACT_W);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"_XDK_VER", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            swprintf_s(buff, L"%u.%u", HIWORD(_XDK_VER), LOWORD(_XDK_VER));
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            if (li.LowPart != _XDK_VER)
            {
                y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"Title OS vs. Xbox One XDK mismatch", mid, y, ATG::Colors::Orange, m_scale);
            }
        }
        break;

    case InfoPage::DIRECT3D:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D", mid, y, ATG::Colors::LightGrey, m_scale);

            auto device = m_deviceResources->GetD3DDevice();

            D3D11X_GPU_HARDWARE_CONFIGURATION hwConfig = {};
            device->GetGpuHardwareConfiguration(&hwConfig);

            wchar_t buff[128] = {};
            swprintf_s(buff, L"%I64u", hwConfig.GpuFrequency);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"GPU Frequency", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

#if _XDK_VER >= 0x38390868 /* XDK Edition 161000 */
            const wchar_t* hwver = L"Unknown";
            switch (hwConfig.HardwareVersion)
            {
            case D3D11X_HARDWARE_VERSION_XBOX_ONE: hwver = L"Xbox One"; break;
            case D3D11X_HARDWARE_VERSION_XBOX_ONE_S: hwver = L"Xbox One S"; break;
#if _XDK_VER >= 0x3F6803F3 /* XDK Edition 170600 */
            case D3D11X_HARDWARE_VERSION_XBOX_ONE_X: hwver = L"Xbox One X"; break;
            case D3D11X_HARDWARE_VERSION_XBOX_ONE_X_DEVKIT: hwver = L"Xbox One X (DevKit)"; break;
#endif
            }

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Hardware Version", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), hwver, right, y, m_scale);
#endif

#if _XDK_VER >= 0x3F6803F3 /* XDK Edition 170600 */
            y += m_smallFont->GetLineSpacing() * 2;

            const wchar_t* conType = L"Unknown";
            switch (GetConsoleType())
            {
            case CONSOLE_TYPE_XBOX_ONE: conType = L"Xbox One"; break;
            case CONSOLE_TYPE_XBOX_ONE_S: conType = L"Xbox One S"; break;
            case CONSOLE_TYPE_XBOX_ONE_X: conType = L"Xbox One X"; break;
            case CONSOLE_TYPE_XBOX_ONE_X_DEVKIT: conType = L"Xbox One X (DevKit)"; break;
            }

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Console Type", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), conType, right, y, m_scale);
#endif
        }
        break;
    }

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

    m_smallFont = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_largeFont = std::make_unique<SpriteFont>(device, L"SegoeUI_36.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"ATGSampleBackground.DDS", nullptr, m_background.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto vp = m_deviceResources->GetScreenViewport();
    m_batch->SetViewport(vp);
}
#pragma endregion
