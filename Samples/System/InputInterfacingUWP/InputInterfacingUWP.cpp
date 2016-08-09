//--------------------------------------------------------------------------------------
// InputInterfacingUWP.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "InputInterfacingUWP.h"

#include "ATGColors.h"
#include <Windows.Gaming.Input.h>

using namespace DirectX;
using namespace Windows::Gaming::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

using Microsoft::WRL::ComPtr;

namespace
{
    static const wchar_t* c_InputTestNames[] =
    {
        L"<Navigation Test>\n",
        L"<ArcadeStick Test>\n",
        L"<RacingWheel Test>\n",
    };

    static const wchar_t c_NavDescription[] =
        L"Using the UINavigationController allows you to read generic navigation \n"
        L"commands from a variety of input devices like wheels, gamepads, and \n"
        L"arcade sticks\n";
}

//--------------------------------------------------------------------------------------
// Helper class for managing the input devices
//--------------------------------------------------------------------------------------
namespace InputManager
{
    UINavigationController^ GetMostRecentNavController()
    {
        UINavigationController^ navController = nullptr;

        auto allNavControllers = UINavigationController::UINavigationControllers;
        if (allNavControllers->Size > 0)
        {
            navController = allNavControllers->GetAt(0);
        }

        return navController;
    }

    ArcadeStick^ GetMostRecentArcadeStick()
    {
        ArcadeStick^ arcadeStick = nullptr;

        auto allArcadeSticks = ArcadeStick::ArcadeSticks;
        if (allArcadeSticks->Size > 0)
        {
            arcadeStick = allArcadeSticks->GetAt(0);
        }

        return arcadeStick;
    }

    RacingWheel^ GetMostRecentRacingWheel()
    {
        RacingWheel^ racingWheel = nullptr;

        auto allRacingWheels = RacingWheel::RacingWheels;
        if (allRacingWheels->Size > 0)
        {
            racingWheel = allRacingWheels->GetAt(0);
        }

        return racingWheel;
    }
};

void Sample::GenerateNavString()
{
    m_buttonString = L"Nav inputs pressed:  ";

    if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::Up) != 0)
    {
        m_buttonString += L"Up ";
    }

    if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::Down) != 0)
    {
        m_buttonString += L"Down ";
    }

    if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::Left) != 0)
    {
        m_buttonString += L"Left ";
    }

    if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::Right) != 0)
    {
        m_buttonString += L"Right ";
    }

    if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::Accept) != 0)
    {
        m_buttonString += L"Accept ";
    }

    if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::Cancel) != 0)
    {
        m_buttonString += L"Cancel ";
    }

    if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::Menu) != 0)
    {
        m_buttonString += L"Menu ";
    }

    if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::None) != 0)
    {
        m_buttonString += L"None ";
    }

    if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::View) != 0)
    {
        m_buttonString += L"View ";
    }
}

void Sample::GenerateStickString()
{
    m_buttonString = L"Arcade Stick inputs pressed:  ";

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::StickUp) != 0)
    {
        m_buttonString += L"Up ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::StickDown) != 0)
    {
        m_buttonString += L"Down ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::StickLeft) != 0)
    {
        m_buttonString += L"Left ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::StickRight) != 0)
    {
        m_buttonString += L"Right ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::Action1) != 0)
    {
        m_buttonString += L"1 ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::Action2) != 0)
    {
        m_buttonString += L"2 ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::Action3) != 0)
    {
        m_buttonString += L"3 ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::Action4) != 0)
    {
        m_buttonString += L"4 ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::Action5) != 0)
    {
        m_buttonString += L"5 ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::Action6) != 0)
    {
        m_buttonString += L"6 ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::None) != 0)
    {
        m_buttonString += L"None ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::Special1) != 0)
    {
        m_buttonString += L"S1 ";
    }

    if (static_cast<int>(m_arcadeReading.Buttons & ArcadeStickButtons::Special2) != 0)
    {
        m_buttonString += L"S2 ";
    }
}

void Sample::DrawWheel(XMFLOAT2 startPosition)
{
    wchar_t wheelString[128] = {};

    swprintf_s(wheelString, L"Wheel %1.3f", m_wheelReading.Wheel);
    m_font->DrawString(m_spriteBatch.get(), wheelString, startPosition, ATG::Colors::Green);
    startPosition.y += m_font->GetLineSpacing() * 1.1f;

    swprintf_s(wheelString, L"Throttle %1.3f", m_wheelReading.Throttle);
    m_font->DrawString(m_spriteBatch.get(), wheelString, startPosition, ATG::Colors::Green);
    startPosition.y += m_font->GetLineSpacing() * 1.1f;

    swprintf_s(wheelString, L"Break %1.3f", m_wheelReading.Brake);
    m_font->DrawString(m_spriteBatch.get(), wheelString, startPosition, ATG::Colors::Green);
    startPosition.y += m_font->GetLineSpacing() * 1.1f;

    if (m_currentWheel->HasClutch)
    {
        swprintf_s(wheelString, L"Clutch %1.3f", m_wheelReading.Clutch);
        m_font->DrawString(m_spriteBatch.get(), wheelString, startPosition, ATG::Colors::Green);
        startPosition.y += m_font->GetLineSpacing() * 1.1f;
    }

    if (m_currentWheel->HasHandbrake)
    {
        swprintf_s(wheelString, L"Handbrake %1.3f", m_wheelReading.Handbrake);
        m_font->DrawString(m_spriteBatch.get(), wheelString, startPosition, ATG::Colors::Green);
        startPosition.y += m_font->GetLineSpacing() * 1.1f;
    }

    if (m_currentWheel->HasPatternShifter)
    {
        swprintf_s(wheelString, L"Shifter %d of %d", m_wheelReading.PatternShifterGear, m_currentWheel->MaxPatternShifterGear);
        m_font->DrawString(m_spriteBatch.get(), wheelString, startPosition, ATG::Colors::Green);
        startPosition.y += m_font->GetLineSpacing() * 1.1f;
    }
}

Sample::Sample()
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
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

    m_currentMode = NavigationDevice;
    m_selectPressed = false;
    m_connected = false;

    m_currentNav = InputManager::GetMostRecentNavController();
    m_currentStick = InputManager::GetMostRecentArcadeStick();
    m_currentWheel = InputManager::GetMostRecentRacingWheel();
    m_currentNavNeedsRefresh = false;
    m_currentWheelNeedsRefresh = false;
    m_currentStickNeedsRefresh = false;

    UINavigationController::UINavigationControllerAdded += ref new EventHandler<UINavigationController^ >([=](Platform::Object^, UINavigationController^ args)
    {
        m_currentNavNeedsRefresh = true;
    });

    UINavigationController::UINavigationControllerRemoved += ref new EventHandler<UINavigationController^ >([=](Platform::Object^, UINavigationController^ args)
    {
        m_currentNavNeedsRefresh = true;
    });

    ArcadeStick::ArcadeStickAdded += ref new EventHandler<ArcadeStick^ >([=](Platform::Object^, ArcadeStick^ args)
    {
        m_currentStickNeedsRefresh = true;
    });

    ArcadeStick::ArcadeStickRemoved += ref new EventHandler<ArcadeStick^ >([=](Platform::Object^, ArcadeStick^ args)
    {
        m_currentStickNeedsRefresh = true;
    });

    RacingWheel::RacingWheelAdded += ref new EventHandler<RacingWheel^ >([=](Platform::Object^, RacingWheel^ args)
    {
        m_currentWheelNeedsRefresh = true;
    });

    RacingWheel::RacingWheelRemoved += ref new EventHandler<RacingWheel^ >([=](Platform::Object^, RacingWheel^ args)
    {
        m_currentWheelNeedsRefresh = true;
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
void Sample::Update(DX::StepTimer const& )
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    if (m_currentNavNeedsRefresh)
    {
        auto mostRecentNav = InputManager::GetMostRecentNavController();
        if (m_currentNav != mostRecentNav)
        {
            m_currentNav = mostRecentNav;
        }
        m_currentNavNeedsRefresh = false;
    }

    if (m_currentNav == nullptr)
    {
        m_connected = false;
        m_currentMode = NavigationDevice;
        PIXEndEvent();
        return;
    }

    m_connected = true;

    if (m_currentWheelNeedsRefresh)
    {
        auto mostRecentWheel = InputManager::GetMostRecentRacingWheel();
        if (m_currentWheel != mostRecentWheel)
        {
            m_currentWheel = mostRecentWheel;
        }
        m_currentWheelNeedsRefresh = false;
    }

    if (m_currentStickNeedsRefresh)
    {
        auto mostRecentStick = InputManager::GetMostRecentArcadeStick();
        if (m_currentStick != mostRecentStick)
        {
            m_currentStick = mostRecentStick;
        }
        m_currentStickNeedsRefresh = false;
    }

    m_navReading = m_currentNav->GetCurrentReading();

    if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::View))
    {
        Windows::ApplicationModel::Core::CoreApplication::Exit();
    }

    if (!m_selectPressed)
    {
        if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::Right))
        {
            m_selectPressed = true;
            m_currentMode = (Modes)((m_currentMode + 1) % _countof(c_InputTestNames));
        }
        else if (static_cast<int>(m_navReading.RequiredButtons & RequiredUINavigationButtons::Left))
        {
            m_selectPressed = true;
            if (m_currentMode == NavigationDevice)
            {
                m_currentMode = RacingWheelDevice;
            }
            else
            {
                m_currentMode = (Modes)(m_currentMode - 1);
            }
        }
    }
    else
    {
        if (static_cast<int>(m_navReading.RequiredButtons & (RequiredUINavigationButtons::Right | RequiredUINavigationButtons::Left)) == 0)
        {
            m_selectPressed = false;
        }
    }

    switch (m_currentMode)
    {
    case NavigationDevice:
        GenerateNavString();
        break;
    case ArcadeStickDevice:
        if (m_currentStick != nullptr)
        {
            m_arcadeReading = m_currentStick->GetCurrentReading();
            GenerateStickString();
        }
        break;
    case RacingWheelDevice:
        if (m_currentWheel != nullptr)
        {
            m_wheelReading = m_currentWheel->GetCurrentReading();
        }
        break;
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

    if (m_connected)
    {
        // Draw description
        m_font->DrawString(m_spriteBatch.get(), c_InputTestNames[m_currentMode], pos, ATG::Colors::White);
        pos.y += m_font->GetLineSpacing() * 1.5f;
        
        switch (m_currentMode)
        {
        case 0:
            m_font->DrawString(m_spriteBatch.get(), c_NavDescription, pos, ATG::Colors::OffWhite);
            pos.y += (m_font->GetLineSpacing() * 1.5f) * 2.f;

            if (!m_buttonString.empty())
            {
                m_font->DrawString(m_spriteBatch.get(), m_buttonString.c_str(), pos, ATG::Colors::Green);
            }
            break;
        case 1:
            if (m_currentStick != nullptr)
            {
                if (!m_buttonString.empty())
                {
                    m_font->DrawString(m_spriteBatch.get(), m_buttonString.c_str(), pos, ATG::Colors::Green);
                }
            }
            else
            {
                m_font->DrawString(m_spriteBatch.get(), L"No arcade stick connected", pos, ATG::Colors::Orange);
            }
            break;
        case 2:
            if (m_currentWheel != nullptr)
            {
                DrawWheel(pos);
            }
            else
            {
                m_font->DrawString(m_spriteBatch.get(), L"No wheel connected", pos, ATG::Colors::Orange);
            }
            break;
        }
    }
    else
    {
        m_font->DrawString(m_spriteBatch.get(), L"No navigation input connected", pos, ATG::Colors::Orange);
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
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

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
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}

void Sample::OnDeviceLost()
{
    m_spriteBatch.reset();
    m_font.reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
