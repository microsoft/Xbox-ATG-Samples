//--------------------------------------------------------------------------------------
// SimplePLM.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimplePLM.h"

#include "ATGColors.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_useDeferral(false),
    m_consoleIsValid(false),
    m_frame(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    m_console = std::make_unique<DX::TextConsoleImage>();
}

void Sample::ShowInstructions()
{
    m_logCache.insert(m_logCache.begin(), ref new Platform::String(L"Simple PLM"));
    m_logCache.insert(m_logCache.begin(), ref new Platform::String(L"Launch Settings with A button"));
    m_logCache.insert(m_logCache.begin(), ref new Platform::String(L"Toggle suspend deferral with B button (default is off)"));
    m_logCache.insert(m_logCache.begin(), ref new Platform::String(L"Show AccountPicker with X button"));
    m_logCache.insert(m_logCache.begin(), ref new Platform::String(L"Perform a RestartAplicationOnly with Y button"));
}

void Sample::LogPLMEvent(const wchar_t* primaryLog, const wchar_t* secondaryData)
{
    unsigned int tid = GetCurrentThreadId();
    SYSTEMTIME curTime;
    GetSystemTime(&curTime);

    wchar_t timeAndTid[25];
    swprintf_s(timeAndTid, L"[%02d:%02d:%02d:%03d](%d)", curTime.wHour, curTime.wMinute, curTime.wSecond, curTime.wMilliseconds, tid);
    Platform::String^ LogLine = ref new Platform::String(timeAndTid) + " " + ref new Platform::String(primaryLog) + " " + ref new Platform::String(secondaryData);

    //Output to Debug Console.
    OutputDebugStringW(LogLine->Data());
    OutputDebugStringW(L"\n");

    //Output to screen. We must cache screen logs if a log occurs when there is no valid screen console yet.
    if (!m_consoleIsValid)
    {
        m_logCache.insert(m_logCache.begin(), LogLine);
    }
    else
    {
        m_console->WriteLine(LogLine->Data());
    }
}

void Sample::ToggleDeferral()
{
    m_useDeferral = !m_useDeferral;
    if (m_useDeferral)
    {
        LogPLMEvent(L"Will use a suspend deferral.");
    }
    else
    {
        LogPLMEvent(L"Will not use a suspend deferral.");
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
        if (m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED)
        {
            ToggleDeferral();
        }
        if (m_gamePadButtons.y == GamePad::ButtonStateTracker::PRESSED)
        {
            LogPLMEvent(L"Performing a Restart");
            Windows::ApplicationModel::Core::CoreApplication::RestartApplicationOnly("Restart", nullptr);
        }
        if (m_gamePadButtons.x == GamePad::ButtonStateTracker::PRESSED)
        {
            LogPLMEvent(L"Showing AccountPicker");
            Windows::Xbox::UI::SystemUI::ShowAccountPickerAsync(nullptr, Windows::Xbox::UI::AccountPickerOptions::None);
        }
        if (m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED)
        {
            LogPLMEvent(L"Launching into Settings");
            Windows::System::Launcher::LaunchUriAsync(ref new Windows::Foundation::Uri(L"settings:"));
        }
    }
    else
    {
        m_gamePadButtons.Reset();
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

    m_console->Render();

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

    m_console->RestoreDevice(m_deviceResources->GetD3DDeviceContext(), L"Courier_16.spritefont", L"ATGSampleBackground.DDS");
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    m_console->SetWindow(m_deviceResources->GetOutputSize(), true);
    //Now that the Console is valid we can flush any logs to the console.
    m_consoleIsValid = true;
    while (!m_logCache.empty())
    {
        m_console->WriteLine(m_logCache.back()->Data());
        m_logCache.pop_back();
    }
}
#pragma endregion
