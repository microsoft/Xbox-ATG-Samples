//--------------------------------------------------------------------------------------
// UserManagement.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "TextConsole.h"

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic game loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();
	void OnVisibilityChanged(Windows::UI::Core::VisibilityChangedEventArgs^ args);

    // Properties
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Get platform gamepad from DirectXTK player index.
    Windows::Xbox::Input::IGamepad^ GetGamepad(int index);

	// Show account picker with options, then return User.
	Concurrency::task<Windows::Xbox::System::User^> PickUserAsync(Windows::Xbox::Input::IController^ controller, Windows::Xbox::UI::AccountPickerOptions options);

    // Write information to the sample console window.
    void WriteControllers();
	void WriteController(Windows::Xbox::Input::IController^ controller);
    void WriteUsers();
    void WriteUser(Windows::Xbox::System::User^ user);

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;
    std::array<DirectX::GamePad::ButtonStateTracker, DirectX::GamePad::MAX_PLAYER_COUNT> m_gamePadButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>        m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>        m_controllerLegendFont;

	// Text console.
	std::unique_ptr<DX::TextConsole>            m_console;

	// User management objects.
	Windows::Xbox::System::User^				m_activeUser;
	Windows::Xbox::System::User^				m_currentUser;
	int                                         m_signOutDeferralTimeInSeconds;
};
