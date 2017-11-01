//--------------------------------------------------------------------------------------
// InputInterfacingUWP.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "InputInterfacingUWP.h"
#include <ppltasks.h>

#include "ATGColors.h"
#include <Windows.Gaming.Input.h>

extern void ExitSample();

using namespace DirectX;
using namespace Windows::Gaming::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Platform::Collections;

using Microsoft::WRL::ComPtr;

namespace
{
    static const wchar_t* c_InputTestNames[] =
    {
        L"<Navigation Test>\n",
        L"<ArcadeStick Test>\n",
        L"<RacingWheel Test>\n",
		L"<FlightStick Test>\n",
	};

    static const wchar_t c_NavDescription[] =
        L"Using the UINavigationController allows you to read generic navigation \n"
        L"commands from a variety of input devices like wheels, gamepads, and \n"
        L"arcade sticks\n";
}

void Sample::GenerateNavString()
{
    m_buttonString = L"Nav inputs pressed:  ";

    if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::Up) == RequiredUINavigationButtons::Up)
    {
        m_buttonString += L"Up ";
    }

    if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::Down) == RequiredUINavigationButtons::Down)
    {
        m_buttonString += L"Down ";
    }

    if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::Left) == RequiredUINavigationButtons::Left)
    {
        m_buttonString += L"Left ";
    }

    if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::Right) == RequiredUINavigationButtons::Right)
    {
        m_buttonString += L"Right ";
    }

    if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::Accept) == RequiredUINavigationButtons::Accept)
    {
        m_buttonString += L"Accept ";
    }

    if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::Cancel) == RequiredUINavigationButtons::Cancel)
    {
        m_buttonString += L"Cancel ";
    }

    if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::Menu) == RequiredUINavigationButtons::Menu)
    {
        m_buttonString += L"Menu ";
    }

    if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::View) == RequiredUINavigationButtons::View)
    {
        m_buttonString += L"View ";
    }
}

void Sample::GenerateStickString()
{
    m_buttonString = L"Arcade Stick inputs pressed:  ";

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::StickUp) == ArcadeStickButtons::StickUp)
    {
        m_buttonString += L"Up ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::StickDown) == ArcadeStickButtons::StickDown)
    {
        m_buttonString += L"Down ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::StickLeft) == ArcadeStickButtons::StickLeft)
    {
        m_buttonString += L"Left ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::StickRight) == ArcadeStickButtons::StickRight)
    {
        m_buttonString += L"Right ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::Action1) == ArcadeStickButtons::Action1)
    {
        m_buttonString += L"1 ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::Action2) == ArcadeStickButtons::Action2)
    {
        m_buttonString += L"2 ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::Action3) == ArcadeStickButtons::Action3)
    {
        m_buttonString += L"3 ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::Action4) == ArcadeStickButtons::Action4)
    {
        m_buttonString += L"4 ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::Action5) == ArcadeStickButtons::Action5)
    {
        m_buttonString += L"5 ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::Action6) == ArcadeStickButtons::Action6)
    {
        m_buttonString += L"6 ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::Special1) == ArcadeStickButtons::Special1)
    {
        m_buttonString += L"S1 ";
    }

    if ((m_arcadeReading.Buttons & ArcadeStickButtons::Special2) == ArcadeStickButtons::Special2)
    {
        m_buttonString += L"S2 ";
    }
}

void Sample::DrawFlightStick(DirectX::XMFLOAT2 startPosition)
{
	std::wstring localButtonString = L"Flight Stick inputs pressed:  ";
	wchar_t stickString[128] = {};

	if ((m_flightStickReading.Buttons & FlightStickButtons::FirePrimary) == FlightStickButtons::FirePrimary)
	{
		localButtonString += L"FirePrimary ";
	}

	if ((m_flightStickReading.Buttons & FlightStickButtons::FireSecondary) == FlightStickButtons::FireSecondary)
	{
		localButtonString += L"FireSecondary ";
	}
	
	if ((m_flightStickReading.HatSwitch & GameControllerSwitchPosition::Up) == GameControllerSwitchPosition::Up)
	{
		localButtonString += L"HatUp ";
	}

	if ((m_flightStickReading.HatSwitch & GameControllerSwitchPosition::UpRight) == GameControllerSwitchPosition::UpRight)
	{
		localButtonString += L"HatUpRight ";
	}

	if ((m_flightStickReading.HatSwitch & GameControllerSwitchPosition::Right) == GameControllerSwitchPosition::Right)
	{
		localButtonString += L"HatRight ";
	}

	if ((m_flightStickReading.HatSwitch & GameControllerSwitchPosition::DownRight) == GameControllerSwitchPosition::DownRight)
	{
		localButtonString += L"HatDownRight ";
	}

	if ((m_flightStickReading.HatSwitch & GameControllerSwitchPosition::Down) == GameControllerSwitchPosition::Down)
	{
		localButtonString += L"HatDown ";
	}

	if ((m_flightStickReading.HatSwitch & GameControllerSwitchPosition::DownLeft) == GameControllerSwitchPosition::DownLeft)
	{
		localButtonString += L"HatDownLeft ";
	}

	if ((m_flightStickReading.HatSwitch & GameControllerSwitchPosition::Left) == GameControllerSwitchPosition::Left)
	{
		localButtonString += L"HatLeft ";
	}

	if ((m_flightStickReading.HatSwitch & GameControllerSwitchPosition::UpLeft) == GameControllerSwitchPosition::UpLeft)
	{
		localButtonString += L"HatUpLeft ";
	}

	m_font->DrawString(m_spriteBatch.get(), localButtonString.c_str(), startPosition, ATG::Colors::Green);
	startPosition.y += m_font->GetLineSpacing() * 1.1f;

	swprintf_s(stickString, L"Roll %1.3f", m_flightStickReading.Roll);
	m_font->DrawString(m_spriteBatch.get(), stickString, startPosition, ATG::Colors::Green);
	startPosition.y += m_font->GetLineSpacing() * 1.1f;

	swprintf_s(stickString, L"Pitch %1.3f", m_flightStickReading.Pitch);
	m_font->DrawString(m_spriteBatch.get(), stickString, startPosition, ATG::Colors::Green);
	startPosition.y += m_font->GetLineSpacing() * 1.1f;

	swprintf_s(stickString, L"Yaw %1.3f", m_flightStickReading.Yaw);
	m_font->DrawString(m_spriteBatch.get(), stickString, startPosition, ATG::Colors::Green);
	startPosition.y += m_font->GetLineSpacing() * 1.1f;

	swprintf_s(stickString, L"Throttle %1.3f", m_flightStickReading.Throttle);
	m_font->DrawString(m_spriteBatch.get(), stickString, startPosition, ATG::Colors::Green);
	startPosition.y += m_font->GetLineSpacing() * 1.1f;
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

UINavigationController^ Sample::GetFirstNavController()
{
    UINavigationController^ navController = nullptr;

    if (m_navCollection->Size > 0)
    {
        navController = m_navCollection->GetAt(0);
    }

    return navController;
}

ArcadeStick^ Sample::GetFirstArcadeStick()
{
    ArcadeStick^ stick = nullptr;

    if (m_stickCollection->Size > 0)
    {
        stick = m_stickCollection->GetAt(0);
    }

    return stick;
}

RacingWheel^ Sample::GetFirstWheel()
{
    RacingWheel^ wheel = nullptr;

    if (m_wheelCollection->Size > 0)
    {
        wheel = m_wheelCollection->GetAt(0);
    }

    return wheel;
}

FlightStick^ Sample::GetFirstFlightStick()
{
	FlightStick^ stick = nullptr;

	if (m_flightStickCollection->Size > 0)
	{
		stick = m_flightStickCollection->GetAt(0);
	}

	return stick;
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
    m_effectLoaded = false;

    //Create an effect
    m_effect = ref new ForceFeedback::ConstantForceEffect();
    TimeSpan time;
    time.Duration = 10000;
    Numerics::float3 vector;
    vector.x = 1.f;
    vector.y = 0.f;
    vector.z = 0.f;
    m_effect->SetParameters(vector, time);

    m_navCollection = ref new Vector<UINavigationController^>();
    m_stickCollection = ref new Vector<ArcadeStick^>();
    m_wheelCollection = ref new Vector<RacingWheel^>();
	m_flightStickCollection = ref new Vector<FlightStick^>();

    auto navControllers = UINavigationController::UINavigationControllers;
    for (auto controller : navControllers)
    {
        m_navCollection->Append(controller);
    }

    auto stickControllers = ArcadeStick::ArcadeSticks;
    for (auto controller : stickControllers)
    {
        m_stickCollection->Append(controller);
    }

    auto wheelControllers = RacingWheel::RacingWheels;
    for (auto controller : wheelControllers)
    {
        m_wheelCollection->Append(controller);
    }

	auto flightStickControllers = FlightStick::FlightSticks;
	for (auto controller : flightStickControllers)
	{
		m_flightStickCollection->Append(controller);
	}

	UINavigationController::UINavigationControllerAdded += ref new EventHandler<UINavigationController^ >([=](Platform::Object^, UINavigationController^ args)
    {
        m_navCollection->Append(args);
        m_currentNavNeedsRefresh = true;
    });

    UINavigationController::UINavigationControllerRemoved += ref new EventHandler<UINavigationController^ >([=](Platform::Object^, UINavigationController^ args)
    {
        unsigned int index;
        if (m_navCollection->IndexOf(args, &index))
        {
            m_navCollection->RemoveAt(index);
            m_currentNavNeedsRefresh = true;
        }
    });

    ArcadeStick::ArcadeStickAdded += ref new EventHandler<ArcadeStick^ >([=](Platform::Object^, ArcadeStick^ args)
    {
        m_stickCollection->Append(args);
        m_currentStickNeedsRefresh = true;
    });

    ArcadeStick::ArcadeStickRemoved += ref new EventHandler<ArcadeStick^ >([=](Platform::Object^, ArcadeStick^ args)
    {
        unsigned int index;
        if (m_stickCollection->IndexOf(args, &index))
        {
            m_stickCollection->RemoveAt(index);
            m_currentStickNeedsRefresh = true;
        }
    });

    RacingWheel::RacingWheelAdded += ref new EventHandler<RacingWheel^ >([=](Platform::Object^, RacingWheel^ args)
    {
        m_wheelCollection->Append(args);
        m_currentWheelNeedsRefresh = true;
    });

    RacingWheel::RacingWheelRemoved += ref new EventHandler<RacingWheel^ >([=](Platform::Object^, RacingWheel^ args)
    {
        unsigned int index;
        if (m_wheelCollection->IndexOf(args, &index))
        {
            m_wheelCollection->RemoveAt(index);
            m_currentWheelNeedsRefresh = true;
        }
    });

	FlightStick::FlightStickAdded += ref new EventHandler<FlightStick^ >([=](Platform::Object^, FlightStick^ args)
	{
		m_flightStickCollection->Append(args);
		m_currentFlightStickNeedsRefresh = true;
	});

	FlightStick::FlightStickRemoved += ref new EventHandler<FlightStick^ >([=](Platform::Object^, FlightStick^ args)
	{
		unsigned int index;
		if (m_flightStickCollection->IndexOf(args, &index))
		{
			m_flightStickCollection->RemoveAt(index);
			m_currentFlightStickNeedsRefresh = true;
		}
	});
	
	m_currentNav = GetFirstNavController();
    m_currentStick = GetFirstArcadeStick();
    m_currentWheel = GetFirstWheel();
	m_currentFlightStick = GetFirstFlightStick();
	m_currentNavNeedsRefresh = false;
    m_currentWheelNeedsRefresh = false;
    m_currentStickNeedsRefresh = false;
	m_currentFlightStickNeedsRefresh = false;

    if (m_currentWheel != nullptr && m_currentWheel->WheelMotor != nullptr)
    {
        IAsyncOperation<ForceFeedback::ForceFeedbackLoadEffectResult>^ request = m_currentWheel->WheelMotor->LoadEffectAsync(m_effect);

        auto loadEffectTask = Concurrency::create_task(request);
        loadEffectTask.then([this](ForceFeedback::ForceFeedbackLoadEffectResult result)
        {
            if (result == ForceFeedback::ForceFeedbackLoadEffectResult::Succeeded)
            {
                m_effectLoaded = true;
            }
            else
            {
                m_effectLoaded = false;
            }
        }).wait();
    }

    if (m_effectLoaded)
    {
        m_effect->Start();
    }

    // UWP on Xbox One triggers a back request whenever the B button is pressed
    // which can result in the app being suspended if unhandled
    using namespace Windows::UI::Core;

    auto navigation = SystemNavigationManager::GetForCurrentView();

    navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>([](Platform::Object^, BackRequestedEventArgs^ args)
    {
        args->Handled = true;
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

    bool toggleFFB = false;

    if (m_currentNavNeedsRefresh)
    {
        auto mostRecentNav = GetFirstNavController();
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
        auto mostRecentWheel = GetFirstWheel();
        if (m_currentWheel != mostRecentWheel)
        {
            m_currentWheel = mostRecentWheel;
        }
        m_currentWheelNeedsRefresh = false;


        if (m_currentWheel != nullptr && m_currentWheel->WheelMotor != nullptr)
        {
            IAsyncOperation<ForceFeedback::ForceFeedbackLoadEffectResult>^ request = m_currentWheel->WheelMotor->LoadEffectAsync(m_effect);

            auto loadEffectTask = Concurrency::create_task(request);
            loadEffectTask.then([this](ForceFeedback::ForceFeedbackLoadEffectResult result)
            {
                if (result == ForceFeedback::ForceFeedbackLoadEffectResult::Succeeded)
                {
                    m_effectLoaded = true;
                }
                else
                {
                    m_effectLoaded = false;
                }
            }).wait();
        }

        if (m_effectLoaded)
        {
            m_effect->Start();
        }
    }

    if (m_currentStickNeedsRefresh)
    {
        auto mostRecentStick = GetFirstArcadeStick();
        if (m_currentStick != mostRecentStick)
        {
            m_currentStick = mostRecentStick;
        }
        m_currentStickNeedsRefresh = false;
    }

	if (m_currentFlightStickNeedsRefresh)
	{
		auto mostRecentFlightStick = GetFirstFlightStick();
		if (m_currentFlightStick != mostRecentFlightStick)
		{
			m_currentFlightStick = mostRecentFlightStick;
		}
		m_currentFlightStickNeedsRefresh = false;
	}
	
	m_navReading = m_currentNav->GetCurrentReading();

    if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::View) == RequiredUINavigationButtons::View)
    {
        ExitSample();
    }

    if (!m_selectPressed)
    {
        if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::Right) == RequiredUINavigationButtons::Right)
        {
            m_selectPressed = true;
            m_currentMode = (Modes)((m_currentMode + 1) % _countof(c_InputTestNames));
        }
        else if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::Left) == RequiredUINavigationButtons::Left)
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
        else if ((m_navReading.RequiredButtons & RequiredUINavigationButtons::Accept) == RequiredUINavigationButtons::Accept)
        {
            toggleFFB = true;
        }
    }
    else
    {
        if ((m_navReading.RequiredButtons & (RequiredUINavigationButtons::Right | RequiredUINavigationButtons::Left | RequiredUINavigationButtons::Accept)) == RequiredUINavigationButtons::None)
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
	case FlightStickDevice:
		if (m_currentFlightStick != nullptr)
		{
			m_flightStickReading = m_currentFlightStick->GetCurrentReading();
		}
		break;
	case RacingWheelDevice:
        if (m_currentWheel != nullptr)
        {
            m_wheelReading = m_currentWheel->GetCurrentReading();

            if(m_effectLoaded && toggleFFB)
            {
                if (m_effect->State == ForceFeedback::ForceFeedbackEffectState::Running)
                {
                    m_effect->Stop();
                }
                else
                {
                    m_effect->Start();
                }
            }
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
		case 3:
			if (m_currentFlightStick != nullptr)
			{
				DrawFlightStick(pos);
			}
			else
			{
				m_font->DrawString(m_spriteBatch.get(), L"No flight stick connected", pos, ATG::Colors::Orange);
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
    auto renderTarget = m_deviceResources->GetRenderTargetView();
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
