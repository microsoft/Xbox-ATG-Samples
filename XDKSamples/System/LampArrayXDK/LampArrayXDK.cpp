//--------------------------------------------------------------------------------------
// LampArrayXDK.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "LampArrayXDK.h"

#include "ATGColors.h"


extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr; 

enum LampPresets
{
	WASDRed,
	WASDBlink,
	Bitmap,
	Snake,
	SolidGreen,
	CycleColors,
	BlinkColors
};

const wchar_t* g_PresetNames[] =
{
	L"WASD Red",
	L"WASD Blink",
	L"Bitmap",
	L"Snake",
	L"Solid Green",
	L"Cycle Colors",
	L"Blink Colors"
};


Sample::Sample() noexcept(false) :
	m_keyDown(false),
	m_frame(0),
	m_effectIndex(0)
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
	m_keyboard = std::make_unique<Keyboard>();
	m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

	m_lightingManager = LightingManager::GetInstance();
	UpdateLighting();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %llu", m_frame);

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

	auto kb = m_keyboard->GetState();
	m_keyboardButtons.Update(kb);

	if (m_keyDown && !kb.Left && !kb.Right)
	{
		m_keyDown = false;
	}

	if (kb.Left && !m_keyDown)
	{
		m_effectIndex--;
		m_keyDown = true;

		if (m_effectIndex < 0)
		{
			m_effectIndex = _countof(g_PresetNames) - 1;
		}
		else if (m_effectIndex >= _countof(g_PresetNames))
		{
			m_effectIndex = 0;
		}

		UpdateLighting();
	}
	else if (kb.Right && !m_keyDown)
	{
		m_effectIndex++;
		m_keyDown = true;

		if (m_effectIndex < 0)
		{
			m_effectIndex = _countof(g_PresetNames) - 1;
		}
		else if (m_effectIndex >= _countof(g_PresetNames))
		{
			m_effectIndex = 0;
		}

		UpdateLighting();
	}

    PIXEndEvent();
}
#pragma endregion

void Sample::UpdateLighting()
{
	m_lightingManager->ClearLampArrays();
	
	switch (m_effectIndex)
	{
	case LampPresets::WASDRed:
		m_lightingManager->WasdKeysRed();
		break;
	case LampPresets::Snake:
		m_lightingManager->PlaySnakeEffect();
		break;
	case LampPresets::Bitmap:
		m_lightingManager->PlaySimpleBitmapEffect();
		break;
	case LampPresets::WASDBlink:
		m_lightingManager->BlinkWasdKeys();
		break;
	case LampPresets::SolidGreen:
		m_lightingManager->PlayGreenSolidEffect();
		break;
	case LampPresets::CycleColors:
		m_lightingManager->CyclePrimaryColors();
		break;
	case LampPresets::BlinkColors:
		m_lightingManager->BlinkRandomColors();
		break;
	}
}

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

	auto rect = m_deviceResources->GetOutputSize();
	auto safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(rect.right, rect.bottom);
	XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));
	std::wstring tempString;

	m_spriteBatch->Begin();
	m_font->DrawString(m_spriteBatch.get(), L"LampArray", pos, ATG::Colors::White);
	pos.y += m_font->GetLineSpacing() * 1.1f;

	tempString = L"< " + std::wstring(g_PresetNames[m_effectIndex]) + L" >";
	m_font->DrawString(m_spriteBatch.get(), tempString.c_str(), pos, ATG::Colors::White);

	pos.y += m_font->GetLineSpacing() * 1.1f;

	if (!m_lightingManager->LampArrayAvailable())
	{
		tempString = L"No supported devices present";
		m_font->DrawString(m_spriteBatch.get(), tempString.c_str(), pos, ATG::Colors::Orange);
	}

	m_spriteBatch->End();
	
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
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
	auto context = m_deviceResources->GetD3DDeviceContext();
	auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

	m_spriteBatch = std::make_unique<SpriteBatch>(context);
	m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion
