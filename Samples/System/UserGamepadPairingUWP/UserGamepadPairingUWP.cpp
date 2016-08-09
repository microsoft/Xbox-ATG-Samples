//--------------------------------------------------------------------------------------
// UserGamepadPairingUWP.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "UserGamepadPairingUWP.h"

#include "ATGColors.h"
#include "ControllerFont.h"

using namespace concurrency;
using namespace DirectX;
using namespace Windows::Gaming::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

using Microsoft::WRL::ComPtr;

Sample::Sample()
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_UNKNOWN);
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

    m_needsRefresh = true;

    Gamepad::GamepadAdded += ref new EventHandler<Gamepad^ >([=](Platform::Object^, Gamepad^ args)
    {
        m_needsRefresh = true;
    });

    Gamepad::GamepadRemoved += ref new EventHandler<Gamepad^ >([=](Platform::Object^, Gamepad^ args)
    {
        m_needsRefresh = true;
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
        m_userTasks.clear();

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
            Gamepad^ myGamepad = m_gamepadList->GetAt(i);
            myGamepad->UserChanged += ref new Windows::Foundation::TypedEventHandler<IGameController^, Windows::System::UserChangedEventArgs^>([=](IGameController^, Windows::System::UserChangedEventArgs^ args)
            {
                m_needsRefresh = true;
            });
        }
    }

    if (m_needsStrings)
    {
        bool isNotDone = false;

        for (unsigned int i = 0; i < m_gamepadList->Size; i++)
        {
            //If no task, create
            if(m_userTasks.size() == i)
			{
				Windows::System::User^ myUser = m_gamepadList->GetAt(i)->User;
				auto getDisplayName = myUser->GetPropertyAsync(Windows::System::KnownUserProperties::DisplayName);
                m_userTasks.emplace_back(create_task(getDisplayName));
				m_userStrings.emplace_back(L"");
				isNotDone = true;
			}
			else
			{
				//otherwise just check
                auto checkTask = m_userTasks.at(i);
                if (checkTask.is_done())
                {
                    auto displayName = checkTask.get();
                    auto stringVal = safe_cast<Platform::String^>(displayName);
                    m_userStrings.at(i) = stringVal->Data();
                }
                else
                {
                    isNotDone = true;
                }
			}
        }

        m_needsStrings = isNotDone;
    }

    for (unsigned int i = 0; i < m_gamepadList->Size; i++)
    {
        m_buttonStrings.clear();
        m_leftTrigger.clear();
        m_rightTrigger.clear();
        m_leftStickX.clear();
        m_leftStickY.clear();
        m_rightStickX.clear();
        m_rightStickY.clear();

        Gamepad^ myGamepad = m_gamepadList->GetAt(i);
        m_reading = myGamepad->GetCurrentReading();

        std::wstring localButtonString = L"Buttons pressed:  ";

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::DPadUp) != 0)
        {
            localButtonString += L"[DPad]Up ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::DPadDown) != 0)
        {
            localButtonString += L"[DPad]Down ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::DPadRight) != 0)
        {
            localButtonString += L"[DPad]Right ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::DPadLeft) != 0)
        {
            localButtonString += L"[DPad]Left ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::A) != 0)
        {
            localButtonString += L"[A] ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::B) != 0)
        {
            localButtonString += L"[B] ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::X) != 0)
        {
            localButtonString += L"[X] ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::Y) != 0)
        {
            localButtonString += L"[Y] ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::LeftShoulder) != 0)
        {
            localButtonString += L"[LB] ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::RightShoulder) != 0)
        {
            localButtonString += L"[RB] ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::LeftThumbstick) != 0)
        {
            localButtonString += L"[LThumb] ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::RightThumbstick) != 0)
        {
            localButtonString += L"[RThumb] ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::Menu) != 0)
        {
            localButtonString += L"[Menu] ";
        }

        if (static_cast<int>(m_reading.Buttons & GamepadButtons::View) != 0)
        {
            localButtonString += L"[View] ";
        }

        m_leftTrigger.emplace_back(m_reading.LeftTrigger);
        m_rightTrigger.emplace_back(m_reading.RightTrigger);
        m_leftStickX.emplace_back(m_reading.LeftThumbstickX);
        m_leftStickY.emplace_back(m_reading.LeftThumbstickY);
        m_rightStickX.emplace_back(m_reading.RightThumbstickX);
        m_rightStickY.emplace_back(m_reading.RightThumbstickY);
        m_buttonStrings.emplace_back(localButtonString);
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

    if (m_buttonStrings.empty())
    {
        m_font->DrawString(m_spriteBatch.get(), L"No controller connected", pos, ATG::Colors::Orange);
    }
    else
    {
        for (size_t i = 0; i < m_buttonStrings.size(); i++)
        {
            swprintf_s(tempString, L"Player %zd: %s", i+1, m_userStrings.at(i).c_str());
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
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
