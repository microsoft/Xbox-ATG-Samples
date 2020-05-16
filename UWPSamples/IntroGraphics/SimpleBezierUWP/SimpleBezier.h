//--------------------------------------------------------------------------------------
// SimpleBezier.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "ControllerHelp.h"


// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample final : public DX::IDeviceNotify
{
public:

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
    void CreateShaders();
    void CreateWindowSizeDependentResources();
    
    // Device resources
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer
    DX::StepTimer                           m_timer;

    // Input devices
    bool									m_ctrlConnected;
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;

    // Sample objects
    struct ConstantBuffer
    {
        DirectX::XMFLOAT4X4 viewProjectionMatrix;
        DirectX::XMFLOAT3   cameraWorldPos;
        float               tessellationFactor;
    };

    enum class PartitionMode
    {
        PartitionInteger,
        PartitionFractionalEven,
        PartitionFractionalOdd
    };

    std::unique_ptr<DirectX::CommonStates>          m_states;

    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11HullShader>        m_hullShaderInteger;
    Microsoft::WRL::ComPtr<ID3D11HullShader>        m_hullShaderFracEven;
    Microsoft::WRL::ComPtr<ID3D11HullShader>        m_hullShaderFracOdd;
    Microsoft::WRL::ComPtr<ID3D11DomainShader>      m_domainShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_solidColorPS;

    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_controlPointVB;     // Control points for mesh
    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_cbPerFrame;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_rasterizerStateSolid;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_rasterizerStateWireframe;

    // Control variables
    float                                           m_subdivs;
    bool                                            m_drawWires;
    PartitionMode                                   m_partitionMode;

    DirectX::XMFLOAT4X4                             m_worldMatrix;
    DirectX::XMFLOAT4X4                             m_viewMatrix;
    DirectX::XMFLOAT4X4                             m_projectionMatrix;
    DirectX::XMFLOAT3                               m_cameraEye;

    // Legend and help UI
    std::unique_ptr<DirectX::SpriteBatch>           m_batch;
    std::unique_ptr<DirectX::SpriteFont>            m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>            m_ctrlFont;

    std::unique_ptr<ATG::Help>                      m_help;
    bool                                            m_showHelp;
};
