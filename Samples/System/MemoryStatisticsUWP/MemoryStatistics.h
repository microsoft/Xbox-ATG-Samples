//--------------------------------------------------------------------------------------
// MemoryStatistics.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

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
    void Render();

    // Rendering helpers
    void Clear();

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

    const DirectX::SimpleMath::Matrix& GetWorld() { return m_world; }
    const DirectX::SimpleMath::Matrix& GetView() { return m_view; }
    const DirectX::SimpleMath::Matrix& GetProjection() { return m_projection; }

    static const unsigned                   c_MaxTeapots = 1000;

private:

    // Holds data returned from GetProcessMemoryInfo. This contains information about the current
    //  and peak memory usage of this game during runtime.
    PROCESS_MEMORY_COUNTERS                 m_frameMemoryUsage;
    PROCESS_MEMORY_COUNTERS                 m_preRunMemoryUsage;

    std::unique_ptr<DirectX::SpriteBatch>   m_batch;
    std::unique_ptr<DirectX::SpriteFont>    m_font;
    std::unique_ptr<DirectX::SpriteFont>    m_ctrlFont;

    wchar_t                                 m_temporaryTextBuffer[1024];
    float                                   m_temporaryTextTime;

    void Update(DX::StepTimer const& timer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void CreateNewTeapot();
    void DestroyTeapot();
    float FloatRand(float lowerBound, float upperBound);
    void PercentageStats();

    struct TeapotData
    {
        TeapotData() {}
        TeapotData(const TeapotData& other) :
            m_location(other.m_location),
            m_lifeFrameCount(other.m_lifeFrameCount)
        {
        }

        std::unique_ptr<DirectX::GeometricPrimitive>    m_teapot;
        DirectX::SimpleMath::Matrix                     m_location;
        unsigned int                                    m_lifeFrameCount;
    }; 

    DirectX::SimpleMath::Matrix             m_projection;
    DirectX::SimpleMath::Matrix             m_world;
    DirectX::SimpleMath::Matrix             m_view;
    DirectX::SimpleMath::Vector3            m_eye;
    DirectX::SimpleMath::Vector3            m_at;
    std::vector<TeapotData>                 m_teapots;

    std::unique_ptr<DirectX::GeometricPrimitive> m_testPrimitive;

    std::default_random_engine              m_randomEngine;
    
    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;

    bool                                    m_gamepadPresent;
};