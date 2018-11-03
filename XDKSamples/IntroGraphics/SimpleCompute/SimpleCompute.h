//--------------------------------------------------------------------------------------
// SimpleCompute.h
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
    void AsyncComputeThreadProc();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    std::unique_ptr<ATG::Help>                  m_help;
    bool                                        m_showHelp;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;
    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // Compute data
    SmoothedFPS                                 m_renderFPS;
    SmoothedFPS                                 m_computeFPS;

    std::atomic<bool>                           m_terminateThread;
    std::atomic<bool>                           m_suspendThread;
    std::thread	*                               m_computeThread;

    CB_FractalCS*                               m_cbFractalData;
    UINT                                        m_renderIndex;
    uint64_t*                                   m_fractalTimestamps;
    DirectX::XMFLOAT4                           m_window;
    std::atomic<bool>                           m_windowUpdated;
    uint32_t                                    m_asyncExecuteCount;
    float                                       m_lastAsyncExecuteTimeMsec;
    uint32_t                                    m_fractalMaxIterations;

    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_cbFractal;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader>	m_csFractal;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>     m_fractalTexture[2];
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>  m_fractalUAV[2];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>   m_fractalSRV[2];
    Microsoft::WRL::ComPtr<ID3D11ComputeContextX>      m_computeContext;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>            m_fractalColorMap[2];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>   m_fractalColorMapSRV[2];
    Microsoft::WRL::ComPtr<ID3D11SamplerState>         m_fractalBilinearSampler;

    std::atomic<bool>                           m_usingAsyncCompute;
    bool                                        m_requestUsingAsyncCompute;
    std::atomic<bool>                           m_asyncComputeActive;

    Microsoft::WRL::Wrappers::Event             m_computeResumeSignal;

    // DirectXTK objects.
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteFont>        m_font;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;
};
