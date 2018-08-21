//--------------------------------------------------------------------------------------
// SimpleInstancingPC.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleInstancingPC.h"

#include "ATGColors.h"
#include "ControllerFont.h"
#include "FindMedia.h"
#include "ReadData.h"

extern void ExitSample();

using namespace DirectX;
using namespace DirectX::PackedVector;

using Microsoft::WRL::ComPtr;

namespace
{
    //--------------------------------------------------------------------------------------
    // Constants
    //--------------------------------------------------------------------------------------
    const uint32_t  c_maxInstances = 20000;
    const uint32_t  c_startInstanceCount = 5000;
    const uint32_t  c_minInstanceCount = 1000;
    const float     c_boxBounds = 60.0f;
    const size_t    c_cubeIndexCount = 36;
    const float     c_velocityMultiplier = 500.0f;
    const float     c_rotationGain = 0.004f;

    //--------------------------------------------------------------------------------------
    // Cube vertex definition
    //--------------------------------------------------------------------------------------
    struct Vertex
    {
        XMFLOAT3 pos;
        XMFLOAT3 norm;
    };
}

Sample::Sample() :
    m_gamepadPresent(false),
    m_usedInstanceCount(c_startInstanceCount),
    m_lights{},
    m_pitch(0.0f),
    m_yaw(0.0f)
{
    XMStoreFloat4x4(&m_proj, XMMatrixIdentity());

    // Use gamma-correct rendering. Requires Feature Level 10.0 or greater.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_10_0);
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(HWND window, int width, int height)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();

    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(window);

    m_deviceResources->SetWindow(window, width, height);

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
    float elapsedTime = float(timer.GetElapsedSeconds());

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    auto pad = m_gamePad->GetState(0);
    m_gamepadPresent = pad.IsConnected();
    if (m_gamepadPresent)
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }

        if (m_gamePadButtons.a == GamePad::ButtonStateTracker::ButtonState::PRESSED)
        {
            ResetSimulation();
        }

        if (m_gamePadButtons.rightShoulder == GamePad::ButtonStateTracker::ButtonState::PRESSED)
        {
            m_usedInstanceCount = std::min(c_maxInstances, m_usedInstanceCount + 1000);
        }
        else if (m_gamePadButtons.leftShoulder == GamePad::ButtonStateTracker::ButtonState::PRESSED)
        {
            m_usedInstanceCount = std::max(c_minInstanceCount, m_usedInstanceCount - 1000);
        }

        if (pad.IsLeftStickPressed())
        {
            m_yaw = m_pitch = 0.f;
        }
        else
        {
            m_yaw += pad.thumbSticks.leftX * 0.1f;
            m_pitch += pad.thumbSticks.leftY * 0.1f;
        }
    }
    else
    {
        m_gamePadButtons.Reset();

        if (kb.A || kb.D)
        {
            m_yaw += (kb.D ? 0.1f : -0.1f);
        }

        if (kb.W || kb.S)
        {
            m_pitch += (kb.W ? 0.1f : -0.1f);
        }

        if (kb.Home)
        {
            m_yaw = m_pitch = 0.f;
        }

        // Basic mouse-look
        auto mouse = m_mouse->GetState();
        if (mouse.positionMode == Mouse::MODE_RELATIVE)
        {
            if (!mouse.leftButton)
            {
                m_mouse->SetMode(Mouse::MODE_ABSOLUTE);
            }
            else
            {
                m_pitch -= mouse.y * c_rotationGain;
                m_yaw += mouse.x * c_rotationGain;
            }
        }
        else if (mouse.leftButton)
        {
            m_mouse->SetMode(Mouse::MODE_RELATIVE);
        }
    }

    if (kb.Escape)
    {
        ExitSample();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Q))
    {
        m_usedInstanceCount = std::max(c_minInstanceCount, m_usedInstanceCount - 1000);
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::E))
    {
        m_usedInstanceCount = std::min(c_maxInstances, m_usedInstanceCount + 1000);
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
    {
        ResetSimulation();
    }

    // Limit to avoid looking directly up or down
    const float limit = XM_PI / 2.0f - 0.01f;
    m_pitch = std::max(-limit, std::min(+limit, m_pitch));

    if (m_yaw > XM_PI)
    {
        m_yaw -= XM_PI * 2.f;
    }
    else if (m_yaw < -XM_PI)
    {
        m_yaw += XM_PI * 2.f;
    }

    XMVECTOR lookAt = XMVectorSet(
        sinf(m_yaw),
        m_pitch,
        cosf(m_yaw),
        0);

    // Update transforms and constant buffer.
    XMMATRIX camera = XMMatrixLookAtLH(g_XMZero, lookAt, g_XMIdentityR1);
    XMMATRIX proj = XMLoadFloat4x4(&m_proj);
    XMMATRIX clip = XMMatrixTranspose(XMMatrixMultiply(camera, proj));
    ReplaceBufferContents(m_vertexConstants.Get(), sizeof(XMMATRIX), &clip);

    // Overwrite our current instance vertex buffer with new data.
    ReplaceBufferContents(m_instanceData.Get(), sizeof(Instance) * m_usedInstanceCount, m_CPUInstanceData.get());

    // Update instance data for the next frame.
    for (size_t i = 1; i < m_usedInstanceCount; ++i)
    {
        // Update positions...
        float velocityMultiplier = i <= c_pointLightCount ? 5.0f * c_velocityMultiplier : c_velocityMultiplier;
        XMVECTOR position = XMLoadFloat4(&m_CPUInstanceData[i].positionAndScale);
        position += m_velocities[i] * elapsedTime * velocityMultiplier;
        XMStoreFloat4(&m_CPUInstanceData[i].positionAndScale, position);

        float X = m_CPUInstanceData[i].positionAndScale.x;
        float Y = m_CPUInstanceData[i].positionAndScale.y;
        float Z = m_CPUInstanceData[i].positionAndScale.z;

        bool bounce = false;

        // If an instance pops out of bounds in any dimension, reverse velocity in that dimension...
        if (X < -c_boxBounds || X > c_boxBounds)
        {
            m_velocities[i] *= XMVectorSet(-1.0f, 1.0f, 1.0f, 1.0f);
            bounce = true;
        }
        if (Y < -c_boxBounds || Y > c_boxBounds)
        {
            m_velocities[i] *= XMVectorSet(1.0f, -1.0f, 1.0f, 1.0f);
            bounce = true;
        }
        if (Z < -c_boxBounds || Z > c_boxBounds)
        {
            m_velocities[i] *= XMVectorSet(1.0f, 1.0f, -1.0f, 1.0f);
            bounce = true;
        }

        // Apply bounce here.
        if (bounce)
        {
            position = XMLoadFloat4(&m_CPUInstanceData[i].positionAndScale);
            position += m_velocities[i] * elapsedTime * c_velocityMultiplier;
            XMStoreFloat4(&m_CPUInstanceData[i].positionAndScale, position);
        }

        // Set up constant buffer with point light info.
        if (i <= c_pointLightCount)
        {
            m_lights.pointPositions[i - 1] = m_CPUInstanceData[i].positionAndScale;
        }

        XMVECTOR q = XMLoadFloat4(&m_CPUInstanceData[i].quaternion);
        q = XMQuaternionNormalizeEst(XMQuaternionMultiply(m_rotationQuaternions[i], q));
        XMStoreFloat4(&m_CPUInstanceData[i].quaternion, q);
    }

    // Update the D3D11 constant buffer with the new lighting constant data.
    ReplaceBufferContents(m_pixelConstants.Get(), sizeof(Lights), &m_lights);
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

    m_deviceResources->PIXBeginEvent(L"Render");
    auto context = m_deviceResources->GetD3DDeviceContext();

    // Use the default blend
    context->OMSetBlendState(nullptr, nullptr, D3D11_DEFAULT_SAMPLE_MASK);

    // Set input assembler state.
    context->IASetInputLayout(m_inputLayout.Get());

    // We're rendering a triangle list.
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set up the vertex buffers. We have 3 streams:
    // Stream 1 contains per-primitive vertices defining the cubes.
    // Stream 2 contains the per-instance data for scale, position and orientation
    // Stream 3 contains the per-instance data for color.
    UINT Strides[] = { sizeof(Vertex), sizeof(Instance), sizeof(uint32_t) };
    UINT Offsets[] = { 0, 0, 0 };
    ID3D11Buffer* Buffers[] = { m_vertexBuffer.Get(), m_instanceData.Get(), m_boxColors.Get() };
    context->IASetVertexBuffers(0, _countof(Strides), Buffers, Strides, Offsets);

    // The per-instance data is referenced by index...
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    // Apply the constants for the vertex and pixel shaders.
    context->VSSetConstantBuffers(0, 1, m_vertexConstants.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_pixelConstants.GetAddressOf());

    // Set shaders.
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    // Draw the entire scene...
    context->DrawIndexedInstanced(c_cubeIndexCount, m_usedInstanceCount, 0, 0, 0);

    // Draw UI
    auto size = m_deviceResources->GetOutputSize();
    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

    m_batch->Begin();

    wchar_t str[32] = {};
    swprintf_s(str, L"Instancing count: %u", m_usedInstanceCount);
    m_smallFont->DrawString(m_batch.get(), str, XMFLOAT2(float(safe.left), float(safe.top)), ATG::Colors::White);

    const wchar_t* legend = m_gamepadPresent
        ? L"[LThumb] Rotate   [A] Reset   [LB]/[RB] Change instance count   [View] Exit"
        : L"WASD/Left Mouse Button: Rotate   Q/E: Change instance count   Home: Center   Space: Reset   Esc: Exit";

    DX::DrawControllerString(m_batch.get(),
        m_smallFont.get(), m_ctrlFont.get(),
        legend,
        XMFLOAT2(float(safe.left),
            float(safe.bottom) - m_smallFont->GetLineSpacing()),
        ATG::Colors::LightGrey);

    m_batch->End();

    m_deviceResources->PIXEndEvent();

    // Show the new frame.
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");
    auto context = m_deviceResources->GetD3DDeviceContext();

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

    m_deviceResources->PIXEndEvent();
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
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
}

void Sample::OnWindowMoved()
{
}

void Sample::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
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

    auto context = m_deviceResources->GetD3DDeviceContext();
    m_batch = std::make_unique<SpriteBatch>(context);


    wchar_t strFilePath[MAX_PATH] = {};
    DX::FindMediaFile(strFilePath, MAX_PATH, L"Media\\Fonts\\SegoeUI_18.spritefont");
    m_smallFont = std::make_unique<SpriteFont>(device, strFilePath);

    DX::FindMediaFile(strFilePath, MAX_PATH, L"Media\\Fonts\\XboxOneControllerLegendSmall.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, strFilePath);

    // Create input layout (must match declaration of Vertex).
    static const D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
        // SemanticName SemanticIndex   Format                          InputSlot AlignedByteOffset             InputSlotClass                    InstancedDataStepRate
        { "POSITION",   0,              DXGI_FORMAT_R32G32B32_FLOAT,    0,        0,                            D3D11_INPUT_PER_VERTEX_DATA,      0 },  // Vertex local position
        { "NORMAL",     0,              DXGI_FORMAT_R32G32B32_FLOAT,    0,        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,      0 },  // Vertex normal
        { "I_ROTATION", 0,              DXGI_FORMAT_R32G32B32A32_FLOAT, 1,        0,                            D3D11_INPUT_PER_INSTANCE_DATA,    1 },  // Instance rotation quaternion
        { "I_POSSCALE", 0,              DXGI_FORMAT_R32G32B32A32_FLOAT, 1,        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA,    1 },  // Instance position and scale (scale in "w")
        { "I_COLOR",    0,              DXGI_FORMAT_R8G8B8A8_UNORM,     2,        D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA,    1 },  // Instance color
    };

    // Load and create shaders.
    {
        auto shaderBytecode = DX::ReadData(L"VertexShader.cso");

        DX::ThrowIfFailed(
            device->CreateVertexShader(shaderBytecode.data(), shaderBytecode.size(), nullptr, m_vertexShader.ReleaseAndGetAddressOf())
        );

        DX::ThrowIfFailed(
            device->CreateInputLayout(inputElementDesc, _countof(inputElementDesc), shaderBytecode.data(), shaderBytecode.size(), m_inputLayout.ReleaseAndGetAddressOf())
        );
    }

    {
        auto shaderBytecode = DX::ReadData(L"PixelShader.cso");

        DX::ThrowIfFailed(
            device->CreatePixelShader(shaderBytecode.data(), shaderBytecode.size(), nullptr, m_pixelShader.ReleaseAndGetAddressOf())
        );
    }

    // Create and initialize the vertex buffer defining a cube.
    {
        static const Vertex vertices[] =
        {
            { XMFLOAT3(-1,  -1,  -1), XMFLOAT3(0,    0,  -1) },
            { XMFLOAT3(1,  -1,  -1), XMFLOAT3(0,    0,  -1) },
            { XMFLOAT3(1,   1,  -1), XMFLOAT3(0,    0,  -1) },
            { XMFLOAT3(-1,   1,  -1), XMFLOAT3(0,    0,  -1) },    // Z negative face

            { XMFLOAT3(1,  -1,   1), XMFLOAT3(0,    0,   1) },
            { XMFLOAT3(-1,  -1,   1), XMFLOAT3(0,    0,   1) },
            { XMFLOAT3(-1,   1,   1), XMFLOAT3(0,    0,   1) },
            { XMFLOAT3(1,   1,   1), XMFLOAT3(0,    0,   1) },    // Z Positive face

            { XMFLOAT3(-1,  -1,  -1), XMFLOAT3(-1,   0,   0) },
            { XMFLOAT3(-1,   1,  -1), XMFLOAT3(-1,   0,   0) },
            { XMFLOAT3(-1,   1,   1), XMFLOAT3(-1,   0,   0) },
            { XMFLOAT3(-1,  -1,   1), XMFLOAT3(-1,   0,   0) },    // X negative face

            { XMFLOAT3(1,   1,  -1), XMFLOAT3(1,   0,   0) },
            { XMFLOAT3(1,  -1,  -1), XMFLOAT3(1,   0,   0) },
            { XMFLOAT3(1,  -1,   1), XMFLOAT3(1,   0,   0) },
            { XMFLOAT3(1,   1,   1), XMFLOAT3(1,   0,   0) },    // X Positive face

            { XMFLOAT3(-1,  -1,   1), XMFLOAT3(0,   -1,   0) },
            { XMFLOAT3(1,  -1,   1), XMFLOAT3(0,   -1,   0) },
            { XMFLOAT3(1,  -1,  -1), XMFLOAT3(0,   -1,   0) },
            { XMFLOAT3(-1,  -1,  -1), XMFLOAT3(0,   -1,   0) },    // Y negative face

            { XMFLOAT3(1,   1,   1), XMFLOAT3(0,    1,   0) },
            { XMFLOAT3(-1,   1,   1), XMFLOAT3(0,    1,   0) },
            { XMFLOAT3(-1,   1,  -1), XMFLOAT3(0,    1,   0) },
            { XMFLOAT3(1,   1,  -1), XMFLOAT3(0,    1,   0) },    // Y Positive face
        };

        D3D11_SUBRESOURCE_DATA initialData = { vertices, 0, 0 };

        CD3D11_BUFFER_DESC bufferDesc(sizeof(vertices), D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE);
        bufferDesc.StructureByteStride = sizeof(Vertex);

        DX::ThrowIfFailed(
            device->CreateBuffer(&bufferDesc, &initialData, m_vertexBuffer.ReleaseAndGetAddressOf())
        );
    }

    // Create vertex buffers with per-instance data.
    {
        CD3D11_BUFFER_DESC bufferDesc(sizeof(Instance) * c_maxInstances, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
        bufferDesc.StructureByteStride = sizeof(Instance);

        DX::ThrowIfFailed(
            device->CreateBuffer(&bufferDesc, nullptr, m_instanceData.ReleaseAndGetAddressOf())
        );
    }

    // Create a static vertex buffer with color data.
    {
        static const XMVECTORF32 c_bigColor = { 1.f, 1.f, 1.f, 0.f };
        uint32_t colors[c_maxInstances];
        colors[0] = XMCOLOR(c_bigColor);
        for (uint32_t i = 1; i < c_maxInstances; ++i)
        {
            if (i <= c_pointLightCount)
            {
                m_lights.pointColors[i - 1] = XMFLOAT4(FloatRand(0.25f, 1.0f), FloatRand(0.25f, 1.0f), FloatRand(0.25f, 1.0f), 1.0f);
                colors[i] = XMCOLOR(m_lights.pointColors[i - 1].x, m_lights.pointColors[i - 1].y, m_lights.pointColors[i - 1].z, 1.f);
            }
            else
            {
                colors[i] = XMCOLOR(FloatRand(0.25f, 1.0f), FloatRand(0.25f, 1.0f), FloatRand(0.25f, 1.0f), 0.f);
            }
        }

        D3D11_SUBRESOURCE_DATA initialData = { colors, 0, 0 };

        CD3D11_BUFFER_DESC bufferDesc(sizeof(uint32_t) * c_maxInstances, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE);
        bufferDesc.StructureByteStride = sizeof(uint32_t);

        DX::ThrowIfFailed(
            device->CreateBuffer(&bufferDesc, &initialData, m_boxColors.ReleaseAndGetAddressOf())
        );
    }

    // Create and initialize the index buffer for the cube geometry.
    {
        static const uint16_t indices[] =
        {
            0,  2,  1,
            0,  3,  2,
            4,  6,  5,
            4,  7,  6,
            8,  10, 9,
            8,  11, 10,
            12, 14, 13,
            12, 15, 14,
            16, 18, 17,
            16, 19, 18,
            20, 22, 21,
            20, 23, 22,
        };

        D3D11_SUBRESOURCE_DATA initialData = { indices, 0, 0 };

        CD3D11_BUFFER_DESC bufferDesc(sizeof(indices), D3D11_BIND_INDEX_BUFFER, D3D11_USAGE_IMMUTABLE);
        bufferDesc.StructureByteStride = sizeof(uint16_t);

        DX::ThrowIfFailed(
            device->CreateBuffer(&bufferDesc, &initialData, m_indexBuffer.ReleaseAndGetAddressOf())
        );
    }

    // Create the vertex shader constant buffer.
    {
        CD3D11_BUFFER_DESC bufferDesc(sizeof(XMMATRIX), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
        DX::ThrowIfFailed(
            device->CreateBuffer(&bufferDesc, nullptr, m_vertexConstants.ReleaseAndGetAddressOf())
        );
    }

    // Create the pixel shader (lighting) constant buffer.
    {
        static_assert((sizeof(Lights) % 16) == 0, "Constant buffer must always be 16-byte aligned");

        CD3D11_BUFFER_DESC bufferDesc(sizeof(Lights), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
        DX::ThrowIfFailed(
            device->CreateBuffer(&bufferDesc, nullptr, m_pixelConstants.ReleaseAndGetAddressOf())
        );
    }

    m_CPUInstanceData.reset(new Instance[c_maxInstances]);
    m_rotationQuaternions.reset(reinterpret_cast<XMVECTOR*>(_aligned_malloc(sizeof(XMVECTOR) * c_maxInstances, 16)));
    m_velocities.reset(reinterpret_cast<XMVECTOR*>(_aligned_malloc(sizeof(XMVECTOR) * c_maxInstances, 16)));

    // Set up the position and scale for the container box. Scale is negative to turn the box inside-out 
    // (this effectively reverses the normals and backface culling).
    // Scale the outside box to slightly larger than our scene boundary, so bouncing boxes never actually clip it.
    m_CPUInstanceData[0].positionAndScale = XMFLOAT4(0.0f, 0.0f, 0.0f, -(c_boxBounds + 5));
    m_CPUInstanceData[0].quaternion = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

    // Initialize the directional light.
    XMStoreFloat4(&m_lights.directional, XMVector3Normalize(XMVectorSet(1.0f, 4.0f, -2.0f, 0)));

    // Initialize the positions/state of all the cubes in the scene.
    ResetSimulation();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    // Initialize the projection matrix.
    auto size = m_deviceResources->GetOutputSize();

    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, float(size.right) / float(size.bottom), 0.1f, 500.0f);
    
    XMStoreFloat4x4(&m_proj, proj);
}

void Sample::OnDeviceLost()
{
    m_batch.reset();
    m_smallFont.reset();
    m_ctrlFont.reset();
    m_inputLayout.Reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_instanceData.Reset();
    m_boxColors.Reset();
    m_vertexConstants.Reset();
    m_pixelConstants.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

void Sample::ReplaceBufferContents(ID3D11Buffer* buffer, size_t bufferSize, const void* data)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    D3D11_MAPPED_SUBRESOURCE mapped;

    DX::ThrowIfFailed(
        context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)
    );

    memcpy(mapped.pData, data, bufferSize);
    context->Unmap(buffer, 0);
}

void Sample::ResetSimulation()
{
    // Reset positions to starting point, and orientations to identity.
    // Note that instance 0 is the scene bounding box, and the position, orientation and scale are static (i.e. never update).
    for (size_t i = 1; i < c_maxInstances; ++i)
    {
        m_CPUInstanceData[i].positionAndScale = XMFLOAT4(0.0f, 0.0f, c_boxBounds / 2.0f, FloatRand(0.1f, 0.4f));
        m_CPUInstanceData[i].quaternion = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

        // For the first c_pointLightCount in the updated array, we scale up by a small factor so they stand out, and
        // update the light constant data with their positions.
        if (i <= c_pointLightCount)
        {
            m_CPUInstanceData[i].positionAndScale.w = 1.53f;
            m_lights.pointPositions[i - 1] = m_CPUInstanceData[i].positionAndScale;
        }

        // Apply a random spin to each instance.
        m_rotationQuaternions[i] = XMQuaternionRotationAxis(XMVector3Normalize(XMVectorSet(FloatRand(), FloatRand(), FloatRand(), 0)), FloatRand(0.001f, 0.1f));

        // ...and a random velocity.
        m_velocities[i] = XMVectorSet(FloatRand(-0.01f, 0.01f), FloatRand(-0.01f, 0.01f), FloatRand(-0.01f, 0.01f), 0);
    }
}

inline float Sample::FloatRand(float lowerBound, float upperBound)
{
    if (lowerBound == upperBound)
        return lowerBound;

    std::uniform_real_distribution<float> dist(lowerBound, upperBound);

    return dist(m_randomEngine);
}
