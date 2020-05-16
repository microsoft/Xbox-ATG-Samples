//--------------------------------------------------------------------------------------
// SimplePLM.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimplePLM.h"

extern void ExitSample() noexcept;

using namespace DirectX;
using namespace Windows::ApplicationModel;
using namespace Windows::UI::Core;
using namespace Windows::ApplicationModel::Activation;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
	m_useDeferral(false),
	m_showToasts(false),
	m_consoleIsValid(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    m_deviceResources->RegisterDeviceNotify(this);
	m_console = std::make_unique<DX::TextConsoleImage>();
}

void Sample::ShowInstructions()
{
    m_logCache.insert(m_logCache.begin(), ref new Platform::String(L"Simple PLM"));
    m_logCache.insert(m_logCache.begin(), ref new Platform::String(L"Toggle Windows notifications with A button or 'N' key (default is off)"));
    m_logCache.insert(m_logCache.begin(), ref new Platform::String(L"Toggle suspend deferral with B button or 'D' key (default is off)"));
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

	//Output to Windows Notification Center.
	if (m_showToasts)
	{
		m_toastManager->Show(primaryLog, secondaryData, timeAndTid);
	}
}

void Sample::ToggleNotifications()
{
	m_showToasts = !m_showToasts;
	if (m_showToasts)
	{
		LogPLMEvent(L"Will log to Windows notifications.");
	}
	else
	{
		LogPLMEvent(L"Will not log to Windows notifications.");
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
void Sample::Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_toastManager = std::make_unique <DX::ToastManager>();

    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();  	
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
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
		if (m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED)
		{
			ToggleNotifications();
		}
		if (m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED)
		{
			ToggleDeferral();
		}
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitSample();
    }

	if (m_keyboardButtons.IsKeyPressed(Keyboard::N))
	{
		ToggleNotifications();
	}

	if (m_keyboardButtons.IsKeyPressed(Keyboard::D))
	{
		ToggleDeferral();
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

    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    m_console->Render();

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent();
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
void Sample::OnActivated()
{
}

void Sample::OnDeactivated()
{
}

void Sample::OnSuspending()
{
	auto context = m_deviceResources->GetD3DDeviceContext();
	context->ClearState();
	m_deviceResources->Trim();
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
}

void Sample::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, rotation))
        return;

    CreateWindowSizeDependentResources();
}

void Sample::ValidateDevice()
{
    m_deviceResources->ValidateDevice();
}

// Properties
void Sample::GetDefaultSize(int& width, int& height) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
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

    m_console->SetRotation(m_deviceResources->GetRotation());
}

void Sample::OnDeviceLost()
{
	m_consoleIsValid = false;
    m_console->ReleaseDevice();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
