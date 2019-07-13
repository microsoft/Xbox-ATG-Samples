//--------------------------------------------------------------------------------------
// ExtendedExecutionOnSuspend.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"

#include "ToastManager.h"
#include "TextConsole.h"

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample final : public DX::IDeviceNotify
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
    void OnSuspending(Windows::ApplicationModel::ExtendedExecution::ExtendedExecutionSession^ session);
    void OnResuming();
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
    void ValidateDevice();

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

    //SampleFunctions
    void ShowInstructions();
    void LogEvent(const wchar_t* primaryLog, const wchar_t* secondaryData = L"");
    void SetExtensionActive(bool isActive) { m_extensionActive = isActive; }

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
    void TogglePing();
    void RequestExtensionOnSuspend(Windows::ApplicationModel::ExtendedExecution::ExtendedExecutionSession^ session);

    std::unique_ptr<DX::TextConsoleImage>	m_console;
    std::unique_ptr<DX::ToastManager>		m_toastManager;
    bool									m_showToasts;
    bool									m_consoleIsValid;
    std::vector<Platform::String^>			m_logCache;
    bool                                    m_extensionActive;
    bool                                    m_pingEveryTenSeconds;
};