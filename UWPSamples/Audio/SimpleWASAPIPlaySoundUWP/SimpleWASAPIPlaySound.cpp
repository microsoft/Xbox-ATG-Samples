//--------------------------------------------------------------------------------------
// SimpleWASAPIPlaySound.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleWASAPIPlaySound.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;
using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_gamepadPresent(false)
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();  	
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

	// Initialize MF
	HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
	if (SUCCEEDED(hr))
	{
		m_wm = ref new WASAPIManager();
		m_wm->SetVolume(50);
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}

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

	bool togglePlayback = false;

	auto kb = m_keyboard->GetState();
	m_keyboardButtons.Update(kb);

    auto pad = m_gamePad->GetState(0);
    m_gamepadPresent = pad.IsConnected();
    if (m_gamepadPresent)
    {
		m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }
		
		if (m_gamePadButtons.a == m_gamePadButtons.RELEASED)
		{
			togglePlayback = true;
		}
	}
	else
	{ 
		m_gamePadButtons.Reset();

        if (m_keyboardButtons.IsKeyReleased(DirectX::Keyboard::Keys::Space))
        {
            togglePlayback = true;
        }

        if (kb.Escape)
        {
            ExitSample();
        }
	}

	//Start/Stop sound playback
	if (togglePlayback)
	{
		m_wm->PlayPauseToggle();
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

    auto rect = m_deviceResources->GetOutputSize();
    auto safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(rect.right, rect.bottom);

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

    m_spriteBatch->Begin();

    m_spriteBatch->Draw(m_background.Get(), m_deviceResources->GetOutputSize());

    wchar_t str[128] = { 0 };
	swprintf_s(str, L"Simple WASAPI Playback:");
	m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
	pos.y += 30;
	swprintf_s(str, L"Audio Source - Test tone at 440hz : %s", (m_wm->IsPlaying())? L"Is Playing" : L"Is Stopped");
    m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);

    const wchar_t* legend = m_gamepadPresent
        ? L"Press [A] to start/stop playback   [View] Exit"
        : L"Use Space to start/stop playback   Esc: Exit";

    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), legend,
        XMFLOAT2(float(safeRect.left), float(safeRect.bottom) - m_font->GetLineSpacing()),
        ATG::Colors::LightGrey);

    m_spriteBatch->End();

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

    // Clear the views
    auto renderTarget = m_deviceResources->GetRenderTargetView();

    // Don't need to clear color as the sample draws a fullscreen image background

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

    // Suspend audio engine
	m_wm->StopDevice();
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
	m_gamePadButtons.Reset();
	m_keyboardButtons.Reset();

    // Resume audio engine
	m_wm->StartDevice();

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
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"ATGSampleBackground.DDS", nullptr, m_background.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    m_spriteBatch->SetRotation(m_deviceResources->GetRotation());
}

void Sample::OnDeviceLost()
{
    m_spriteBatch.reset();
    m_font.reset();
    m_ctrlFont.reset();
    m_background.Reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

