//--------------------------------------------------------------------------------------
// GamepadUWP.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "GamepadUWP.h"

#include "ATGColors.h"
#include "ControllerFont.h"

using namespace DirectX;
using namespace Windows::Gaming::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

using Microsoft::WRL::ComPtr;

namespace GamepadManager
{
    Gamepad^ GetFirstGamepad()
    {
        Gamepad^ gamepad = nullptr;

        auto allGamepads = Gamepad::Gamepads;
        if (allGamepads->Size > 0)
        {
            gamepad = allGamepads->GetAt(0);
        }

        return gamepad;
    }
};

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

    m_currentGamepad = GamepadManager::GetFirstGamepad();
    m_currentGamepadNeedsRefresh = false;

    Gamepad::GamepadAdded += ref new EventHandler<Gamepad^ >([=](Platform::Object^, Gamepad^ args)
    {
        m_currentGamepadNeedsRefresh = true;
    });

    Gamepad::GamepadRemoved += ref new EventHandler<Gamepad^ >([=](Platform::Object^, Gamepad^ args)
    {
        m_currentGamepadNeedsRefresh = true;
    });
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

    if (m_currentGamepadNeedsRefresh)
    {
        auto mostRecentGamepad = GamepadManager::GetFirstGamepad();
        if (m_currentGamepad != mostRecentGamepad)
        {
            m_currentGamepad = mostRecentGamepad;
        }
        m_currentGamepadNeedsRefresh = false;
    }

    if (m_currentGamepad == nullptr)
    {
        m_buttonString.clear();
        PIXEndEvent();
        return;
    }

    m_reading = m_currentGamepad->GetCurrentReading();

    m_buttonString = L"Buttons pressed:  ";

    if (static_cast<int>(m_reading.Buttons & GamepadButtons::DPadUp) != 0)
    {
        m_buttonString += L"[DPad]Up ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::DPadDown) != 0)
    {
        m_buttonString += L"[DPad]Down ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::DPadRight) != 0)
    {
        m_buttonString += L"[DPad]Right ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::DPadLeft) != 0)
    {
        m_buttonString += L"[DPad]Left ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::A) != 0)
    {
        m_buttonString += L"[A] ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::B) != 0)
    {
        m_buttonString += L"[B] ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::X) != 0)
    {
        m_buttonString += L"[X] ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::Y) != 0)
    {
        m_buttonString += L"[Y] ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::LeftShoulder) != 0)
    {
        m_buttonString += L"[LB] ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::RightShoulder) != 0)
    {
        m_buttonString += L"[RB] ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::LeftThumbstick) != 0)
    {
        m_buttonString += L"[LThumb] ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::RightThumbstick) != 0)
    {
        m_buttonString += L"[RThumb] ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::Menu) != 0)
    {
        m_buttonString += L"[Menu] ";
    }
    
    if (static_cast<int>(m_reading.Buttons & GamepadButtons::View) != 0)
    {
        m_buttonString += L"[View] ";
    }

    m_leftTrigger = m_reading.LeftTrigger;
    m_rightTrigger = m_reading.RightTrigger;
    m_leftStickX = m_reading.LeftThumbstickX;
    m_leftStickY = m_reading.LeftThumbstickY;
    m_rightStickX = m_reading.RightThumbstickX;
    m_rightStickY = m_reading.RightThumbstickY;

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
    auto renderTarget = m_deviceResources->GetBackBufferRenderTargetView();

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
