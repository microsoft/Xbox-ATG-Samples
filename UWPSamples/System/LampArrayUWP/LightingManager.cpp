//--------------------------------------------------------------------------------------
// LightingManager.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <vector>
#include <time.h>
#include <ppltasks.h>

using namespace Platform;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation;
using namespace Windows::Devices::Lights;
using namespace Windows::Devices::Lights::Effects;
using namespace Windows::Devices::Enumeration;
using namespace Windows::UI;
using namespace Windows::System;

LightingManager^ LightingManager::s_instance = nullptr;
Microsoft::WRL::Wrappers::CriticalSection LightingManager::m_initLock;

// All Effects use Ticks; below multiplers transform units to Ticks.
constexpr uint32_t c_secondsMultiplier = 10'000'000;
constexpr uint32_t c_millisecondsMultiplier = 10'000;

LightingManager::LightingManager()
{
}

LightingManager::~LightingManager()
{
}

void LightingManager::Initialize()
{
    srand((UINT)time(nullptr));

    // Getting a reference to LightingManager the first time will initialize it.
    LightingManager::GetInstance();
}

LightingManager^ LightingManager::GetInstance()
{
    auto lock = m_initLock.Lock();

    // Implementing the singleton pattern; only need one manager for any application.
    if (s_instance == nullptr)
    {
        s_instance = ref new LightingManager();

        s_instance->InitializeInternal();
    }
    return s_instance;
}

void LightingManager::InitializeInternal()
{
    LampArray^ temp;
    m_lampArrayWatcher = DeviceInformation::CreateWatcher(temp->GetDeviceSelector());

    m_lampArrayWatcher->Added += ref new TypedEventHandler<DeviceWatcher^, DeviceInformation^>(this, &LightingManager::LampArrayAdded);
    m_lampArrayWatcher->Removed += ref new TypedEventHandler<DeviceWatcher^, DeviceInformationUpdate^>(this, &LightingManager::LampArrayRemoved);
    m_lampArrayWatcher->EnumerationCompleted += ref new TypedEventHandler<DeviceWatcher^, Object^>(this, &LightingManager::LampArrayWatcherEnumerationCompleted);
    m_lampArrayWatcher->Stopped += ref new TypedEventHandler<DeviceWatcher^, Object^>(this, &LightingManager::LampArrayWatcherStopped);

    m_lampArrayWatcher->Start();
}

void LightingManager::LampArrayAdded(DeviceWatcher^ watcher, DeviceInformation^ lampArray)
{
    {
        auto lock = m_lock.Lock();

        concurrency::create_task(LampArray::FromIdAsync(lampArray->Id)).then([this](LampArray ^ newDevice)
        {
            m_lampArrays.emplace_back(LampPair(newDevice, ref new LampArrayEffectPlaylist()));
        });
    }
}

void LightingManager::LampArrayRemoved(DeviceWatcher^ watcher, DeviceInformationUpdate^ args)
{
    auto lock = m_lock.Lock();

    for (auto instance = m_lampArrays.begin(); instance != m_lampArrays.end(); instance++)
    {
        if ((*instance).first->DeviceId == args->Id)
        {
            // Will only ever have a single instance with the same DeviceId (guaranteed by PNP)
            m_lampArrays.erase(instance);
            break;
        }
    }
}

void LightingManager::LampArrayWatcherEnumerationCompleted(DeviceWatcher^ watcher, Object^ args)
{
    // Triggered when the watcher has finished enumerating all devices (currently attached).
    // Don't care about this event as we'll trigger the same event whenever a device is plugged-in.
}

void LightingManager::LampArrayWatcherStopped(DeviceWatcher^ watcher, Object^ args)
{
    // Triggered when watcher Stop() has completed.
    // Always want to listen for LampArrays, don't care about stopped event completing
}

/// <summary>
/// Sets all Lamps on every known LampArray to desiredColor
/// </summary>
void LightingManager::SetLampArraysColor(Windows::UI::Color desiredColor)
{
    auto lock = m_lock.Lock();

    for (LampPair instance : m_lampArrays)
    {
        // Sets every Lamp on the LampArray to desiredColor.
        instance.first->SetColor(desiredColor);
    }
}

/// <summary>
/// Clears all known LampArrays (i.e. sets to black)
/// </summary>
void LightingManager::ClearLampArrays()
{
    auto lock = m_lock.Lock();

    for (SnakeEffect^ instance : m_snakeEffects)
    {
        instance->Stop();
    }

    for (LampPair instance : m_lampArrays)
    {
        instance.second->Stop();

        // Set all LampArrays to black/cleared.
        instance.first->SetColor(Windows::UI::Colors::Black);
    }
}

/// <summary>
/// Sets all control lamps on the left side of the LampArray to blue.
/// </summary>
void LightingManager::LeftControlLampsBlue()
{
    auto lock = m_lock.Lock();

    for (LampPair lampArray : m_lampArrays)
    {
        // LampArray Midpoint.
        float midPoint = lampArray.first->BoundingBox.y / 2;

        // Find all Lamps on the left-side of the device.
        std::vector<int> leftLampIndexes;
        for (int i = 0; i < lampArray.first->LampCount; i++)
        {
            LampInfo^ info = lampArray.first->GetLampInfo(i);

            bool isControlLamp = !!((int)info->Purposes & (int)LampPurposes::Control);

            if (isControlLamp && info->Position.x <= midPoint)
            {
                leftLampIndexes.push_back(i);
            }
        }

        auto blueIndexes = ref new Platform::Array<int>(leftLampIndexes.data(), (int)leftLampIndexes.size());
        lampArray.first->SetSingleColorForIndices({ 0xFF, 0x00, 0x00, 0xFF }, blueIndexes);
    }
}

/// <summary>
/// Sets the WASD keys to red (for all keyboard LampArrays) and all other keys to blue.
/// </summary>
void LightingManager::WasdKeysRed()
{
    auto lock = m_lock.Lock();

    for (LampPair lampArray : m_lampArrays)
    {
        // Validate LampArray is bound to a keyboard
        if (lampArray.first->LampArrayKind != LampArrayKind::Keyboard)
        {
            continue;
        }

        // Disable all lamps.  Calls to SetColor now won't have any effect until LampArray re-enabled.
        lampArray.first->IsEnabled = false;

        // Sets the base color for all lamps to blue
        lampArray.first->SetColor({ 0xFF, 0x00, 0x00, 0xFF });

        // Set the WASD keys (if they exist) to red. This will override the blue set previously.
        // Note: It's permissible for a key to have more than one lamp.
        lampArray.first->SetColorsForKey({ 0xFF, 0xFF, 0x00, 0x00 }, Windows::System::VirtualKey::W);
        lampArray.first->SetColorsForKey({ 0xFF, 0xFF, 0x00, 0x00 }, Windows::System::VirtualKey::A);
        lampArray.first->SetColorsForKey({ 0xFF, 0xFF, 0x00, 0x00 }, Windows::System::VirtualKey::S);
        lampArray.first->SetColorsForKey({ 0xFF, 0xFF, 0x00, 0x00 }, Windows::System::VirtualKey::D);

        // Enabling all lamps. The effects from SetColor will now be seen.
        lampArray.first->IsEnabled = true;
    }
}

/// <summary>
/// Blink W, A, S, D keyboard keys blue, 5 times, on for 1second, off for 1second.
/// </summary>
void LightingManager::BlinkWasdKeys()
{
    auto lock = m_lock.Lock();

    Windows::Foundation::TimeSpan spanSecond;
    spanSecond.Duration = c_secondsMultiplier;

    Windows::Foundation::TimeSpan spanZero;
    spanZero.Duration = 0;

    for (LampPair lampArray : m_lampArrays)
    {
        // Only care about keyboards for the case. (Note: Any device is permitted to have keys associated with it)
        if (lampArray.first->LampArrayKind != LampArrayKind::Keyboard)
        {
            continue;
        }

        std::vector<int> wasdKeys;

        Platform::Array<int>^ lampIndexes = lampArray.first->GetIndicesForKey(Windows::System::VirtualKey::W);
        wasdKeys.insert(wasdKeys.end(), lampIndexes->begin(), lampIndexes->end());

        lampIndexes = lampArray.first->GetIndicesForKey(Windows::System::VirtualKey::A);
        wasdKeys.insert(wasdKeys.end(), lampIndexes->begin(), lampIndexes->end());

        lampIndexes = lampArray.first->GetIndicesForKey(Windows::System::VirtualKey::S);
        wasdKeys.insert(wasdKeys.end(), lampIndexes->begin(), lampIndexes->end());

        lampIndexes = lampArray.first->GetIndicesForKey(Windows::System::VirtualKey::D);
        wasdKeys.insert(wasdKeys.end(), lampIndexes->begin(), lampIndexes->end());

        auto blinkLampIndexes = ref new Platform::Array<int>(wasdKeys.data(), (int)wasdKeys.size());

        // Properties of an effect are read-only after being Appended to a playlist.
        // Calls to set() will result in an error.
        auto blinkEffect = ref new LampArrayBlinkEffect(lampArray.first, blinkLampIndexes);
        blinkEffect->Color = Windows::UI::Colors::Blue;
        blinkEffect->ZIndex = 0;
        blinkEffect->SustainDuration = spanSecond;
        blinkEffect->DecayDuration = spanZero;
        blinkEffect->RepetitionDelay = spanSecond;
        blinkEffect->RepetitionMode = LampArrayRepetitionMode::Occurrences;
        blinkEffect->Occurrences = 5;

        // Create the playlist and Append the single effect to it.
        lampArray.second = ref new LampArrayEffectPlaylist();
        lampArray.second->RepetitionMode = LampArrayRepetitionMode::Occurrences;
        lampArray.second->Occurrences = 1;
        lampArray.second->EffectStartMode = LampArrayEffectStartMode::Simultaneous;
        lampArray.second->Append(blinkEffect);

        // Start the playlist.
        // A reference to the playlist will be maintained by the system until it completes (or is stopped manually).
        lampArray.second->Start();
    }
}

/// <summary>
/// Plays a green solid effect for 5 seconds.
/// </summary>
void LightingManager::PlayGreenSolidEffect()
{
    Windows::Foundation::TimeSpan span;
    span.Duration = 5 * c_secondsMultiplier;

    auto lock = m_lock.Lock();

    for (LampPair lampArray : m_lampArrays)
    {
        auto allLampIndexes = ref new Platform::Array<int>(lampArray.first->LampCount);
        for (int i = 0; i < lampArray.first->LampCount; i++)
        {
            allLampIndexes[i] = i;
        }

        auto greenEffect = ref new LampArraySolidEffect(lampArray.first, allLampIndexes);

        greenEffect->Color = Windows::UI::Colors::Lime;
        greenEffect->Duration = span;

        lampArray.second = ref new LampArrayEffectPlaylist();

        lampArray.second->Append(greenEffect);

        lampArray.second->Start();
    }
}

/// <summary>
/// Uses LampArrayColorRampEffect to seamlessly transition/blend between primary colors (Red/Yellow/Green/Blue)
/// </summary>
void LightingManager::CyclePrimaryColors()
{
    auto lock = m_lock.Lock();

    Windows::Foundation::TimeSpan span;
    span.Duration = 500 * c_millisecondsMultiplier;

    for (LampPair lampArray : m_lampArrays)
    {
        auto allLampIndexes = ref new Platform::Array<int>(lampArray.first->LampCount);
        for (int i = 0; i < lampArray.first->LampCount; i++)
        {
            allLampIndexes[i] = i;
        }

        lampArray.second = ref new LampArrayEffectPlaylist();
        lampArray.second->RepetitionMode = LampArrayRepetitionMode::Forever;

        auto redColorRampEffect = ref new LampArrayColorRampEffect(lampArray.first, allLampIndexes);
        redColorRampEffect->Color = Windows::UI::Colors::Red;
        redColorRampEffect->ZIndex = 0;
        redColorRampEffect->RampDuration = span;
        redColorRampEffect->CompletionBehavior = LampArrayEffectCompletionBehavior::KeepState;
        lampArray.second->Append(redColorRampEffect);

        auto yellowColorRampEffect = ref new LampArrayColorRampEffect(lampArray.first, allLampIndexes);
        yellowColorRampEffect->Color = Windows::UI::Colors::Yellow;
        yellowColorRampEffect->ZIndex = 0;
        yellowColorRampEffect->RampDuration = span;
        yellowColorRampEffect->CompletionBehavior = LampArrayEffectCompletionBehavior::KeepState;
        lampArray.second->Append(yellowColorRampEffect);

        auto greenColorRampEffect = ref new LampArrayColorRampEffect(lampArray.first, allLampIndexes);
        greenColorRampEffect->Color = Windows::UI::Colors::Lime;
        greenColorRampEffect->ZIndex = 0;
        greenColorRampEffect->RampDuration = span;
        greenColorRampEffect->CompletionBehavior = LampArrayEffectCompletionBehavior::KeepState;
        lampArray.second->Append(greenColorRampEffect);

        auto blueColorRampEffect = ref new LampArrayColorRampEffect(lampArray.first, allLampIndexes);
        blueColorRampEffect->Color = Windows::UI::Colors::Blue;
        blueColorRampEffect->ZIndex = 0;
        blueColorRampEffect->RampDuration = span;
        blueColorRampEffect->CompletionBehavior = LampArrayEffectCompletionBehavior::KeepState;
        lampArray.second->Append(blueColorRampEffect);

        lampArray.second->Start();
    }
}

/// <summary>
/// Blinks every Lamp in unison, each with a different random color.
/// </summary>
void LightingManager::BlinkRandomColors()
{
    auto lock = m_lock.Lock();

    Windows::Foundation::TimeSpan span;

    for (LampPair lampArray : m_lampArrays)
    {
        lampArray.second = ref new LampArrayEffectPlaylist();
        lampArray.second->RepetitionMode = LampArrayRepetitionMode::Forever;
        lampArray.second->EffectStartMode = LampArrayEffectStartMode::Simultaneous;

        for (int i = 0; i < lampArray.first->LampCount; i++)
        {
            auto blinkEffect = ref new LampArrayBlinkEffect(lampArray.first, ref new Platform::Array<int>{ i });

            Windows::UI::Color randColor{ 0xFF, (byte)(rand() % 255), (byte)(rand() % 255), (byte)(rand() % 255) };
            blinkEffect->Color = randColor;
            blinkEffect->ZIndex = 0;

            span.Duration = 300 * c_millisecondsMultiplier;
            blinkEffect->AttackDuration = span;

            span.Duration = 500 * c_millisecondsMultiplier;
            blinkEffect->SustainDuration = span;

            span.Duration = 800 * c_millisecondsMultiplier;
            blinkEffect->DecayDuration = span;

            span.Duration = 100 * c_millisecondsMultiplier;
            blinkEffect->RepetitionDelay = span;

            blinkEffect->RepetitionMode = LampArrayRepetitionMode::Forever;
            lampArray.second->Append(blinkEffect);
        }

        lampArray.second->Start();
    }
}

void LightingManager::PlaySnakeEffect()
{
    auto lock = m_lock.Lock();

    for (LampPair lampArray : m_lampArrays)
    {
        for (uint32_t i = 0; i < m_snakeEffects.size(); i++)
        {
            if (m_snakeEffects[i]->ContainsLampArray(lampArray.first))
            {
                m_snakeEffects.erase(m_snakeEffects.begin() + i);

                // Can only ever be one; so skip the rest (additionally, the indexing will be off))
                break;
            }
        }

        // Creating a SnakeEffect will also start it.
        m_snakeEffects.emplace_back(ref new SnakeEffect({ 0xFF, 0x00, 0x00, 0xFF }, 15, lampArray.first));
    }
}
