//--------------------------------------------------------------------------------------
// SimplePlay3DSpatialSound.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "ISACRenderer.h"
#include "WAVFileReader.h"

class Sample final : public DX::IDeviceNotify
{
public:
#define MAX_CHANNELS 12 //up to 7.1.4 channels

    struct AudioEmitter
    {
        char*   wavBuffer;
        UINT32  buffersize;
        UINT32  curBufferLoc;
        float   posX;
        float   posY;
        float   posZ;
        float   angle;
        Microsoft::WRL::ComPtr<ISpatialAudioObject> object;
    };

    Sample() noexcept(false);
    ~Sample() = default;

    Sample(Sample&&) = default;
    Sample& operator= (Sample&&) = default;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

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

    //Audio properties needed for threading
    Microsoft::WRL::ComPtr<ISACRenderer>            m_renderer;
    AudioEmitter                                    m_emitter;
    AudioEmitter                                    m_listener;
    bool	                                        m_threadActive;

private:
    bool LoadFile(LPCWSTR inFile);

    //Draw functions
    void XM_CALLCONV DrawGrid(size_t xdivs, size_t ydivs, DirectX::FXMVECTOR color);
    void XM_CALLCONV DrawTriangle(AudioEmitter source, DirectX::FXMVECTOR color);

    void Update(DX::StepTimer const& timer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    
    // UI resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_batch;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_batchInputLayout;
    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::BasicEffect>           m_batchEffect;
    std::unique_ptr<DirectX::SpriteBatch>           m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>            m_font;
    std::unique_ptr<DirectX::SpriteFont>            m_ctrlFont;

    // Rendering loop timer.
    DX::StepTimer                                   m_timer;

    // Game state
    bool                                            m_fileLoaded;

    // Worker thread for spatial system
    PTP_WORK                                        m_workThread;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>               m_gamePad;
    std::unique_ptr<DirectX::Keyboard>              m_keyboard;

    DirectX::GamePad::ButtonStateTracker            m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker         m_keyboardButtons;
    bool                                            m_gamepadPresent;
};
