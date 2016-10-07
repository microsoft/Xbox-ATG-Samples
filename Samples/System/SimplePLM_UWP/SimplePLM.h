//--------------------------------------------------------------------------------------
// SimplePLM.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

#include "ToastManager.h"
#include "TextConsole.h"

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample : public DX::IDeviceNotify
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

    // Basic render loop
    void Tick();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
	void OnActivated();
	void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
    void ValidateDevice();

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

	//SampleFunctions
	void ShowInstructions();
	void LogPLMEvent(const wchar_t* primaryLog, const wchar_t* secondaryData = L"");
	bool GetUseDeferral() { return m_useDeferral; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    
    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;

    // Sample Specific
	void ToggleNotifications();
	void ToggleDeferral();

    std::unique_ptr<DX::TextConsoleImage>	m_console;
    std::unique_ptr<DX::ToastManager>		m_toastManager;
	bool									m_useDeferral;
	bool									m_showToasts;
	bool									m_consoleIsValid;
	std::vector<Platform::String^>			m_logCache;
};