//--------------------------------------------------------------------------------------
// SimpleLightingUWP.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleLightingUWP.h"
#include "ATGColors.h"
#include "ReadData.h"

extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
    struct Vertex
    {
        XMFLOAT3 pos;
        XMFLOAT3 normal;
    };

    struct ConstantBuffer
    {
        XMMATRIX worldMatrix;
        XMMATRIX viewMatrix;
        XMMATRIX projectionMatrix;
        XMVECTOR lightDir[2];
        XMVECTOR lightColor[2];
        XMVECTOR outputColor;
    };

    static_assert((sizeof(ConstantBuffer) % 16) == 0, "Constant buffer must always be 16-byte aligned");
}

Sample::Sample() :
    m_curRotationAngleRad(0.0f)
{
    // Use gamma-correct rendering.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();  	
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());

    // Update the rotation constant
    m_curRotationAngleRad += elapsedTime / 3.f;
    if (m_curRotationAngleRad >= XM_2PI)
    {
        m_curRotationAngleRad -= XM_2PI;
    }

    // Rotate the cube around the origin
    XMStoreFloat4x4(&m_worldMatrix, XMMatrixRotationY(m_curRotationAngleRad));

    // Setup our lighting parameters
    m_lightDirs[0] = XMFLOAT4(-0.577f, 0.577f, -0.577f, 1.0f);
    m_lightDirs[1] = XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f);

    m_lightColors[0] = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
    m_lightColors[1] = XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f);

    // Rotate the second light around the origin
    XMMATRIX rotate = XMMatrixRotationY(-2.0f * m_curRotationAngleRad);
    XMVECTOR lightDir = XMLoadFloat4(&m_lightDirs[1]);
    lightDir = XMVector3Transform(lightDir, rotate);
    XMStoreFloat4(&m_lightDirs[1], lightDir);

    // Handle controller input for exit
    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitSample();
    }

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Sample::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    // Set the vertex buffer
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

    // Set the index buffer
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    // Set the input layout
    context->IASetInputLayout(m_inputLayout.Get());

    // Set the primitive toplogy
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set the per-frame constants
    ConstantBuffer sceneParameters = {};

    // Shaders compiled with default row-major matrices
    sceneParameters.worldMatrix = XMMatrixTranspose(XMLoadFloat4x4(&m_worldMatrix));
    sceneParameters.viewMatrix = XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix));
    sceneParameters.projectionMatrix = XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix));

    sceneParameters.lightDir[0] = XMLoadFloat4(&m_lightDirs[0]);
    sceneParameters.lightDir[1] = XMLoadFloat4(&m_lightDirs[1]);
    sceneParameters.lightColor[0] = XMLoadFloat4(&m_lightColors[0]);
    sceneParameters.lightColor[1] = XMLoadFloat4(&m_lightColors[1]);
    sceneParameters.outputColor = XMLoadFloat4(&m_outputColor);

    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        DX::ThrowIfFailed(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        memcpy(mapped.pData, &sceneParameters, sizeof(ConstantBuffer));
        context->Unmap(m_constantBuffer.Get(), 0);
    }

    // Render the cube
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);

    // Vertex shader needs view and projection matrices to perform vertex transform
    context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    
    // Pixel shader needs light-direction vectors to perform per-pixel lighting
    context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->DrawIndexed(36, 0, 0);
    
    // Render each light
    for (int m = 0; m < 2; ++m)
    {
        XMMATRIX lightMatrix = XMMatrixTranslationFromVector(5.0f * sceneParameters.lightDir[m]);
        XMMATRIX lightScaleMatrix = XMMatrixScaling(0.2f, 0.2f, 0.2f);
        lightMatrix = lightScaleMatrix * lightMatrix;

        // Update the world variable to reflect the current light
        sceneParameters.worldMatrix = XMMatrixTranspose(lightMatrix);
        sceneParameters.outputColor = sceneParameters.lightColor[m];

        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            DX::ThrowIfFailed(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
            memcpy(mapped.pData, &sceneParameters, sizeof(ConstantBuffer));
            context->Unmap(m_constantBuffer.Get(), 0);
        }

        context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
        context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

        context->PSSetShader(m_pixelShaderSolid.Get(), nullptr, 0);
        context->DrawIndexed(36, 0, 0);
    }
    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    // Use linear clear color for gamma-correct rendering
    context->ClearRenderTargetView(renderTarget, ATG::ColorsLinear::Background);

    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnActivated()
{
}

void Sample::OnDeactivated()
{
}

void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->ClearState();

    m_deviceResources->Trim();
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
}

void Sample::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, rotation))
        return;

    CreateWindowSizeDependentResources();
}

void Sample::ValidateDevice()
{
    m_deviceResources->ValidateDevice();
}

// Properties
void Sample::GetDefaultSize(int& width, int& height) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Load and create shaders
    {
        auto blob = DX::ReadData(L"TriangleVS.cso");

        DX::ThrowIfFailed(
            device->CreateVertexShader(blob.data(), blob.size(),
                nullptr, m_vertexShader.ReleaseAndGetAddressOf()));

        // Create the input layout
        const D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        DX::ThrowIfFailed(
            device->CreateInputLayout(inputElementDesc, _countof(inputElementDesc),
                blob.data(), blob.size(),
                m_inputLayout.ReleaseAndGetAddressOf()));
    }

    {
        auto blob = DX::ReadData(L"LambertPS.cso");

        DX::ThrowIfFailed(
            device->CreatePixelShader(blob.data(), blob.size(),
                nullptr, m_pixelShader.ReleaseAndGetAddressOf()));
    }

    {
        auto blob = DX::ReadData(L"SolidColorPS.cso");

        DX::ThrowIfFailed(
            device->CreatePixelShader(blob.data(), blob.size(),
                nullptr, m_pixelShaderSolid.ReleaseAndGetAddressOf()));
    }

    // Create and initialize the vertex buffer
    {
        static const Vertex vertices[] =
        {
            { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
            { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
            { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
            { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },

            { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
            { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
            { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
            { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },

            { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },

            { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },

            { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
            { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
            { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
            { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },

            { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
            { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
            { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
            { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        };

        CD3D11_BUFFER_DESC bufferDesc(sizeof(Vertex) * _countof(vertices), D3D11_BIND_VERTEX_BUFFER);
        bufferDesc.StructureByteStride = sizeof(Vertex);

        D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
        DX::ThrowIfFailed(device->CreateBuffer(&bufferDesc, &initData, m_vertexBuffer.ReleaseAndGetAddressOf()));
    }

    // Create and initialize the index buffer
    {
        static const uint16_t indices[] =
        {
            3,1,0,
            2,1,3,

            6,4,5,
            7,4,6,

            11,9,8,
            10,9,11,

            14,12,13,
            15,12,14,

            19,17,16,
            18,17,19,

            22,20,21,
            23,20,22
        };
        CD3D11_BUFFER_DESC bufferDesc(sizeof(uint16_t) * _countof(indices), D3D11_BIND_INDEX_BUFFER);
        bufferDesc.StructureByteStride = sizeof(uint16_t);

        D3D11_SUBRESOURCE_DATA initData = { indices, 0, 0 };
        DX::ThrowIfFailed(device->CreateBuffer(&bufferDesc, &initData, m_indexBuffer.ReleaseAndGetAddressOf()));
    }

    // Create the constant buffer
    {
        CD3D11_BUFFER_DESC bufferDesc(sizeof(ConstantBuffer), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
        DX::ThrowIfFailed(device->CreateBuffer(&bufferDesc, nullptr, m_constantBuffer.ReleaseAndGetAddressOf()));
    }

    // Initialize the world matrix
    XMStoreFloat4x4(&m_worldMatrix, XMMatrixIdentity());

    // Initialize the view matrix
    static const XMVECTORF32 c_eye = { 0.0f, 4.0f, -10.0f, 0.0f };
    static const XMVECTORF32 c_at = { 0.0f, 1.0f, 0.0f, 0.0f };
    static const XMVECTORF32 c_up = { 0.0f, 1.0f, 0.0f, 0.0 };
    XMStoreFloat4x4(&m_viewMatrix, XMMatrixLookAtLH(c_eye, c_at, c_up));

    // Initialize the lighting parameters
    m_lightDirs[0] = XMFLOAT4(-0.577f, 0.577f, -0.577f, 1.0f);
    m_lightDirs[1] = XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f);

    m_lightColors[0] = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
    m_lightColors[1] = XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f);

    // Initialize the scene output color
    m_outputColor = XMFLOAT4(0, 0, 0, 0);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    // Initialize the projection matrix
    auto size = m_deviceResources->GetOutputSize();
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, float(size.right) / float(size.bottom), 0.01f, 100.0f);

    XMFLOAT4X4 orient = m_deviceResources->GetOrientationTransform3D();
    XMStoreFloat4x4(&m_projectionMatrix, projection * XMLoadFloat4x4(&orient));
}

void Sample::OnDeviceLost()
{
    m_inputLayout.Reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_constantBuffer.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_pixelShaderSolid.Reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
