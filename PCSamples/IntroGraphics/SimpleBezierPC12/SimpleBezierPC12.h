//--------------------------------------------------------------------------------------
// SimpleBezierPC12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "ControllerHelp.h"


// A basic sample implementation that creates a D3D12 device and
// provides a render loop.
class Sample final : public DX::IDeviceNotify
{
public:

    Sample() noexcept(false);
    ~Sample();

    Sample(Sample&&) = default;
    Sample& operator= (Sample&&) = default;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

    // Initialization and management
    void Initialize(HWND window, int width, int height);

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
    void OnWindowMoved();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateShaders();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                               m_timer;

    // Input devices.
    bool									    m_ctrlConnected;
    std::unique_ptr<DirectX::GamePad>           m_gamePad;
    std::unique_ptr<DirectX::Keyboard>          m_keyboard;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker     m_keyboardButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

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

    static const size_t c_numPixelShaders = 2;
    static const size_t c_numHullShaders = 3;

    Microsoft::WRL::ComPtr<ID3D12RootSignature>     m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>     m_PSOs[c_numPixelShaders][c_numHullShaders];

    std::unique_ptr<DirectX::DescriptorHeap>        m_resourceDescriptors;

    D3D12_VERTEX_BUFFER_VIEW                        m_controlPointVBView;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_controlPointVB;     // Control points for mesh
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cbPerFrame;
    ConstantBuffer*                                 m_mappedConstantData;

    // Index in the root parameter table
    static const UINT                               c_rootParameterCB = 0;

    // Control variables
    float                                           m_subdivs;
    bool                                            m_drawWires;
    PartitionMode                                   m_partitionMode;

    DirectX::XMFLOAT4X4                             m_worldMatrix;
    DirectX::XMFLOAT4X4                             m_viewMatrix;
    DirectX::XMFLOAT4X4                             m_projectionMatrix;
    DirectX::XMFLOAT3                               m_cameraEye;

    // Legend and help UI
    std::unique_ptr<DirectX::DescriptorHeap>        m_fontDescriptors;
    std::unique_ptr<DirectX::SpriteBatch>           m_batch;
    std::unique_ptr<DirectX::SpriteFont>            m_smallFont;
    std::unique_ptr<DirectX::SpriteFont>            m_ctrlFont;

    std::unique_ptr<ATG::Help>                      m_help;
    bool                                            m_showHelp;
};
