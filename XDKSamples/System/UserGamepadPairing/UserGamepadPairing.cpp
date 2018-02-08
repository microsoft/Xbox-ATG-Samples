//--------------------------------------------------------------------------------------
// UserGamepadPairing.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "UserGamepadPairing.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;
using namespace Windows::Xbox::System;
using namespace Windows::Xbox::Input;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::System;
using namespace Windows::Foundation;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_frame(0)
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

    m_needsRefresh = true;
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
void Sample::Update(DX::StepTimer const& /*timer*/)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    UpdateControllers();

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
    auto safeRect = DirectX::SimpleMath::Viewport::ComputeTitleSafeArea(rect.right, rect.bottom);

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));
    wchar_t tempString[256] = {};

    m_spriteBatch->Begin();

    if (m_buttonStrings.empty())
    {
        m_font->DrawString(m_spriteBatch.get(), L"No controller connected", pos, ATG::Colors::Orange);
    }
    else
    {
        for (size_t i = 0; i < m_buttonStrings.size(); i++)
        {
            swprintf_s(tempString, L"Player %zd: %s", i + 1, m_userStrings.at(i).c_str());
            m_font->DrawString(m_spriteBatch.get(), tempString, pos, ATG::Colors::White);
            pos.y += m_font->GetLineSpacing() * 1.3f;
            pos.x += 20;

            DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), m_buttonStrings.at(i).c_str(), pos);
            pos.y += m_font->GetLineSpacing() * 1.3f;

            swprintf_s(tempString, L"[LT]  %1.3f   [RT]  %1.3f", m_leftTrigger.at(i), m_rightTrigger.at(i));
            DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), tempString, pos);
            pos.y += m_font->GetLineSpacing() * 1.3f;

            swprintf_s(tempString, L"[LThumb]  X: %1.3f  Y: %1.3f   [RThumb]  X: %1.3f  Y: %1.3f",
                m_leftStickX.at(i), m_leftStickY.at(i), m_rightStickX.at(i), m_rightStickY.at(i));
            DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), tempString, pos);
            pos.y += m_font->GetLineSpacing() * 1.3f;
            pos.x -= 20;
        }
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
    m_gamePadButtons.Reset();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    m_spriteBatch = std::make_unique<SpriteBatch>(m_deviceResources->GetD3DDeviceContext());

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_24.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneController.spritefont");
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto viewport = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewport);
}
#pragma endregion

void Sample::UpdateControllers()
{
    if (m_needsRefresh)
    {
        m_needsStrings = true;
        m_needsRefresh = false;
        m_gamepadList = Gamepad::Gamepads;

        m_buttonStrings.clear();
        m_leftTrigger.clear();
        m_rightTrigger.clear();
        m_leftStickX.clear();
        m_leftStickY.clear();
        m_rightStickX.clear();
        m_rightStickY.clear();
        m_userStrings.clear();

        m_buttonStrings.reserve(m_gamepadList->Size);
        m_leftTrigger.reserve(m_gamepadList->Size);
        m_rightTrigger.reserve(m_gamepadList->Size);
        m_leftStickX.reserve(m_gamepadList->Size);
        m_leftStickY.reserve(m_gamepadList->Size);
        m_rightStickX.reserve(m_gamepadList->Size);
        m_rightStickY.reserve(m_gamepadList->Size);
        m_userStrings.reserve(m_gamepadList->Size);
        m_userTasks.reserve(m_gamepadList->Size);

        for (unsigned int i = 0; i < m_gamepadList->Size; i++)
        {
            IGamepad^ myGamepad = m_gamepadList->GetAt(i);

            User::UserAdded += ref new EventHandler<UserAddedEventArgs^ >([=](Platform::Object^, UserAddedEventArgs^ args)
            {
                m_needsRefresh = true;
            });

            User::UserRemoved += ref new EventHandler<UserRemovedEventArgs^ >([this](Platform::Object^, UserRemovedEventArgs^ args)
            {
                m_needsRefresh = true;
            });
        }
    }

    if (m_needsStrings)
    {
        for (unsigned int i = 0; i < m_gamepadList->Size; i++)
        {
            auto gamepad = m_gamepadList->GetAt(i);
            if (gamepad != nullptr)
            {
                Windows::Xbox::System::User^ myUser = gamepad->User;
                m_userStrings.insert(m_userStrings.begin() + i, myUser != nullptr ? myUser->DisplayInfo->Gamertag->Data() : L"Not signed in");
            }
        }

        m_needsStrings = false;
    }

    m_buttonStrings.clear();
    m_leftTrigger.clear();
    m_rightTrigger.clear();
    m_leftStickX.clear();
    m_leftStickY.clear();
    m_rightStickX.clear();
    m_rightStickY.clear();

    for (unsigned int i = 0; i < m_gamepadList->Size; i++)
    {
        IGamepad^ myGamepad = m_gamepadList->GetAt(i);
        m_reading = myGamepad->GetCurrentReading();

        std::wstring localButtonString = L"Buttons pressed:  ";

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::DPadUp) != 0)
        {
            localButtonString += L"[DPad]Up ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::DPadDown) != 0)
        {
            localButtonString += L"[DPad]Down ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::DPadRight) != 0)
        {
            localButtonString += L"[DPad]Right ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::DPadLeft) != 0)
        {
            localButtonString += L"[DPad]Left ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::A) != 0)
        {
            localButtonString += L"[A] ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::B) != 0)
        {
            localButtonString += L"[B] ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::X) != 0)
        {
            localButtonString += L"[X] ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::Y) != 0)
        {
            localButtonString += L"[Y] ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::LeftShoulder) != 0)
        {
            localButtonString += L"[LB] ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::RightShoulder) != 0)
        {
            localButtonString += L"[RB] ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::LeftThumbstick) != 0)
        {
            localButtonString += L"[LThumb] ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::RightThumbstick) != 0)
        {
            localButtonString += L"[RThumb] ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::Menu) != 0)
        {
            localButtonString += L"[Menu] ";
        }

        if (static_cast<int>(m_reading->Buttons & GamepadButtons::View) != 0)
        {
            localButtonString += L"[View] ";
        }

        m_leftTrigger.emplace_back(m_reading->LeftTrigger);
        m_rightTrigger.emplace_back(m_reading->RightTrigger);
        m_leftStickX.emplace_back(m_reading->LeftThumbstickX);
        m_leftStickY.emplace_back(m_reading->LeftThumbstickY);
        m_rightStickX.emplace_back(m_reading->RightThumbstickX);
        m_rightStickY.emplace_back(m_reading->RightThumbstickY);
        m_buttonStrings.emplace_back(localButtonString);
    }
}