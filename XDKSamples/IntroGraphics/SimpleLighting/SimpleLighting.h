//--------------------------------------------------------------------------------------
// SimpleLighting.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

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

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();


    // Device resources.
    std::unique_ptr<DX::DeviceResources>       m_deviceResources;

    // Rendering loop timer.
    uint64_t                                   m_frame;
    DX::StepTimer                              m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>          m_gamePad;

    DirectX::GamePad::ButtonStateTracker       m_gamePadButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>   m_graphicsMemory;

	// Sample Objects
	Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_inputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       m_indexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       m_constantBuffer;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_pixelShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_pixelShaderSolid;
	
	// Scene constants, updated per-frame
	float                                      m_curRotationAngleRad;

	// These computed values will be loaded into a ConstantBuffer
	// during Render
	DirectX::XMFLOAT4X4                        m_worldMatrix;
	DirectX::XMFLOAT4X4                        m_viewMatrix;
	DirectX::XMFLOAT4X4                        m_projectionMatrix;
	DirectX::XMFLOAT4                          m_lightDirs[2];
	DirectX::XMFLOAT4                          m_lightColors[2];
	DirectX::XMFLOAT4                          m_outputColor;
};
