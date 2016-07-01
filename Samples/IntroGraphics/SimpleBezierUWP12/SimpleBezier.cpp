//--------------------------------------------------------------------------------------
// SimpleBezier.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleBezier.h"

#include "ATGColors.h"
#include "ReadData.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

#pragma region Globals
// Global variables
namespace
{
    // Min and max divisions of the patch per side for the slider control.
    const float c_minDivs = 4;
    const float c_maxDivs = 16;
    // Startup subdivisions per side.
    const float c_defaultSubdivs = 8.0f;
    // Camera's rotation angle per step.
    const float c_rotationAnglePerStep = XM_2PI / 360.0f;

    // Initial camera setup
    const XMVECTORF32 c_cameraEye = { 0.0f, 0.45f, 2.7f, 0.0f };
    const XMVECTORF32 c_cameraAt = { 0.0f, 0.0f, 0.0f, 0.0f };
    const XMVECTORF32 c_cameraUp = { 0.0f, 1.0f, 0.0f, 0.0f };

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

    inline size_t AlignSize(size_t size, size_t alignment)
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }
}
#pragma endregion

Sample::Sample()
    : m_subdivs(c_defaultSubdivs)
    , m_drawWires(c_defaultWireframeRendering)
    , m_partitionMode(PartitionMode::PartitionInteger)
    , m_mappedConstantData(nullptr)
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
void Sample::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Escape) || pad.IsViewPressed())
    {
        Windows::ApplicationModel::Core::CoreApplication::Exit();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::W) || m_gamePadButtons.y == GamePad::ButtonStateTracker::PRESSED)
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
         m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED)
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

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    //Set appropriate pipeline state.
    commandList->SetPipelineState(m_PSOs[m_drawWires ? 1 : 0][(int)m_partitionMode].Get());

    // Set root signature and descriptor heaps.
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Calculate world-view-projection matrix.
    XMMATRIX view = XMLoadFloat4x4(&m_viewMatrix);
    XMMATRIX projection = XMLoadFloat4x4(&m_projectionMatrix);
    XMMATRIX viewProjectionMatrix = XMMatrixMultiply(view, projection);

    // Update per-frame variables.
    if (m_mappedConstantData != nullptr)
    {
        XMStoreFloat4x4(&m_mappedConstantData->viewProjectionMatrix, viewProjectionMatrix);
        m_mappedConstantData->cameraWorldPos = m_cameraEye;
        m_mappedConstantData->tessellationFactor = (float)m_subdivs;
    }

    // Finalize dynamic constant buffer into descriptor heap.
    commandList->SetGraphicsRootDescriptorTable(c_rootParameterCB, m_resourceDescriptors->GetGpuHandle(c_rootParameterCB));

    commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST);
    commandList->IASetVertexBuffers(0, 1, &m_controlPointVBView);

    // Draw the mesh
    commandList->DrawInstanced(_countof(c_mobiusStrip), 1, 0, 0);

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
    PIXEndEvent(m_deviceResources->GetCommandQueue());
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    // Use linear clear color for gamma-correct rendering.
    commandList->ClearRenderTargetView(rtvDescriptor, ATG::ColorsLinear::Background, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
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

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    CreateShaders();

    // Initialize the world and view matrices.
    XMMATRIX world = XMMatrixIdentity();
    XMMATRIX view = XMMatrixLookAtLH(c_cameraEye, c_cameraAt, c_cameraUp);
    XMStoreFloat4x4(&m_worldMatrix, world);
    XMStoreFloat4x4(&m_viewMatrix, view);
    XMStoreFloat3(&m_cameraEye, c_cameraEye);
}

// Creates and initializes shaders and their data.
void Sample::CreateShaders()
{
    auto device = m_deviceResources->GetD3DDevice();
    
    {
        // Define root table layout.
        CD3DX12_DESCRIPTOR_RANGE descRange[1];
        descRange[c_rootParameterCB].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
        CD3DX12_ROOT_PARAMETER rootParameters[1];
        rootParameters[c_rootParameterCB].InitAsDescriptorTable(
            1, &descRange[c_rootParameterCB], D3D12_SHADER_VISIBILITY_ALL); // b0

        // Create the root signature.
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        DX::ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        DX::ThrowIfFailed(
            device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));
    }
    
    // Create our vertex input layout.
    const D3D12_INPUT_ELEMENT_DESC c_inputElementDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // Load shaders.
    auto vertexShaderBlob = DX::ReadData(L"BezierVS.cso");
    
    std::vector<uint8_t> hullShaderBlobs[c_numHullShaders];
    hullShaderBlobs[(int)PartitionMode::PartitionInteger] = DX::ReadData(L"BezierHS_int.cso");
    hullShaderBlobs[(int)PartitionMode::PartitionFractionalEven] = DX::ReadData(L"BezierHS_fracEven.cso");
    hullShaderBlobs[(int)PartitionMode::PartitionFractionalOdd] = DX::ReadData(L"BezierHS_fracOdd.cso");
    
    auto domainShaderBlob = DX::ReadData(L"BezierDS.cso");

    std::vector<uint8_t> pixelShaderBlobs[c_numPixelShaders];
    pixelShaderBlobs[0] = DX::ReadData(L"BezierPS.cso");
    pixelShaderBlobs[1] = DX::ReadData(L"SolidColorPS.cso");

    // Create solid and wireframe rasterizer state objects.
    D3D12_RASTERIZER_DESC RasterDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    RasterDesc.CullMode = D3D12_CULL_MODE_NONE;
    RasterDesc.DepthClipEnable = TRUE;

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout.pInputElementDescs = c_inputElementDesc;
    psoDesc.InputLayout.NumElements = _countof(c_inputElementDesc);
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.data(), vertexShaderBlob.size());
    psoDesc.DS = CD3DX12_SHADER_BYTECODE(domainShaderBlob.data(), domainShaderBlob.size());
    psoDesc.RasterizerState = RasterDesc;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_deviceResources->GetBackBufferFormat();
    psoDesc.DSVFormat = m_deviceResources->GetDepthBufferFormat();
    psoDesc.SampleDesc.Count = 1;

    // Enumerate PSOs.
    D3D12_FILL_MODE fillModes[] = { D3D12_FILL_MODE_SOLID, D3D12_FILL_MODE_WIREFRAME };
    for (uint8_t i = 0; i < c_numPixelShaders; i++)
    {
        psoDesc.RasterizerState.FillMode = fillModes[i];
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlobs[i].data(), pixelShaderBlobs[i].size());

        for (uint8_t j = 0; j < c_numHullShaders; j++)
        {
            psoDesc.HS = CD3DX12_SHADER_BYTECODE(hullShaderBlobs[j].data(), hullShaderBlobs[j].size());

            DX::ThrowIfFailed(
                device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_PSOs[i][j].ReleaseAndGetAddressOf())));
        }
    }
    
    {
        // Create constant buffer.
        const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        size_t cbSize = AlignSize(sizeof(ConstantBuffer), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
        DX::ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
            &constantBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_cbPerFrame.GetAddressOf())));
        DX::ThrowIfFailed(m_cbPerFrame->SetName(L"Per Frame CB"));

        // Map it to a CPU variable. Leave the mapping active for per-frame updates.
        DX::ThrowIfFailed(m_cbPerFrame->Map(0, nullptr, reinterpret_cast< void** >(&m_mappedConstantData)));

        // Create constant buffer view.
        const uint32_t c_cbCount = 1;
        m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, c_cbCount);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_cbPerFrame->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = UINT(cbSize);
        device->CreateConstantBufferView(&cbvDesc, m_resourceDescriptors->GetCpuHandle(c_rootParameterCB));
        
        // Create vertex buffer containing a mesh's control points.
        // Note: Using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are few verts to actually transfer.        
        const D3D12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(c_mobiusStrip));
        DX::ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
            &vertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_controlPointVB.GetAddressOf())));
        DX::ThrowIfFailed(m_controlPointVB->SetName(L"Control Point VB"));

        // Copy the MobiusStrip data to the vertex buffer.
        uint8_t* dataBegin;
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(m_controlPointVB->Map(
            0, &readRange, reinterpret_cast<void**>(&dataBegin)));
        memcpy(dataBegin, c_mobiusStrip, sizeof(c_mobiusStrip));
        m_controlPointVB->Unmap(0, nullptr);

        // Initialize vertex buffer view.
        ZeroMemory(&m_controlPointVBView, sizeof(m_controlPointVBView));
        m_controlPointVBView.BufferLocation = m_controlPointVB->GetGPUVirtualAddress();
        m_controlPointVBView.StrideInBytes = sizeof(XMFLOAT3);
        m_controlPointVBView.SizeInBytes = sizeof(c_mobiusStrip);
    }

    // Wait until assets have been uploaded to the GPU.
    m_deviceResources->WaitForGpu();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();

    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, float(size.right) / float(size.bottom), 0.01f, 100.0f);

    XMFLOAT4X4 orient = m_deviceResources->GetOrientationTransform3D();

    XMStoreFloat4x4(&m_projectionMatrix, projection * XMLoadFloat4x4(&orient));
}

void Sample::OnDeviceLost()
{
    m_graphicsMemory.reset();
    m_rootSignature.Reset();
    m_resourceDescriptors.reset();

    for (uint8_t i = 0; i < c_numPixelShaders; i++)
    {
        for (uint8_t j = 0; j < c_numHullShaders; j++)
        {
            m_PSOs[i][j].Reset();
        }
    }

    m_controlPointVB.Reset();
    m_cbPerFrame.Reset();
    m_mappedConstantData = nullptr;
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
