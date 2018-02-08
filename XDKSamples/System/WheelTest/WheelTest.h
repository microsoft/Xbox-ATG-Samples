//--------------------------------------------------------------------------------------
// WheelTest.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "buttons.h"
#include "SpriteFont.h"

using namespace std;
using namespace Windows::Xbox::Input;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Xbox::Input;

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

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void DoWork();
    void OnButton(ButtonData * activeButton);

    void DrawVersion();
    void DrawReadings();
    void DrawText(float sx, float sy, const wchar_t* value);

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

    //Variables associated with wheel's navigation functions
    INavigationReading^ m_reading;

    //Set of onscreen buttons
    ButtonGrid* m_pButtons;

    //Data for vendor debug report
    Platform::Array<uint8_t>^ m_vendorDebugReport;

    std::unique_ptr<DirectX::SpriteBatch>               m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>                m_font;
};
