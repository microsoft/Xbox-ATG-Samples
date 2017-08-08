//--------------------------------------------------------------------------------------
// CPUSets.h
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

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    void OnActivated() {}
    void OnDeactivated() {}
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
    void ValidateDevice();

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

    HANDLE GetGraphicsMutex() { return m_graphicsMutex;  }
    void SetWorldMatrix(DirectX::SimpleMath::Matrix& matrix) { m_world = matrix; }
    DirectX::AudioEngine* GetAudioEngine() { return m_audioEngine.get(); }
    static const wchar_t* g_graphicsMutexName;

    bool CpuIsUsingHyperthreading() { return m_hyperThreading == HyperThreadedState::HyperThreaded;  }

private:

    enum class HyperThreadedState
    {
        Unknown,
        HyperThreaded,
        NotHyperThreaded
    };

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void ReportCPUInformation(SYSTEM_CPU_SET_INFORMATION* cpuSetInfo, size_t count);
    void OrganizeCPUSets(SYSTEM_CPU_SET_INFORMATION* cpuSetInfo, size_t count);
    void SortThreads();

    std::map<unsigned char, std::vector<SYSTEM_CPU_SET_INFORMATION>> m_cpuSets;

    static unsigned long __stdcall AudioThread(void* params);
    static unsigned long __stdcall WorkerThread(void* params);
    static unsigned long __stdcall GeneratorThread(void* params);

    std::unique_ptr<DirectX::IEffectFactory> m_fxFactory;
    std::unique_ptr<DirectX::CommonStates> m_states;
    std::unique_ptr<DirectX::Model> m_model;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture;
    DirectX::SimpleMath::Matrix m_view, m_world, m_projection;
    HANDLE m_graphicsMutex;
    HyperThreadedState m_hyperThreading;

    DirectX::SimpleMath::Vector3 m_eye;
    DirectX::SimpleMath::Vector3 m_at;

    std::unique_ptr<DirectX::AudioEngine> m_audioEngine;

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;
};