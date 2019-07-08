//--------------------------------------------------------------------------------------
// LightingManager.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

typedef std::pair<Windows::Devices::Lights::LampArray^, Windows::Devices::Lights::Effects::LampArrayEffectPlaylist^> LampPair;

ref class LightingManager sealed
{
public:
    static LightingManager^ GetInstance();

    static void Initialize();

public:
    void SetLampArraysColor(
        Windows::UI::Color desiredColor);

    void ClearLampArrays();

	bool LampArrayAvailable() { return m_lampArrays.size() > 0; };

	// Samples
    void LeftControlLampsBlue();

    void WasdKeysRed();

    void BlinkWasdKeys();

    void PlayGreenSolidEffect();

    void CyclePrimaryColors();

    void BlinkRandomColors();

    void PlaySnakeEffect();

    void PlaySimpleBitmapEffect();

private:
    LightingManager();

    ~LightingManager();

    void InitializeInternal();

    void LampArrayAdded(
		Windows::Devices::Enumeration::DeviceWatcher^ watcher,
		Windows::Devices::Enumeration::DeviceInformation^ lampArray);

    void LampArrayRemoved(
		Windows::Devices::Enumeration::DeviceWatcher^ watcher,
        Windows::Devices::Enumeration::DeviceInformationUpdate^ args);

    void LampArrayWatcherEnumerationCompleted(
		Windows::Devices::Enumeration::DeviceWatcher^ watcher,
        Object^ args);

    void LampArrayWatcherStopped(
		Windows::Devices::Enumeration::DeviceWatcher^ watcher,
        Object^ args);

private:
    static LightingManager^ s_instance;

    static Microsoft::WRL::Wrappers::CriticalSection m_initLock;

	Windows::Devices::Enumeration::DeviceWatcher^   m_lampArrayWatcher;

    // Maintain a list of known LampArrays and their playlist.
	std::vector<LampPair> m_lampArrays;

    // Serializes access to LampArray devices known by application.
    Microsoft::WRL::Wrappers::CriticalSection m_lock;

    std::vector<SnakeEffect^> m_snakeEffects;

    std::vector<SimpleBitmapEffect^> m_bitmapEffects;
};