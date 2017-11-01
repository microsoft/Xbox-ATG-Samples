//--------------------------------------------------------------------------------------
// SimpleWASAPICaptureXDK.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleWASAPICaptureXDK.h"

#include "ATGColors.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;
using namespace Windows::Media::Devices;
using namespace Windows::Foundation;

bool g_bListDirty = true;
int g_CaptureID = 0;

void NotifyListUpdate(int iCaptureID)
{
	g_CaptureID = iCaptureID;
	g_bListDirty = true;
}

Sample::Sample()
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

	m_wm = ref new WASAPIManager();

	m_wm->StartDevice();

	m_wm->SetDeviceChangeCallback(&NotifyListUpdate);

	m_bHasCaptured = false;
	m_keyDown = false;
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

		if (!m_keyDown)
		{
			if (pad.IsAPressed() && m_bHasCaptured)
			{
				m_wm->PlayPauseToggle();
				m_keyDown = true;
			}
			else if (pad.IsBPressed())
			{
				m_wm->RecordToggle();
				m_keyDown = true;
				m_bHasCaptured = true;
			}
			else if (pad.IsXPressed())
			{
				m_wm->LoopbackToggle();
				m_keyDown = true;
			}
			else if (pad.IsDPadUpPressed())
			{
				m_keyDown = true;
				g_CaptureID--;
				if (g_CaptureID < 0)
				{
					g_CaptureID = (int)m_deviceList.size() - 1;
				}

				m_wm->SetCaptureDevice(g_CaptureID);
			}
			else if (pad.IsDPadDownPressed())
			{
				m_keyDown = true;
				g_CaptureID++;
				if (g_CaptureID > m_deviceList.size() - 1)
				{
					g_CaptureID = 0;
				}

				m_wm->SetCaptureDevice(g_CaptureID);
			}
		}
		else if(!pad.IsAPressed() && !pad.IsBPressed() && !pad.IsXPressed() && !pad.IsDPadUpPressed() && !pad.IsDPadDownPressed())
		{
			m_keyDown = false;
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

    auto rect = m_deviceResources->GetOutputSize();
    auto safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(rect.right, rect.bottom);
	
	m_wm->GetStatus(&m_managerStatus);

	if (g_bListDirty)
	{
		m_wm->GetCaptureDevices(m_deviceList);
		//m_wm->SetCaptureDevice(g_CaptureID);
		g_bListDirty = false;
	}

	XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));
	std::wstring tempString;

	m_spriteBatch->Begin();

	m_font->DrawString(m_spriteBatch.get(), L"Audio captured from the selected mic is looped to the default output", pos, ATG::Colors::OffWhite);
	pos.y += m_font->GetLineSpacing() * 1.f;
	m_font->DrawString(m_spriteBatch.get(), L"Note that no sample conversion is done!", pos, ATG::Colors::OffWhite);
	pos.y += m_font->GetLineSpacing() * 1.6f;
	
	
	if (m_deviceList.size() < 1)
	{
		m_font->DrawString(m_spriteBatch.get(), L"No capture devices!", pos, ATG::Colors::Orange);
	}
	else
	{
		for (int i = 0; i<m_deviceList.size(); i++)
		{
			if (i == g_CaptureID)
			{
				tempString = L"> " + std::wstring(m_deviceList.at(i));
			}
			else
			{
				tempString = m_deviceList.at(i);
			}

			m_font->DrawString(m_spriteBatch.get(), tempString.c_str(), pos, ATG::Colors::OffWhite);
			pos.y += m_font->GetLineSpacing() * 1.f;
		}
	}
	
	if (m_bHasCaptured)
	{
		m_font->DrawString(m_spriteBatch.get(), L"Press A Button to start / stop playback of last recording", pos, ATG::Colors::OffWhite);
		pos.y += m_font->GetLineSpacing() * 1.f;
	}

	m_font->DrawString(m_spriteBatch.get(), L"Press B Button to start / stop recording", pos, ATG::Colors::OffWhite);
	pos.y += m_font->GetLineSpacing() * 1.f;
	m_font->DrawString(m_spriteBatch.get(), L"Press X Button to enable / disable loopback (may cause feedback)", pos, ATG::Colors::OffWhite);
	pos.y += m_font->GetLineSpacing() * 1.f;
	m_font->DrawString(m_spriteBatch.get(), L"Press DPad Up/Down to change capture device", pos, ATG::Colors::OffWhite);
	pos.y += m_font->GetLineSpacing() * 1.f;

	tempString = L"Capture: " + convertBoolToRunning(m_managerStatus.bCapturing);
	m_font->DrawString(m_spriteBatch.get(), tempString.c_str(), pos, ATG::Colors::OffWhite);
	pos.y += m_font->GetLineSpacing() * 1.f;
	tempString = L"Playback: " + convertBoolToRunning(m_managerStatus.bPlaying);
	m_font->DrawString(m_spriteBatch.get(), tempString.c_str(), pos, ATG::Colors::OffWhite);
	pos.y += m_font->GetLineSpacing() * 1.f;
	tempString = L"Loopback: " + convertBoolToEnabled(m_managerStatus.bLoopback);
	m_font->DrawString(m_spriteBatch.get(), tempString.c_str(), pos, ATG::Colors::OffWhite);
	pos.y += m_font->GetLineSpacing() * 1.f;

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
    m_gamePadButtons.Reset();
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
    m_font->SetDefaultCharacter(L' ');
    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion
