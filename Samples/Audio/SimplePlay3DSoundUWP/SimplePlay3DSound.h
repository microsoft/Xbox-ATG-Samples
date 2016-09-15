//--------------------------------------------------------------------------------------
// SimplePlay3DSound.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "WAVFileReader.h"

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

private:
    //Audio functions
    void SetReverb(int index);
    void SetupEnvironment();
    void PlayFile(const wchar_t* filename);
    void AdjustFront(float value, X3DAUDIO_VECTOR *orientFront, float *angle);
    
    //Draw functions
    void XM_CALLCONV DrawGrid(size_t xdivs, size_t ydivs, DirectX::FXMVECTOR color);
    void DrawCircle(X3DAUDIO_VECTOR position, float radius);
    void XM_CALLCONV DrawEmitter(X3DAUDIO_CONE* cone, X3DAUDIO_VECTOR position, float angle, DirectX::FXMVECTOR color, UINT size);
    void XM_CALLCONV DrawListener(X3DAUDIO_VECTOR position, DirectX::FXMVECTOR color);

    void Update(DX::StepTimer const& timer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    
    // UI resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_batch;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_circleTexture;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_batchInputLayout;
    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::BasicEffect>           m_batchEffect;
    std::unique_ptr<DirectX::SpriteBatch>           m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>            m_font;
    std::unique_ptr<DirectX::SpriteFont>            m_ctrlFont;

    // Rendering loop timer.
    DX::StepTimer                                   m_timer;

    //XAudio2 interfaces
    std::unique_ptr<uint8_t[]>                      m_waveFile;
    Microsoft::WRL::ComPtr<IXAudio2>                m_XAudio2;
    IXAudio2MasteringVoice*                         m_masteringVoice;
    IXAudio2SourceVoice*                            m_sourceVoice;
    IXAudio2SubmixVoice*                            m_submixVoice;
    Microsoft::WRL::ComPtr<IUnknown>                m_reverbEffect;
    std::unique_ptr<float[]>                        m_matrix;

    //X3DAudio values
    X3DAUDIO_DISTANCE_CURVE_POINT                   m_volumePoints[10];
    X3DAUDIO_DISTANCE_CURVE                         m_volumeCurve;
    X3DAUDIO_DISTANCE_CURVE_POINT                   m_reverbPoints[10];
    X3DAUDIO_DISTANCE_CURVE                         m_reverbCurve;
    X3DAUDIO_HANDLE			                        m_X3DInstance;
    X3DAUDIO_LISTENER		                        m_X3DListener;
    X3DAUDIO_EMITTER		                        m_X3DEmitter;
    X3DAUDIO_DSP_SETTINGS                           m_X3DDSPSettings;
    XAUDIO2_VOICE_DETAILS                           m_deviceDetails;
    X3DAUDIO_CONE                                   m_emitterCone;

    // Game state
    int                                             m_reverbIndex;
    float                                           m_listenerAngle;
    float                                           m_emitterAngle;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>               m_gamePad;
    std::unique_ptr<DirectX::Keyboard>              m_keyboard;

    DirectX::GamePad::ButtonStateTracker            m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker         m_keyboardButtons;
    bool                                            m_gamepadPresent;
};