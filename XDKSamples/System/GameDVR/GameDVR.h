//--------------------------------------------------------------------------------------
// GameDVR.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

namespace WXMC = Windows::Xbox::Media::Capture;

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample() noexcept(false);
    ~Sample() = default;

    Sample(Sample&&) = delete;
    Sample& operator= (Sample&&) = delete;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

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

    void UpdateCube(float elapsedTime);
    void RenderCube(_In_ ID3D11DeviceContextX* const pCtx);
    void RenderText(_In_ ID3D11DeviceContextX* const pCtx);
    void DrawMessage(float sx, float sy, const wchar_t* value);
    void DisplayMessage(const std::wstring& message);

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
    
    // Sample objects
    enum APPSTATE
    {
        APP_INIT,
        APP_AWAITING_LOGIN,
        APP_START_RECORDING,
        APP_RECORDING,
        APP_DONE_RECORDING,
        APP_START_QUERY,
        APP_QUERY,
        APP_DONE_ERROR,
        APP_DONE
    };

    std::unique_ptr<DirectX::GeometricPrimitive> m_GeoCube = nullptr;


    DirectX::XMMATRIX                        m_mWorld;
    DirectX::XMMATRIX                        m_mView;
    DirectX::XMMATRIX                        m_mProjection;

    float                           m_curRotationAngleRad;
    float                           m_totalCaptureTime;

    std::atomic<APPSTATE>           m_appState;

    std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont> m_font;
    std::unique_ptr<DirectX::SpriteFont> m_fontLarge;
    std::unique_ptr<DirectX::SpriteFont> m_fontExtraLarge;

    std::deque< std::wstring >      m_messageQueue;

    Windows::Xbox::System::User^    m_user;

    WXMC::ApplicationClipCapture^   m_appCapture;
    WXMC::ApplicationClipInfo^      m_appClipInfo;
    WXMC::ApplicationClipQuery^     m_appClipQuery;
};
