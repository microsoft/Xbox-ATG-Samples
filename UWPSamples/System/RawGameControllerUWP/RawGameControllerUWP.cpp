//--------------------------------------------------------------------------------------
// RawGameControllerUWP.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "RawGameControllerUWP.h"

#include "ATGColors.h"
#include "ControllerFont.h"

using namespace DirectX;
using namespace Windows::Gaming::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Platform::Collections;

using Microsoft::WRL::ComPtr;

Sample::Sample()
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();  	
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_localCollection = ref new Vector<RawGameController^>();

    auto gamecontrollers = RawGameController::RawGameControllers;
    for (auto gamecontroller : gamecontrollers)
    {
        m_localCollection->Append(gamecontroller);
    }

    RawGameController::RawGameControllerAdded += ref new EventHandler<RawGameController^ >([=](Platform::Object^, RawGameController^ args)
    {
        m_localCollection->Append(args);
        m_currentControllerNeedsRefresh = true;
    });

    RawGameController::RawGameControllerRemoved += ref new EventHandler<RawGameController^ >([=](Platform::Object^, RawGameController^ args)
    {
        unsigned int index;
        if (m_localCollection->IndexOf(args, &index))
        {
            m_localCollection->RemoveAt(index);
            m_currentControllerNeedsRefresh = true;
        }
    });

    m_currentController = GetFirstController();
	RefreshControllerInfo();

    m_currentControllerNeedsRefresh = false;
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

RawGameController^ Sample::GetFirstController()
{
    RawGameController^ controller = nullptr;

    if (m_localCollection->Size > 0)
    {
        controller = m_localCollection->GetAt(0);
    }
    
    return controller;
}

void Sample::RefreshControllerInfo()
{
    if (m_currentController != nullptr)
    {
		m_currentButtonCount = m_currentController->ButtonCount;
		m_currentAxisCount = m_currentController->AxisCount;
		m_currentSwitchCount = m_currentController->SwitchCount;

		m_currentButtonReading = ref new Platform::Array<bool>(m_currentButtonCount);
        m_currentSwitchReading = ref new Platform::Array<GameControllerSwitchPosition>(m_currentSwitchCount);
        m_currentAxisReading = ref new Platform::Array<double>(m_currentAxisCount);
    }
}

// Updates the world.
void Sample::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    if (m_currentControllerNeedsRefresh)
    {
        auto mostRecentController = GetFirstController();
        if (m_currentController != mostRecentController)
        {
            m_currentController = mostRecentController;
			RefreshControllerInfo();
        }
        m_currentControllerNeedsRefresh = false;
    }

    if (m_currentController == nullptr)
    {
        m_buttonString.clear();
        PIXEndEvent();
        return;
    }

    GameControllerButtonLabel buttonLabel;

    m_currentController->GetCurrentReading(
        m_currentButtonReading,
        m_currentSwitchReading,
        m_currentAxisReading);

    m_buttonString = L"Buttons pressed:  ";

    for (uint32_t i = 0; i < m_currentButtonCount; i++)
    {
        if (m_currentButtonReading[i])
        {
            buttonLabel = m_currentController->GetButtonLabel(i);

            switch (buttonLabel)
            {
            case GameControllerButtonLabel::XboxA:
                m_buttonString += L"[A] ";
                break;
            case GameControllerButtonLabel::XboxB:
                m_buttonString += L"[B] ";
                break;
            case GameControllerButtonLabel::XboxX:
                m_buttonString += L"[X] ";
                break;
            case GameControllerButtonLabel::XboxY:
                m_buttonString += L"[Y] ";
                break;
            case GameControllerButtonLabel::XboxLeftBumper:
                m_buttonString += L"[LB] ";
                break;
            case GameControllerButtonLabel::XboxRightBumper:
                m_buttonString += L"[RB] ";
                break;
            case GameControllerButtonLabel::XboxLeftStickButton:
                m_buttonString += L"[LThumb] ";
                break;
            case GameControllerButtonLabel::XboxRightStickButton:
                m_buttonString += L"[RThumb] ";
                break;
            case GameControllerButtonLabel::XboxMenu:
                m_buttonString += L"[Menu] ";
                break;
            case GameControllerButtonLabel::XboxView:
                m_buttonString += L"[View] ";
                break;
			case GameControllerButtonLabel::XboxUp:
				m_buttonString += L"[DPad]Up ";
				break;
			case GameControllerButtonLabel::XboxDown:
				m_buttonString += L"[DPad]Down ";
				break;
			case GameControllerButtonLabel::XboxLeft:
				m_buttonString += L"[DPad]Left ";
				break;
			case GameControllerButtonLabel::XboxRight:
				m_buttonString += L"[DPad]Right ";
				break;
			}
        }
    }

    for (uint32_t i = 0; i < m_currentSwitchCount; i++)
    {
		//Handle m_currentSwitchReading[i], reading GameControllerSwitchPosition
    }

	if (m_currentAxisCount == 6)
	{
		//Xbox controllers have 6 axis: 2 for each stick and one for each trigger
		m_leftStickX = m_currentAxisReading[0];
		m_leftStickY = m_currentAxisReading[1];
		m_rightStickX = m_currentAxisReading[2];
		m_rightStickY = m_currentAxisReading[3];
		m_leftTrigger = m_currentAxisReading[4];
		m_rightTrigger = m_currentAxisReading[5];
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
    wchar_t tempString[256] = {};

    m_spriteBatch->Begin();
    m_spriteBatch->Draw(m_background.Get(), m_deviceResources->GetOutputSize());
    
    if (!m_buttonString.empty())
    {
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), m_buttonString.c_str(), pos);
        pos.y += m_font->GetLineSpacing() * 1.5f;

        swprintf(tempString, 255, L"[LT]  %1.3f", m_leftTrigger);
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), tempString, pos);
        pos.y += m_font->GetLineSpacing() * 1.5f;

        swprintf(tempString, 255, L"[RT]  %1.3f", m_rightTrigger);
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), tempString, pos);
        pos.y += m_font->GetLineSpacing() * 1.5f;

        swprintf(tempString, 255, L"[LThumb]  X: %1.3f  Y: %1.3f", m_leftStickX, m_leftStickY);
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), tempString, pos);
        pos.y += m_font->GetLineSpacing() * 1.5f;

        swprintf(tempString, 255, L"[RThumb]  X: %1.3f  Y: %1.3f", m_rightStickX, m_rightStickY);
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), tempString, pos);
    }
    else
    {
        m_font->DrawString(m_spriteBatch.get(), L"No controller connected", pos, ATG::Colors::Orange);
    }

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

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_24.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneController.spritefont");

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"gamepad.dds", nullptr, m_background.ReleaseAndGetAddressOf()));
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
