//--------------------------------------------------------------------------------------
// SimpleComputeUWP.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "ControllerHelp.h"

class SmoothedFPS
{
public:
    SmoothedFPS(uint32_t frameInterval = 100)
    {
        Initialize(frameInterval);
    }

    void Initialize(uint32_t frameInterval = 100)
    {
        m_frameInterval = frameInterval;
        m_timeAccumulator = 0.0f;
        m_frameAccumulator = 0;
        m_smoothedFPS = 0.0f;
    }

    void Tick(float DeltaTime)
    {
        m_timeAccumulator += DeltaTime;
        ++m_frameAccumulator;

        if (m_frameAccumulator >= m_frameInterval)
        {
            m_smoothedFPS = (float)m_frameInterval / m_timeAccumulator;
            m_timeAccumulator = 0.0f;
            m_frameAccumulator = 0;
        }
    }

    float GetFPS() const { return m_smoothedFPS; }

private:
    float m_smoothedFPS;
    float m_timeAccumulator;
    uint32_t m_frameAccumulator;
    uint32_t m_frameInterval;

};


// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample final : public DX::IDeviceNotify
{
public:

    Sample();

    // Initialization and management
    void Initialize(::IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

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

private:
    struct CB_FractalCS
    {
        DirectX::XMFLOAT4 MaxThreadIter;
        DirectX::XMFLOAT4 Window;
    };

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void ResetWindow();
    void UpdateFractalData();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    std::unique_ptr<ATG::Help>              m_help;
    bool                                    m_showHelp;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;
    std::unique_ptr<DirectX::Mouse>         m_mouse;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;
    bool                                    m_gamepadPresent;

    // Compute data
    SmoothedFPS                                 m_renderFPS;

    uint64_t*                                   m_fractalTimestamps;
    DirectX::XMFLOAT4                           m_window;
    bool                                        m_windowUpdated;
    uint32_t                                    m_fractalMaxIterations;

    Microsoft::WRL::ComPtr<ID3D11Buffer>                m_cbFractal;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader>	        m_csFractal;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>             m_fractalTexture;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>   m_fractalUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_fractalSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>             m_fractalColorMap;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_fractalColorMapSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>          m_fractalBilinearSampler;

    // DirectXTK objects.
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>        m_font;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;
};