//--------------------------------------------------------------------------------------
// SimpleBezier.cpp
//
// This sample demonstrates the basic usage of the DirectX 11 tessellation feature to 
// render a simple cubic Bezier patch.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleBezierPC.h"

#include "DirectXHelpers.h"
#include "ATGColors.h"
#include "ControllerFont.h"
#include "ReadData.h"
#include "FindMedia.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

#pragma region Globals
// Global variables
namespace
{
    // Help menu text
    const wchar_t* c_sampleTitle = L"Simple Bezier Sample";
    const wchar_t* c_sampleDescription = L"Demonstrates how to create hull and domain shaders to draw a\ntessellated Bezier surface representing a Mobius strip.";
    const ATG::HelpButtonAssignment c_helpButtons[] = {
        { ATG::HelpID::MENU_BUTTON,      L"Show/Hide Help" },
        { ATG::HelpID::VIEW_BUTTON,      L"Exit" },
        { ATG::HelpID::LEFT_STICK,       L"Rotate Camera" },
        { ATG::HelpID::LEFT_TRIGGER,     L"Decrease Subdivisions" },
        { ATG::HelpID::RIGHT_TRIGGER,    L"Increase Subdivisions" },
        { ATG::HelpID::Y_BUTTON,         L"Toggle Wireframe" },
        { ATG::HelpID::A_BUTTON,         L"Fractional Partitioning (Even)" },
        { ATG::HelpID::B_BUTTON,         L"Fractional Partitioning (Odd)" },
        { ATG::HelpID::X_BUTTON,         L"Integer Partitioning" },
    };

    // Min and max divisions of the patch per side for the slider control
    const float c_minDivs = 4;
    const float c_maxDivs = 16;
    // Startup subdivisions per side
    const float c_defaultSubdivs = 8.0f;
    // Camera's rotation angle per step
    const float c_rotationAnglePerStep = XM_2PI / 360.0f;

    // Initial camera setup
    const XMVECTOR c_cameraEye = XMVectorSet(0.0f, 0.45f, 2.7f, 0.0f);
    const XMVECTOR c_cameraAt = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    const XMVECTOR c_cameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // Draw the mesh with shaded triangles at start.
    const bool c_defaultWireframeRendering = false;

    // Simple Bezier patch for a Mobius strip.
    // 4 patches with 16 control points each.
    const XMFLOAT3 c_mobiusStrip[64] = {
        { 1.0f, -0.5f, 0.0f },
        { 1.0f, -0.5f, 0.5f },
        { 0.5f, -0.3536f, 1.354f },
        { 0.0f, -0.3536f, 1.354f },
        { 1.0f, -0.1667f, 0.0f },
        { 1.0f, -0.1667f, 0.5f },
        { 0.5f, -0.1179f, 1.118f },
        { 0.0f, -0.1179f, 1.118f },
        { 1.0f, 0.1667f, 0.0f },
        { 1.0f, 0.1667f, 0.5f },
        { 0.5f, 0.1179f, 0.8821f },
        { 0.0f, 0.1179f, 0.8821f },
        { 1.0f, 0.5f, 0.0f },
        { 1.0f, 0.5f, 0.5f },
        { 0.5f, 0.3536f, 0.6464f },
        { 0.0f, 0.3536f, 0.6464f },
        { 0.0f, -0.3536f, 1.354f },
        { -0.5f, -0.3536f, 1.354f },
        { -1.5f, 0.0f, 0.5f },
        { -1.5f, 0.0f, 0.0f },
        { 0.0f, -0.1179f, 1.118f },
        { -0.5f, -0.1179f, 1.118f },
        { -1.167f, 0.0f, 0.5f },
        { -1.167f, 0.0f, 0.0f },
        { 0.0f, 0.1179f, 0.8821f },
        { -0.5f, 0.1179f, 0.8821f },
        { -0.8333f, 0.0f, 0.5f },
        { -0.8333f, 0.0f, 0.0f },
        { 0.0f, 0.3536f, 0.6464f },
        { -0.5f, 0.3536f, 0.6464f },
        { -0.5f, 0.0f, 0.5f },
        { -0.5f, 0.0f, 0.0f },
        { -1.5f, 0.0f, 0.0f },
        { -1.5f, 0.0f, -0.5f },
        { -0.5f, 0.3536f, -1.354f },
        { 0.0f, 0.3536f, -1.354f },
        { -1.167f, 0.0f, 0.0f },
        { -1.167f, 0.0f, -0.5f },
        { -0.5f, 0.1179f, -1.118f },
        { 0.0f, 0.1179f, -1.118f },
        { -0.8333f, 0.0f, 0.0f },
        { -0.8333f, 0.0f, -0.5f },
        { -0.5f, -0.1179f, -0.8821f },
        { 0.0f, -0.1179f, -0.8821f },
        { -0.5f, 0.0f, 0.0f },
        { -0.5f, 0.0f, -0.5f },
        { -0.5f, -0.3536f, -0.6464f },
        { 0.0f, -0.3536f, -0.6464f },
        { 0.0f, 0.3536f, -1.354f },
        { 0.5f, 0.3536f, -1.354f },
        { 1.0f, 0.5f, -0.5f },
        { 1.0f, 0.5f, 0.0f },
        { 0.0f, 0.1179f, -1.118f },
        { 0.5f, 0.1179f, -1.118f },
        { 1.0f, 0.1667f, -0.5f },
        { 1.0f, 0.1667f, 0.0f },
        { 0.0f, -0.1179f, -0.8821f },
        { 0.5f, -0.1179f, -0.8821f },
        { 1.0f, -0.1667f, -0.5f },
        { 1.0f, -0.1667f, 0.0f },
        { 0.0f, -0.3536f, -0.6464f },
        { 0.5f, -0.3536f, -0.6464f },
        { 1.0f, -0.5f, -0.5f },
        { 1.0f, -0.5f, 0.0f },
    };
}
#pragma endregion

Sample::Sample()
    : m_subdivs(c_defaultSubdivs)
    , m_drawWires(c_defaultWireframeRendering)
    , m_partitionMode(PartitionMode::PartitionInteger)
    , m_showHelp(false)
    , m_ctrlConnected(false)
{
    // Use gamma-correct rendering.  Hardware tessellation requires Feature Level 11.0 or later.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0);
    m_deviceResources->RegisterDeviceNotify(this);

    m_help = std::make_unique<ATG::Help>(c_sampleTitle, c_sampleDescription, c_helpButtons, _countof(c_helpButtons), true);
}

// Initializes the Direct3D resources required to run.
void Sample::Initialize(HWND window, int width, int height)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();

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
void Sample::Update(DX::StepTimer const&)
{
    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_ctrlConnected = true;
        m_gamePadButtons.Update(pad);
    }
    else
    {
        m_ctrlConnected = false;
        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if ((!m_showHelp && m_keyboardButtons.IsKeyPressed(Keyboard::Escape)) || pad.IsViewPressed())
    {
        ExitSample();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::F1) || m_gamePadButtons.menu == GamePad::ButtonStateTracker::PRESSED)
    {
        m_showHelp = !m_showHelp;
    }
    else if (m_showHelp && (m_keyboardButtons.IsKeyPressed(Keyboard::Escape) || m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED))
    {
        m_showHelp = false;
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::W) || m_gamePadButtons.y == GamePad::ButtonStateTracker::PRESSED)
    {
        m_drawWires = !m_drawWires;
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::D1) || m_keyboardButtons.IsKeyPressed(Keyboard::NumPad1) ||
        m_gamePadButtons.x == GamePad::ButtonStateTracker::PRESSED)
    {
        m_partitionMode = PartitionMode::PartitionInteger;
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::D2) || m_keyboardButtons.IsKeyPressed(Keyboard::NumPad2) ||
        m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED)
    {
        m_partitionMode = PartitionMode::PartitionFractionalEven;
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::D3) || m_keyboardButtons.IsKeyPressed(Keyboard::NumPad3) ||
        (!m_showHelp && m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED))
    {
        m_partitionMode = PartitionMode::PartitionFractionalOdd;
    }

    if (kb.Down || pad.IsLeftTriggerPressed())
    {
        m_subdivs = std::max(m_subdivs - 0.1f, c_minDivs);
    }

    if (kb.Up || pad.IsRightTriggerPressed())
    {
        m_subdivs = std::min(m_subdivs + 0.1f, c_maxDivs);
    }

    float rotationAxisY = 0.0f;

    if (pad.thumbSticks.leftX != 0.0f)
    {
        rotationAxisY = -pad.thumbSticks.leftX * c_rotationAnglePerStep;
    }
    else if (kb.Left)
    {
        rotationAxisY = c_rotationAnglePerStep;
    }
    else if (kb.Right)
    {
        rotationAxisY = -c_rotationAnglePerStep;
    }

    if (rotationAxisY != 0.0f)
    {
        XMVECTOR eye = XMLoadFloat3(&m_cameraEye);
        eye = XMVector3Transform(eye, XMMatrixRotationY(rotationAxisY));
        XMMATRIX view = XMMatrixLookAtLH(eye, c_cameraAt, c_cameraUp);
        XMStoreFloat4x4(&m_viewMatrix, view);
        XMStoreFloat3(&m_cameraEye, eye);
    }
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

    if (m_showHelp)
    {
        //Clear hull and domain shaders so help can render.
        context->HSSetShader(nullptr, nullptr, 0);
        context->DSSetShader(nullptr, nullptr, 0);

        m_help->Render();
    }
    else
    {
        //Reset state that may have been modified by showing help.
        context->OMSetBlendState(m_states->Opaque(), Colors::Black, 0xFFFFFFFF);
        context->OMSetDepthStencilState(m_states->DepthDefault(), 0);

        XMMATRIX view = XMLoadFloat4x4(&m_viewMatrix);
        XMMATRIX projection = XMLoadFloat4x4(&m_projectionMatrix);
        XMMATRIX viewProjectionMatrix = XMMatrixMultiply(view, projection);

        // Update per-frame variables.
        auto d3dBuffer = m_cbPerFrame.Get();

        {
            MapGuard mappedResource(context, d3dBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0);

            ConstantBuffer* data = reinterpret_cast<ConstantBuffer*>(mappedResource.get());

            XMStoreFloat4x4(&data->viewProjectionMatrix, viewProjectionMatrix);
            data->cameraWorldPos = m_cameraEye;
            data->tessellationFactor = (float)m_subdivs;
        }

        // Render the meshes.
        // Bind all of the CBs.
        context->VSSetConstantBuffers(0, 1, &d3dBuffer);
        context->HSSetConstantBuffers(0, 1, &d3dBuffer);
        context->DSSetConstantBuffers(0, 1, &d3dBuffer);
        context->PSSetConstantBuffers(0, 1, &d3dBuffer);

        // Set the shaders.
        context->VSSetShader(m_vertexShader.Get(), nullptr, 0);

        // For this sample, choose either the "integer", "fractional_even",
        // or "fractional_odd" hull shader.
        if (m_partitionMode == PartitionMode::PartitionInteger)
        {
            context->HSSetShader(m_hullShaderInteger.Get(), nullptr, 0);
        }
        else if (m_partitionMode == PartitionMode::PartitionFractionalEven)
        {
            context->HSSetShader(m_hullShaderFracEven.Get(), nullptr, 0);
        }
        else if (m_partitionMode == PartitionMode::PartitionFractionalOdd)
        {
            context->HSSetShader(m_hullShaderFracOdd.Get(), nullptr, 0);
        }

        context->DSSetShader(m_domainShader.Get(), nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);

        // Optionally draw the wireframe.
        if (m_drawWires)
        {
            context->PSSetShader(m_solidColorPS.Get(), nullptr, 0);
            context->RSSetState(m_states->Wireframe());
        }
        else
        {
            context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
            context->RSSetState(m_states->CullNone());
        }

        // Set the input assembler.
        // This sample uses patches with 16 control points each.
        // Although the Mobius strip only needs to use a vertex buffer,
        // you can use an index buffer as well by calling IASetIndexBuffer().
        context->IASetInputLayout(m_inputLayout.Get());
        UINT stride = sizeof(XMFLOAT3);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, m_controlPointVB.GetAddressOf(), &stride, &offset);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST);

        // Draw the mesh
        context->Draw(_countof(c_mobiusStrip), 0);

        // Draw the UI        
        context->HSSetShader(nullptr, nullptr, 0);
        context->DSSetShader(nullptr, nullptr, 0);

        auto size = m_deviceResources->GetOutputSize();
        auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

        m_batch->Begin();

        wchar_t str[64] = {};
        swprintf_s(str, L"Subdivisions: %.2f   Partition Mode: %ls", m_subdivs,
            m_partitionMode == PartitionMode::PartitionInteger ? L"Integer" :
            (m_partitionMode == PartitionMode::PartitionFractionalEven ? L"Fractional Even" : L"Fractional Odd"));
        m_smallFont->DrawString(m_batch.get(), str, XMFLOAT2(float(safe.left), float(safe.top)), ATG::Colors::LightGrey);

        const wchar_t* legend = m_ctrlConnected ?
            L"[LThumb] Rotate   [RT][LT] Increase/decrease subdivisions\n[A][B][X] Change partition mode   [Y] Toggle wireframe   [View] Exit   [Menu] Help"
            : L"Left/Right - Rotate   Up/Down - Increase/decrease subdivisions\n1/2/3 - Change partition mode   W - Toggle wireframe   Esc - Exit   F1 - Help";
        DX::DrawControllerString(m_batch.get(), m_smallFont.get(), m_ctrlFont.get(),
            legend,
            XMFLOAT2(float(safe.left), float(safe.bottom) - 2 * m_smallFont->GetLineSpacing()),
            ATG::Colors::LightGrey);

        m_batch->End();
    }

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

    m_states = std::make_unique<CommonStates>(device);

    CreateShaders();

    // Initialize the world and view matrices.
    XMMATRIX world = XMMatrixIdentity();
    XMMATRIX view = XMMatrixLookAtLH(c_cameraEye, c_cameraAt, c_cameraUp);
    XMStoreFloat4x4(&m_worldMatrix, world);
    XMStoreFloat4x4(&m_viewMatrix, view);
    XMStoreFloat3(&m_cameraEye, c_cameraEye);

    m_batch = std::make_unique<SpriteBatch>(context);

    wchar_t strFilePath[MAX_PATH] = {};
    DX::FindMediaFile(strFilePath, MAX_PATH, L"SegoeUI_18.spritefont");
    m_smallFont = std::make_unique<SpriteFont>(device, strFilePath);

    DX::FindMediaFile(strFilePath, MAX_PATH, L"XboxOneControllerLegendSmall.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, strFilePath);

    m_help->RestoreDevice(context);
}

// Creates and initializes shaders and their data.
void Sample::CreateShaders()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Load and create shaders.
    auto vertexShaderBlob = DX::ReadData(L"BezierVS.cso");
    DX::ThrowIfFailed(
        device->CreateVertexShader(vertexShaderBlob.data(), vertexShaderBlob.size(),
            nullptr, m_vertexShader.ReleaseAndGetAddressOf()));

    auto hullShaderIntegerBlob = DX::ReadData(L"BezierHS_int.cso");
    DX::ThrowIfFailed(
        device->CreateHullShader(hullShaderIntegerBlob.data(), hullShaderIntegerBlob.size(),
            nullptr, m_hullShaderInteger.ReleaseAndGetAddressOf()));

    auto hullShaderFracEvenBlob = DX::ReadData(L"BezierHS_fracEven.cso");
    DX::ThrowIfFailed(
        device->CreateHullShader(hullShaderFracEvenBlob.data(), hullShaderFracEvenBlob.size(),
            nullptr, m_hullShaderFracEven.ReleaseAndGetAddressOf()));

    auto hullShaderFracOddBlob = DX::ReadData(L"BezierHS_fracOdd.cso");
    DX::ThrowIfFailed(
        device->CreateHullShader(hullShaderFracOddBlob.data(), hullShaderFracOddBlob.size(),
            nullptr, m_hullShaderFracOdd.ReleaseAndGetAddressOf()));

    auto domainShaderBlob = DX::ReadData(L"BezierDS.cso");
    DX::ThrowIfFailed(
        device->CreateDomainShader(domainShaderBlob.data(), domainShaderBlob.size(),
            nullptr, m_domainShader.ReleaseAndGetAddressOf()));

    auto pixelShaderBlob = DX::ReadData(L"BezierPS.cso");
    DX::ThrowIfFailed(
        device->CreatePixelShader(pixelShaderBlob.data(), pixelShaderBlob.size(),
            nullptr, m_pixelShader.ReleaseAndGetAddressOf()));

    auto solidColorPSBlob = DX::ReadData(L"SolidColorPS.cso");
    DX::ThrowIfFailed(
        device->CreatePixelShader(solidColorPSBlob.data(), solidColorPSBlob.size(),
            nullptr, m_solidColorPS.ReleaseAndGetAddressOf()));


    // Create our vertex input layout.
    static const D3D11_INPUT_ELEMENT_DESC c_inputElementDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    DX::ThrowIfFailed(
        device->CreateInputLayout(c_inputElementDesc, _countof(c_inputElementDesc),
            vertexShaderBlob.data(), vertexShaderBlob.size(),
            m_inputLayout.ReleaseAndGetAddressOf()));


    // Create constant buffers.
    D3D11_BUFFER_DESC bufferDesc;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;
    bufferDesc.ByteWidth = sizeof(ConstantBuffer);

    DX::ThrowIfFailed(device->CreateBuffer(&bufferDesc, nullptr, m_cbPerFrame.ReleaseAndGetAddressOf()));


    // Create vertex buffer.
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(XMFLOAT3) * ARRAYSIZE(c_mobiusStrip);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbInitData = {};
    vbInitData.pSysMem = c_mobiusStrip;

    DX::ThrowIfFailed(device->CreateBuffer(&vbDesc, &vbInitData, m_controlPointVB.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();

    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, float(size.right) / float(size.bottom), 0.01f, 100.0f);
    XMStoreFloat4x4(&m_projectionMatrix, projection);

    m_batch->SetViewport(m_deviceResources->GetScreenViewport());

    m_help->SetWindow(size);
}

void Sample::OnDeviceLost()
{
    m_states.reset();
    m_inputLayout.Reset();
    m_vertexShader.Reset();
    m_hullShaderInteger.Reset();
    m_hullShaderFracEven.Reset();
    m_hullShaderFracOdd.Reset();
    m_domainShader.Reset();
    m_pixelShader.Reset();
    m_solidColorPS.Reset();
    m_controlPointVB.Reset();
    m_cbPerFrame.Reset();
    m_rasterizerStateSolid.Reset();
    m_rasterizerStateWireframe.Reset();

    m_batch.reset();
    m_smallFont.reset();
    m_ctrlFont.reset();

    m_help->ReleaseDevice();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
