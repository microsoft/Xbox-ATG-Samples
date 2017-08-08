//--------------------------------------------------------------------------------------
// SimpleInstancing.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "Shared.h"

#include <memory.h>

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
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
    void ValidateDevice();

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void ReplaceBufferContents(ID3D11Buffer* buffer, size_t bufferSize, const void* data);
    void ResetSimulation();

    float FloatRand(float lowerBound = -1.0f, float upperBound = 1.0f);

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;
    std::unique_ptr<DirectX::Mouse>         m_mouse;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;
    bool                                    m_gamepadPresent;

    // DirectXTK objects.
    std::unique_ptr<DirectX::SpriteBatch>       m_batch;
    std::unique_ptr<DirectX::SpriteFont>        m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;

    //--------------------------------------------------------------------------------------
    // Sample Objects.
    //--------------------------------------------------------------------------------------

    // Instance vertex definition
    struct Instance
    {
        DirectX::XMFLOAT4 quaternion;
        DirectX::XMFLOAT4 positionAndScale;
    };

    // Light data structure (maps to constant buffer in pixel shader)
    struct Lights
    {
        DirectX::XMFLOAT4 directional;
        DirectX::XMFLOAT4 pointPositions[c_pointLightCount];
        DirectX::XMFLOAT4 pointColors[c_pointLightCount];
    };

    Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_instanceData;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_boxColors;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_vertexConstants;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_pixelConstants;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pixelShader;

    struct aligned_deleter { void operator()(void* p) { _aligned_free(p); } };

    std::unique_ptr<Instance[]>                             m_CPUInstanceData;
    std::unique_ptr<DirectX::XMVECTOR[], aligned_deleter>   m_rotationQuaternions;
    std::unique_ptr<DirectX::XMVECTOR[], aligned_deleter>   m_velocities;
    uint32_t                                                m_usedInstanceCount;

    DirectX::XMFLOAT4X4                         m_proj;
    Lights                                      m_lights;
    float                                       m_pitch;
    float                                       m_yaw;

    std::default_random_engine                  m_randomEngine;
};