//--------------------------------------------------------------------------------------
// Gamepad.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Gamepad.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;
using namespace Windows::Foundation;
using namespace Windows::Xbox::Input;

namespace GamepadManager
{
    IGamepad^ GetFirstGamepad()
    {
        IGamepad^ gamepad = nullptr;

        auto allGamepads = Gamepad::Gamepads;
        if (allGamepads->Size > 0)
        {
            gamepad = allGamepads->GetAt(0);
        }

        return gamepad;
    }
};

Sample::Sample() noexcept(false) :
    m_currentGamepadNeedsRefresh(false),
    m_trusted(false),
    m_leftTrigger(0),
    m_rightTrigger(0),
    m_leftStickX(0),
    m_leftStickY(0),
    m_rightStickX(0),
    m_rightStickY(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_currentGamepad = GamepadManager::GetFirstGamepad();
    m_currentGamepadNeedsRefresh = false;

    Gamepad::GamepadAdded += ref new EventHandler<GamepadAddedEventArgs^ >([=](Platform::Object^, GamepadAddedEventArgs^ args)
    {
        m_currentGamepadNeedsRefresh = true;
    });

    Gamepad::GamepadRemoved += ref new EventHandler<GamepadRemovedEventArgs^ >([=](Platform::Object^, GamepadRemovedEventArgs^ args)
    {
        m_currentGamepadNeedsRefresh = true;
    });
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame");

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
}

// Updates the world.
void Sample::Update(DX::StepTimer const& )
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    if (m_currentGamepadNeedsRefresh)
    {
        auto mostRecentGamepad = GamepadManager::GetFirstGamepad();
        if (m_currentGamepad != mostRecentGamepad)
        {
            m_currentGamepad = mostRecentGamepad;
        
#if _XDK_VER >= 0x42ED07D9 /* XDK Edition 180400 */
            auto trustedGamepad = dynamic_cast<Windows::Xbox::Input::IGamepad2^>(m_currentGamepad);

            // Do not make policy decisions soley based on the "IsTrusted" property!
            // It is meant as a tool alongside other game metrics
            m_trusted = trustedGamepad->IsTrusted;
#endif
        }

        m_currentGamepadNeedsRefresh = false;
    }

    if (m_currentGamepad == nullptr)
    {
        m_buttonString.clear();
        m_trusted = false;
        PIXEndEvent();
        return;
    }

    m_reading = m_currentGamepad->GetCurrentReading();

    m_buttonString = L"Buttons pressed:  ";

    int exitComboPressed = 0;

    if (m_reading->IsDPadUpPressed)
    {
        m_buttonString += L"[DPad]Up ";
    }

    if (m_reading->IsDPadDownPressed)
    {
        m_buttonString += L"[DPad]Down ";
    }

    if (m_reading->IsDPadRightPressed)
    {
        m_buttonString += L"[DPad]Right ";
    }

    if (m_reading->IsDPadLeftPressed)
    {
        m_buttonString += L"[DPad]Left ";
    }

    if (m_reading->IsAPressed)
    {
        m_buttonString += L"[A] ";
    }

    if (m_reading->IsBPressed)
    {
        m_buttonString += L"[B] ";
    }

    if (m_reading->IsXPressed)
    {
        m_buttonString += L"[X] ";
    }

    if (m_reading->IsYPressed)
    {
        m_buttonString += L"[Y] ";
    }

    if (m_reading->IsLeftShoulderPressed)
    {
        m_buttonString += L"[LB] ";
        exitComboPressed += 1;
    }

    if (m_reading->IsRightShoulderPressed)
    {
        m_buttonString += L"[RB] ";
        exitComboPressed += 1;
    }

    if (m_reading->IsLeftThumbstickPressed)
    {
        m_buttonString += L"[LThumb] ";
    }

    if (m_reading->IsRightThumbstickPressed)
    {
        m_buttonString += L"[RThumb] ";
    }

    if (m_reading->IsMenuPressed)
    {
        m_buttonString += L"[Menu] ";
        exitComboPressed += 1;
    }

    if (m_reading->IsViewPressed)
    {
        m_buttonString += L"[View] ";
        exitComboPressed += 1;
    }

    m_leftTrigger = m_reading->LeftTrigger;
    m_rightTrigger = m_reading->RightTrigger;
    m_leftStickX = m_reading->LeftThumbstickX;
    m_leftStickY = m_reading->LeftThumbstickY;
    m_rightStickX = m_reading->RightThumbstickX;
    m_rightStickY = m_reading->RightThumbstickY;

    if (exitComboPressed == 4)
        ExitSample();

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
    
        pos.y += m_font->GetLineSpacing() * 2.f;

#if _XDK_VER >= 0x42ED07D9 /* XDK Edition 180400 */
        if (m_trusted)
        {
            DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), L"IsTrusted: True", pos);
        }
        else
        {
            DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), L"IsTrusted: False", pos);
        }
#endif
    }
    else
    {
        m_font->DrawString(m_spriteBatch.get(), L"No controller connected", pos, ATG::Colors::Orange);
    }

    DX::DrawControllerString(m_spriteBatch.get(),
        m_font.get(), m_ctrlFont.get(),
        L"[RB][LB][View][Menu] Exit",
        XMFLOAT2(float(safeRect.left),
            float(safeRect.bottom) - m_font->GetLineSpacing()),
        ATG::Colors::LightGrey);

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

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_24.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneController.spritefont");

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"gamepad.dds", nullptr, m_background.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion
