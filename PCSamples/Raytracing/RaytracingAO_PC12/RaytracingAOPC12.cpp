//--------------------------------------------------------------------------------------
// D3D12RaytracingAO.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "RaytracingAOPC12.h"

#include "ATGColors.h"
#include "FindMedia.h"
#include "RayTracingHelper.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
    const float c_camStep = .1f;
    const float c_initialRadius = -20.f;

    const wchar_t* s_assetList[2] = {
        L"Media\\Meshes\\Dragon\\Dragon.sdkmesh",
        L"Media\\Meshes\\Maze\\Maze1.sdkmesh",
    };
}

Sample::Sample() noexcept(false) :
    m_meshIndex(0),
    m_isSplit(true),
    m_isSplitMode(false),
    m_radius(c_initialRadius)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

Sample::~Sample()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(HWND window, int width, int height)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();

    m_menus = std::make_unique<Menus>();

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

// Update camera matrices passed into the shader.
void Sample::UpdateCameraMatrices()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();
    auto output = m_deviceResources->GetOutputSize();

    auto screenWidth = float(output.right - output.left) * (m_isSplit ? .5f : 1.f);
    auto screenHeight = float(output.bottom - output.top);

    float fovAngleY = 45.0f;
    XMVECTOR updatedEye = { 0, 0, m_radius, 1 };

    XMMATRIX view = XMMatrixLookAtLH(updatedEye, g_XMIdentityR3, g_XMIdentityR1);
    float aspectRatio = screenWidth / screenHeight;
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), aspectRatio, NEAR_PLANE, FAR_PLANE);
    XMMATRIX viewProj = view * proj;

    m_sceneCB[frameIndex].cameraPosition = updatedEye;
    m_sceneCB[frameIndex].projectionToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj));

    m_sceneCB[frameIndex].worldView = XMMatrixTranspose(view);
    m_sceneCB[frameIndex].worldViewProjection = XMMatrixTranspose(viewProj);

    // Update frustum.
    {
        BoundingFrustum bf;
        BoundingFrustum::CreateFromMatrix(bf, proj);

        XMMATRIX viewToWorld = XMMatrixInverse(nullptr, view);

        XMFLOAT3 corners[BoundingFrustum::CORNER_COUNT];
        bf.GetCorners(corners);

        auto lowerLeft = XMVector3Transform(
            XMLoadFloat3(&corners[7]),
            viewToWorld
        );
        auto lowerRight = XMVector3Transform(
            XMLoadFloat3(&corners[6]),
            viewToWorld
        );
        auto topLeft = XMVector3Transform(
            XMLoadFloat3(&corners[4]),
            viewToWorld
        );

        XMVECTOR point = XMVectorSubtract(topLeft, updatedEye);
        XMVECTOR horizDelta = XMVectorSubtract(lowerRight, lowerLeft);
        XMVECTOR vertDelta = XMVectorSubtract(lowerLeft, topLeft);

        m_sceneCB[frameIndex].frustumPoint = point;
        m_sceneCB[frameIndex].frustumHDelta = horizDelta;
        m_sceneCB[frameIndex].frustumVDelta = vertDelta;
    }

    DirectX::XMMATRIX identityMatrix = DirectX::XMMatrixIdentity();

    if (m_ao)
        m_ao->OnCameraChanged(identityMatrix, view, proj);
    if (m_ssao)
        m_ssao->OnCameraChanged(identityMatrix, view, proj);
}

// Initialize scene rendering parameters.
void Sample::InitializeScene()
{
    auto frameIndex     = m_deviceResources->GetCurrentFrameIndex();
    auto frameCount     = m_deviceResources->GetBackBufferCount();

    auto output = m_deviceResources->GetOutputSize();
    float screenWidth    = float(output.right - output.left) * (m_isSplit ? .5f : 1.f);
    float screenHeight   = float(output.bottom - output.top);

    // Setup camera.
    {
        // Allocate for the frame size.
        m_sceneCB.resize(frameCount);

        UpdateCameraMatrices();
    }

    // Setup noise tile.
    {
        m_sceneCB[frameIndex].noiseTile = {
            screenWidth / float(NOISE_W),
            screenHeight / float(NOISE_W),
            0,
            0
        };
    }

    // Apply the initial values to all of the frames' buffer instances.
    for (auto& sceneCB : m_sceneCB)
    {
        sceneCB = m_sceneCB[frameIndex];
    }

    // Assign split.
    {
        m_isSplit = true;
        m_isSplitMode = false;
    }
}

// Create constant buffers.
void Sample::CreateConstantBuffers()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto frameCount = m_deviceResources->GetBackBufferCount();

    // Create a constant buffer for each frame to solve read and write conflicts.
    {
        m_mappedSceneConstantResource.resize(frameCount);
        m_mappedSceneConstantData.resize(frameCount);

        for (unsigned int i = 0; i < frameCount; i++)
        {
            AllocateUploadBuffer(device, nullptr, sizeof(*m_mappedSceneConstantData[i]), &m_mappedSceneConstantResource[i]);

            // Map the constant buffer and cache its heap pointers.
            // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
            m_mappedSceneConstantResource[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedSceneConstantData[i]));
        }
    }
}

void Sample::SetupLighting()
{
    // Setup the lighting model.
    m_ao->Setup(m_deviceResources);
    m_ssao->Setup(m_deviceResources);

    // Send mesh to the lighting model.
    m_ao->SetMesh(m_mesh);
    m_ssao->SetMesh(m_mesh);
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
    UNREFERENCED_PARAMETER(timer);

    auto device = m_deviceResources->GetD3DDevice();
    auto commandQueue = m_deviceResources->GetCommandQueue();
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    auto output = m_deviceResources->GetOutputSize();
    float screenWidth = float(output.right - output.left) * (m_isSplit ? .5f : 1.f);
    float screenHeight = float(output.bottom - output.top) * (m_isSplit ? .5f : 1.f);

    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

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
    else if (kb.A)
    {
        m_radius += c_camStep;
    }
    else if (kb.D)
    {
        m_radius -= c_camStep;
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
    {
        m_meshIndex++;

        if (m_meshFiles.size() <= m_meshIndex)
            m_meshIndex = 0;

        // Wait for frames to finish to change the model.
        m_deviceResources->WaitForGpu();

        // Clear all references to mesh to free memory before the load.
        m_ao->SetMesh({ nullptr });
        m_ssao->SetMesh({ nullptr });
        m_mesh.reset();

        // Load in mesh.
        m_mesh = std::make_shared<Mesh>(
            Mesh(
                device,
                commandQueue,
                std::wstring(m_meshFiles[m_meshIndex].begin(),
                    m_meshFiles[m_meshIndex].end()).c_str())
            );

        // Send mesh to the lighting model.
        m_ao->SetMesh(m_mesh);
        m_ssao->SetMesh(m_mesh);
    }
    else if (m_keyboardButtons.IsKeyPressed(Keyboard::S))
    {
        m_isSplit = !m_isSplit;
    }

    bool update = m_menus->ProcessKeys(m_keyboardButtons);

    // Update AO and SSAO CBVs.
    if (update)
    {
        // Wait on GPU to finish.
        m_deviceResources->WaitForGpu();

        // Update options.
        m_ao->OnOptionUpdate(m_menus);
        m_ssao->OnOptionUpdate(m_menus);

        // Wait on upload.
        m_deviceResources->WaitForGpu();
    }

    // Update world.
    {
        m_sceneCB[frameIndex].noiseTile = {
            screenWidth / float(NOISE_W),
            screenHeight / float(NOISE_W),
            0,
            0
        };

        UpdateCameraMatrices();
    }

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render

// Draws the scene.
void Sample::Render()
{
    auto frameIndex = m_deviceResources->GetCurrentFrameIndex();

    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    // Copy the updated scene constant buffer to the GPU.
    {
        memcpy(&m_mappedSceneConstantData[frameIndex]->constants, &m_sceneCB[frameIndex], sizeof(m_sceneCB[frameIndex]));
    }

    // Process split.
    if (m_isSplit && !m_isSplitMode)
    {
        m_isSplitMode = true;
        m_ssao->ChangeScreenScale(.5f);
        m_ao->ChangeScreenScale(.5f);
    }
    else if (!m_isSplit && m_isSplitMode)
    {
        m_isSplitMode = false;
        m_ssao->ChangeScreenScale(1.f);
        m_ao->ChangeScreenScale(1.f);
    }

    // Apply lighting model.
    if (m_isSplit)
    {
        m_ao->Run(m_mappedSceneConstantResource[frameIndex].Get());
        m_ssao->Run(m_mappedSceneConstantResource[frameIndex].Get());
    }
    else if (m_menus->m_lightingModel == MenuLightingModel::AO)
    {
        m_ao->Run(m_mappedSceneConstantResource[frameIndex].Get());
    }
    else if (m_menus->m_lightingModel == MenuLightingModel::SSAO)
    {
        m_ssao->Run(m_mappedSceneConstantResource[frameIndex].Get());
    }

    // Draw HUD.
    m_menus->Draw(m_timer.GetFramesPerSecond(), m_isSplit);

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

void Sample::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
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
    auto commandQueue = m_deviceResources->GetCommandQueue();

    // Set graphics memory
    {
        m_graphicsMemory = std::make_unique<GraphicsMemory>(device);
    }

    // Get mesh.
    {
        std::vector<wchar_t> buff(MAX_PATH);

        for (auto& el : s_assetList)
        {
            DX::FindMediaFile(buff.data(), MAX_PATH, el);
            m_meshFiles.emplace_back(std::string(buff.begin(), buff.end()));
        }

        assert(0 < m_meshFiles.size());

        m_meshIndex = 0;
        m_mesh = std::make_shared<Mesh>(
            Mesh(
                device,
                commandQueue,
                std::wstring(m_meshFiles[m_meshIndex].begin(), m_meshFiles[m_meshIndex].end()).c_str()
            )
            );
    }

    // Create constant buffers for the geometry and the scene.
    CreateConstantBuffers();

    // Create scene variables.
    InitializeScene();

    // Setup initial lighting model.
    {
        // Init lighting var.
        m_ao = std::make_unique<AO>();
        m_ssao = std::make_unique<SSAO>();

        // Send over information to the lighting model.
        SetupLighting();
    }

    // Setup menus.
    {
        m_menus->Setup(m_deviceResources);
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    // Inform lighting.
    m_ao->OnSizeChanged();
    m_ssao->OnSizeChanged();

    // Force update.
    m_ao->OnOptionUpdate(m_menus);
    m_ssao->OnOptionUpdate(m_menus);

    // Inform menus.
    m_menus->OnSizeChanged();

    // Broadcast camera information.
    UpdateCameraMatrices();
}

void Sample::OnDeviceLost()
{
    // Direct3D resource cleanup.
    m_graphicsMemory.reset();
    m_mappedSceneConstantData.clear();
    m_mappedSceneConstantResource.clear();
    m_sceneCB.clear();
    m_ssao.reset();
    m_ao.reset();
    m_meshFiles.clear();
    m_mesh.reset();
    m_menus.reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion