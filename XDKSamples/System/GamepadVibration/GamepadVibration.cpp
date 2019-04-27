//--------------------------------------------------------------------------------------
// GamepadVibration.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "GamepadVibration.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

using namespace Windows::Xbox::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

namespace
{
    static const wchar_t* c_TRIGGER_EFFECT_NAME_TEXT[Sample::TRIGGEREFFECTS_MAX] =
    {
        L"<Trigger Test>\n",
        L"<Flat Tire>\n",
        L"<Gun with Recoil>\n",
        L"<Heartbeat>\n",
        L"<Footsteps>\n"
    };

    static const wchar_t* c_TRIGGER_EFFECT_DESC_TEXT[Sample::TRIGGEREFFECTS_MAX] =
    {
        L"Use the [LT] and [RT] to test the feedback\n"
        L"function of the gamepad. The envelope is set based on\n"
        L"the trigger position. The more you pull the triggers,\n"
        L"the more feedback you will feel.",

        L"Impulse triggers can provide feedback about the environment.\n"
        L"Assuming the player is driving a car, this example uses\n"
        L"the impulse triggers to inform a flat tire on the left side.",

        L"Demonstrates how impulse triggers can be combined with the\n"
        L"vibration motors to simulate weapon firing and recoil.\n"
        L"Press the [LT] to activate the effect.",

        L"Impulse triggers can relay information about the player\'s\n"
        L"in-game representation. Here we relay the character\'s\n"
        L"heartbeat, which can be used to let the player know that\n"
        L"their character is exhausted.",

        L"Impulse triggers can relay information external to the\n"
        L"player. This example use the impulse triggers to simulate\n"
        L"footsteps which could indicate the presence of a nearby\n"
        L"character."
    };

    uint32_t flatTireLeftTriggerDurations[] = { 33, 80, 16 };
    float flatTireLeftTriggerLevels[] = { 0.8f, 0.0f, 0.0f };

    uint32_t gunWithRecoilLeftTriggerDurations[] = { 20, 10, 90, 10000 };
    float gunWithRecoilLeftTriggerLevels[] = { 1.0f, 0.0f, 0.0f, 0.0f };

    uint32_t heartbeatLeftTriggerDurations[] = { 25, 200, 25, 10, 745 };
    float heartbeatLeftTriggerLevels[] = { 0.2f, 0.0f, 0.0f, 0.0f, 0.0f };
    uint32_t heartbeatRightTriggerDurations[] = { 25, 200, 25, 10, 745 };
    float heartbeatRightTriggerLevels[] = { 0.0f, 0.0f, 0.2f, 0.02f, 0.0f };

    uint32_t footstepsLeftTriggerDurations[] = { 25, 600, 25, 600 };
    float footstepsLeftTriggerLevels[] = { 0.3f, 0.0f, 0.0f, 0.0f };
    uint32_t footstepsRightTriggerDurations[] = { 25, 600, 25, 600 };
    float footstepsRightTriggerLevels[] = { 0.0f, 0.0f, 0.3f, 0.0f };
}

//--------------------------------------------------------------------------------------
// Helper class for managing the gamepads
//--------------------------------------------------------------------------------------
namespace GamepadManager
{
    IGamepad^ GetMostRecentGamepad()
    {
        IGamepad^ gamepad = nullptr;

        auto allGamepads = Gamepad::Gamepads;
        if (allGamepads->Size > 0)
        {
            gamepad = allGamepads->GetAt(0);
        }

        return gamepad;
    }

    bool IsGamepadValid(IGamepad^ gamepad)
    {
        IIterator<IGamepad^>^ it = Gamepad::Gamepads->First();

        while (it->HasCurrent)
        {
            if (gamepad == it->Current)
            {
                return true;
            }

            it->MoveNext();
        }

        return false;
    }
};

Sample::Sample() noexcept(false) : 
    m_currentGamepadNeedsRefresh(false),
    m_connected(false),
    m_leftMotorSpeed(0),
    m_leftTriggerLevel(0),
    m_rightMotorSpeed(0),
    m_rightTriggerLevel(0),
    m_dPadPressed(false),
    m_selectedTriggerEffect(TRIGGEREFFECTS::TRIGGEREFFECTS_IMPULSETEST),
    m_triggerEffectCounter(0),
    m_leftTriggerArraySize(0),
    m_pLeftTriggerDurations(nullptr),
    m_pLeftTriggerLevels(nullptr),
    m_rightTriggerArraySize(0),
    m_pRightTriggerDurations(nullptr),
    m_pRightTriggerLevels(nullptr),
    m_leftTriggerIndex(0),
    m_rightTriggerIndex(0),
    m_frequency(0),
    m_counter(0),
    m_leftTriggerIndexUpdateTime(0),
    m_rightTriggerIndexUpdateTime(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_selectedTriggerEffect = TRIGGEREFFECTS::TRIGGEREFFECTS_IMPULSETEST;

    m_currentGamepad = GamepadManager::GetMostRecentGamepad();
    m_currentGamepadNeedsRefresh = false;
    m_connected = false;

    Gamepad::GamepadAdded += ref new EventHandler<GamepadAddedEventArgs^ >([=](Platform::Object^, GamepadAddedEventArgs^ args)
    {
        m_currentGamepadNeedsRefresh = true;
    });

    Gamepad::GamepadRemoved += ref new EventHandler<GamepadRemovedEventArgs^ >([=](Platform::Object^, GamepadRemovedEventArgs^ args)
    {
        m_currentGamepadNeedsRefresh = true;
    });

    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER *>(&m_frequency));
}

void Sample::InitializeCurrentGamepad()
{
    if (m_currentGamepad)
    {
        m_reading = m_currentGamepad->GetCurrentReading();

        ZeroMemory(&m_vibration, sizeof(GamepadVibration));

        m_dPadPressed = false;

        m_selectedTriggerEffect = TRIGGEREFFECTS::TRIGGEREFFECTS_IMPULSETEST;
        InitializeImpulseTriggerEffects();
    }
}

void Sample::ShutdownCurrentGamepad()
{
    if (m_currentGamepad && GamepadManager::IsGamepadValid(m_currentGamepad))
    {
        GamepadVibration vibration = {};
        m_currentGamepad->SetVibration(vibration);
    }
}

//--------------------------------------------------------------------------------------
// Name: InitializeImpulseTriggerEffects()
// Desc: Clear variables used by the tigger effects and initialize them as needed for 
//       the currently selected effect.
//--------------------------------------------------------------------------------------
void Sample::InitializeImpulseTriggerEffects()
{
    m_leftTriggerIndex = 0;
    m_rightTriggerIndex = 0;

    m_leftMotorSpeed = 0;
    m_leftTriggerLevel = 0;
    m_rightMotorSpeed = 0;
    m_rightTriggerLevel = 0;

    m_triggerEffectCounter = 0;
    switch (m_selectedTriggerEffect)
    {
    case TRIGGEREFFECTS::TRIGGEREFFECTS_IMPULSETEST:
        break;

    case TRIGGEREFFECTS::TRIGGEREFFECTS_FLATTIRE:
        m_leftTriggerArraySize = 3;

        m_pLeftTriggerDurations = flatTireLeftTriggerDurations;
        m_pLeftTriggerLevels = flatTireLeftTriggerLevels;

        // Set the timing for the transition to the second vibration level
        // Further transition timings will be handled by the transition code.
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&m_counter));
        m_leftTriggerIndexUpdateTime = m_counter + (m_frequency * m_pLeftTriggerDurations[m_leftTriggerIndex]) / 1000;
        break;

    case TRIGGEREFFECTS::TRIGGEREFFECTS_GUNWITHRECOIL:
        m_leftTriggerArraySize = 4;

        m_pLeftTriggerDurations = gunWithRecoilLeftTriggerDurations;
        m_pLeftTriggerLevels = gunWithRecoilLeftTriggerLevels;
        break;

    case TRIGGEREFFECTS::TRIGGEREFFECTS_HEARTBEAT:
        m_leftTriggerArraySize = 5;
        m_rightTriggerArraySize = 5;

        m_pLeftTriggerDurations = heartbeatLeftTriggerDurations;
        m_pLeftTriggerLevels = heartbeatLeftTriggerLevels;
        m_pRightTriggerDurations = heartbeatRightTriggerDurations;
        m_pRightTriggerLevels = heartbeatRightTriggerLevels;

        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&m_counter));
        m_leftTriggerIndexUpdateTime = m_counter + (m_frequency * m_pLeftTriggerDurations[m_leftTriggerIndex]) / 1000;
        m_rightTriggerIndexUpdateTime = m_counter + (m_frequency * m_pRightTriggerDurations[m_rightTriggerIndex]) / 1000;
        break;

    case TRIGGEREFFECTS::TRIGGEREFFECTS_FOOTSTEPS:
        m_leftTriggerArraySize = 4;
        m_rightTriggerArraySize = 4;

        m_pLeftTriggerDurations = footstepsLeftTriggerDurations;
        m_pLeftTriggerLevels = footstepsLeftTriggerLevels;
        m_pRightTriggerDurations = footstepsRightTriggerDurations;
        m_pRightTriggerLevels = footstepsRightTriggerLevels;

        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&m_counter));
        m_leftTriggerIndexUpdateTime = m_counter + (m_frequency * m_pLeftTriggerDurations[m_leftTriggerIndex]) / 1000;
        m_rightTriggerIndexUpdateTime = m_counter + (m_frequency * m_pRightTriggerDurations[m_rightTriggerIndex]) / 1000;
        break;

    default:
        assert(false);
    }
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
        auto mostRecentGamepad = GamepadManager::GetMostRecentGamepad();
        if (m_currentGamepad != mostRecentGamepad)
        {
            ShutdownCurrentGamepad();
            m_currentGamepad = mostRecentGamepad;
            InitializeCurrentGamepad();
        }
        m_currentGamepadNeedsRefresh = false;
    }

    if (m_currentGamepad == nullptr)
    {
        m_connected = false;
        PIXEndEvent();
        return;
    }

    m_connected = true;

    m_reading = m_currentGamepad->GetCurrentReading();

    if (m_reading->IsViewPressed)
    {
        ExitSample();
    }

    if (!m_dPadPressed)
    {
        if (m_reading->IsDPadRightPressed)
        {
            m_dPadPressed = true;
            m_selectedTriggerEffect = static_cast<TRIGGEREFFECTS>((static_cast<int>(m_selectedTriggerEffect) + 1) % TRIGGEREFFECTS_MAX);
            InitializeImpulseTriggerEffects();
        }
        else if (m_reading->IsDPadLeftPressed)
        {
            m_dPadPressed = true;
            m_selectedTriggerEffect = static_cast<TRIGGEREFFECTS>((static_cast<int>(m_selectedTriggerEffect) + TRIGGEREFFECTS_MAX - 1) % TRIGGEREFFECTS_MAX);
            InitializeImpulseTriggerEffects();
        }
    }
    else
    {
        if (static_cast<int>(m_reading->Buttons & (GamepadButtons::DPadRight | GamepadButtons::DPadLeft)) == 0)
        {
            m_dPadPressed = false;
        }
    }

    switch (m_selectedTriggerEffect)
    {
    case TRIGGEREFFECTS::TRIGGEREFFECTS_IMPULSETEST:
        // This example uses a very simple vibration envelope waveform by setting the vibration
        // levels to the current trigger values. This means the more you pull the triggers, the more
        // vibration you will feel.
        m_leftTriggerLevel = m_reading->LeftTrigger;
        m_rightTriggerLevel = m_reading->RightTrigger;
        m_leftMotorSpeed = m_reading->LeftTrigger;
        m_rightMotorSpeed = m_reading->RightTrigger;
        break;

    case TRIGGEREFFECTS::TRIGGEREFFECTS_FLATTIRE:
        m_leftTriggerLevel = m_pLeftTriggerLevels[m_leftTriggerIndex];

        // If we've reached or passed the transition time, update m_leftTriggerIndexUpdateTime
        // with the next transition time and update the effect index.
        // This will cause the effect to change in the next loop iteration.
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&m_counter));
        if (m_counter > m_leftTriggerIndexUpdateTime)
        {
            m_leftTriggerIndex = (m_leftTriggerIndex + 1) % m_leftTriggerArraySize;
            m_leftTriggerIndexUpdateTime = m_counter + (m_frequency * m_pLeftTriggerDurations[m_leftTriggerIndex]) / 1000;
        }
        break;

    case TRIGGEREFFECTS::TRIGGEREFFECTS_GUNWITHRECOIL:
        switch (m_triggerEffectCounter)
        {
        case 0: // Wait for the trigger to be fully released before
                // the effect can begin
            if (m_reading->LeftTrigger <= 1.0f / 255.0f)
            {
                m_triggerEffectCounter = 1;
            }
            break;

        case 1: // Wait for the trigger to be depressed enough to cause the gun to fire
            if (m_reading->LeftTrigger >= 32.0f / 255.0f)
            {
                QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&m_counter));
                m_leftTriggerIndexUpdateTime = m_counter + (m_frequency * m_pLeftTriggerDurations[m_leftTriggerIndex]) / 1000;
                m_triggerEffectCounter = 2;
            }
            break;

        case 2: // Delay recoil a little after the bullet has left the gun
            m_leftTriggerLevel = m_pLeftTriggerLevels[m_leftTriggerIndex];

            if (m_leftTriggerIndex == 2)
            {
                m_leftMotorSpeed = 1.0f;
                m_rightMotorSpeed = 1.0f;
            }
            else
            {
                m_leftMotorSpeed = 0.0f;
                m_rightMotorSpeed = 0.0f;
            }

            if (m_leftTriggerIndex == 3)
            {
                m_leftTriggerIndex = 0;
                m_triggerEffectCounter = 0;
                break;
            }

            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&m_counter));
            if (m_counter > m_leftTriggerIndexUpdateTime)
            {
                m_leftTriggerIndex = (m_leftTriggerIndex + 1) % m_leftTriggerArraySize;
                m_leftTriggerIndexUpdateTime = m_counter + (m_frequency * m_pLeftTriggerDurations[m_leftTriggerIndex]) / 1000;
            }
            break;
        }
        break;

    case TRIGGEREFFECTS::TRIGGEREFFECTS_HEARTBEAT:
        // use the left level/duration for both triggers
        m_leftTriggerLevel = m_pLeftTriggerLevels[m_leftTriggerIndex];
        m_rightTriggerLevel = m_pRightTriggerLevels[m_rightTriggerIndex];

        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&m_counter));
        if (m_counter > m_leftTriggerIndexUpdateTime)
        {
            m_leftTriggerIndex = (m_leftTriggerIndex + 1) % m_leftTriggerArraySize;
            m_leftTriggerIndexUpdateTime = m_counter + (m_frequency * m_pLeftTriggerDurations[m_leftTriggerIndex]) / 1000;
        }
        if (m_counter > m_rightTriggerIndexUpdateTime)
        {
            m_rightTriggerIndex = (m_rightTriggerIndex + 1) % m_rightTriggerArraySize;
            m_rightTriggerIndexUpdateTime = m_counter + (m_frequency * m_pRightTriggerDurations[m_rightTriggerIndex]) / 1000;
        }
        break;

    case TRIGGEREFFECTS::TRIGGEREFFECTS_FOOTSTEPS:
        m_leftTriggerLevel = m_pLeftTriggerLevels[m_leftTriggerIndex];
        m_rightTriggerLevel = m_pRightTriggerLevels[m_rightTriggerIndex];

        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&m_counter));
        if (m_counter > m_leftTriggerIndexUpdateTime)
        {
            m_leftTriggerIndex = (m_leftTriggerIndex + 1) % m_leftTriggerArraySize;
            m_leftTriggerIndexUpdateTime = m_counter + (m_frequency * m_pLeftTriggerDurations[m_leftTriggerIndex]) / 1000;
        }
        if (m_counter > m_rightTriggerIndexUpdateTime)
        {
            m_rightTriggerIndex = (m_rightTriggerIndex + 1) % m_rightTriggerArraySize;
            m_rightTriggerIndexUpdateTime = m_counter + (m_frequency * m_pRightTriggerDurations[m_rightTriggerIndex]) / 1000;
        }
        break;

    default:
        assert(false);
    }

    m_vibration.LeftMotorLevel = m_leftMotorSpeed;
    m_vibration.RightMotorLevel = m_rightMotorSpeed;
    m_vibration.LeftTriggerLevel = m_leftTriggerLevel;
    m_vibration.RightTriggerLevel = m_rightTriggerLevel;
    m_currentGamepad->SetVibration(m_vibration);

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

    m_spriteBatch->Begin();
    m_spriteBatch->Draw(m_background.Get(), m_deviceResources->GetOutputSize());

    if (m_connected)
    {
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(),
            L"Use the [DPad] Left and Right to select a vibration effect.", pos, ATG::Colors::OffWhite);
        pos.y += m_font->GetLineSpacing() * 2.f;

        // Draw description
        m_font->DrawString(m_spriteBatch.get(), c_TRIGGER_EFFECT_NAME_TEXT[static_cast<int>(m_selectedTriggerEffect)], pos, ATG::Colors::Green);
        pos.y += m_font->GetLineSpacing() * 1.5f;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), c_TRIGGER_EFFECT_DESC_TEXT[static_cast<int>(m_selectedTriggerEffect)], pos);
    }
    else
    {
        m_font->DrawString(m_spriteBatch.get(), L"No controller connected", pos, ATG::Colors::Orange);
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
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"gamepad.dds", nullptr, m_background.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion
