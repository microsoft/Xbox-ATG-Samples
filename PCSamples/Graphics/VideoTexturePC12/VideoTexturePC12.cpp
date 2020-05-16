//--------------------------------------------------------------------------------------
// VideoTexturePC12.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "VideoTexturePC12.h"

#include "ATGColors.h"
#include "ControllerFont.h"
#include "FindMedia.h"

extern void ExitSample() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

Sample::Sample() noexcept(false) :
    m_show3D(true),
    m_sharedVideoTexture(nullptr),
    m_videoWidth(0),
    m_videoHeight(0)
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
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float time = float(timer.GetTotalSeconds());

    m_world = Matrix::CreateRotationY(cosf(time) * 2.f);

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }

        if (m_gamePadButtons.a == GamePad::ButtonStateTracker::ButtonState::PRESSED)
        {
            m_show3D = !m_show3D;
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

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Space))
    {
        m_show3D = !m_show3D;
    }

    if (m_player && m_player->IsFinished())
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

    RECT r = { 0, 0, long(m_videoWidth), long(m_videoHeight) };
    MFVideoNormalizedRect rect = { 0.0f, 0.0f, 1.0f, 1.0f };
    m_player->TransferFrame(m_sharedVideoTexture, rect, r);

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    RECT output = m_deviceResources->GetOutputSize();

    RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(output.right, output.bottom);

    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    if (m_show3D)
    {
        m_effect->SetMatrices(m_world, m_view, m_proj);

        m_effect->Apply(commandList);

        m_cube->Draw(commandList);
    }
    else
    {
        m_batchOpaque->Begin(commandList);

        m_batchOpaque->Draw(
            m_resourceDescriptors->GetGpuHandle(Descriptors::VideoTexture),
            XMUINT2(m_videoWidth, m_videoHeight),
            XMFLOAT2(float(safeRect.left), float(safeRect.top)), nullptr, Colors::White);

        m_batchOpaque->End();
    }

    m_spriteBatch->Begin(commandList);

    DX::DrawControllerString(m_spriteBatch.get(),
        m_smallFont.get(), m_ctrlFont.get(),
        L"[View] / Esc  Exit   [A] / Space  Toggle texture vs. cutscene",
        XMFLOAT2(float(safeRect.left), float(safeRect.bottom) - m_smallFont->GetLineSpacing()),
        ATG::Colors::LightGrey);

    m_spriteBatch->End();
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
    commandList->ClearRenderTargetView(rtvDescriptor, ATG::Colors::Background, 0, nullptr);
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

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    m_states = std::make_unique<CommonStates>(device);

    m_cube = GeometricPrimitive::CreateCube();

    RenderTargetState rtState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    {
        EffectPipelineStateDescription pd(
            &GeometricPrimitive::VertexType::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullNone,
            rtState);

        m_effect = std::make_unique<BasicEffect>(device, EffectFlags::Lighting | EffectFlags::Texture, pd);
        m_effect->EnableDefaultLighting();
        m_effect->SetTexture(m_resourceDescriptors->GetGpuHandle(Descriptors::VideoTexture), m_states->AnisotropicWrap());
    }

    wchar_t mediabuff[MAX_PATH];
    DX::FindMediaFile(mediabuff, MAX_PATH, L"SampleVideo.mp4");

    m_player = std::make_unique<MediaEnginePlayer>();
    m_player->Initialize(m_deviceResources->GetDXGIFactory(), device, DXGI_FORMAT_B8G8R8A8_UNORM);
    m_player->SetSource(mediabuff);

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    {
        SpriteBatchPipelineStateDescription pd(rtState, &CommonStates::Opaque);
        m_batchOpaque = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

    {
        SpriteBatchPipelineStateDescription pd(rtState);
        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

    DX::FindMediaFile(mediabuff, MAX_PATH, L"SegoeUI_18.spritefont");
    m_smallFont = std::make_unique<SpriteFont>(device, resourceUpload,
        mediabuff,
        m_resourceDescriptors->GetCpuHandle(Descriptors::TextFont),
        m_resourceDescriptors->GetGpuHandle(Descriptors::TextFont));

    DX::FindMediaFile(mediabuff, MAX_PATH, L"XboxOneControllerLegendSmall.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, resourceUpload,
        mediabuff,
        m_resourceDescriptors->GetCpuHandle(Descriptors::ControllerFont),
        m_resourceDescriptors->GetGpuHandle(Descriptors::ControllerFont));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    uploadResourcesFinished.wait();

    while (!m_player->IsInfoReady())
    {
        SwitchToThread();
    }

    m_player->GetNativeVideoSize(m_videoWidth, m_videoHeight);

#ifdef _DEBUG
    char buff[128] = {};
    sprintf_s(buff, "INFO: Video Size %u x %u\n", m_videoWidth, m_videoHeight);
    OutputDebugStringA(buff);
#endif

    CD3DX12_RESOURCE_DESC desc(
        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        0,
        m_videoWidth,
        m_videoHeight,
        1,
        1,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        1,
        0,
        D3D12_TEXTURE_LAYOUT_UNKNOWN,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);

    CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    DX::ThrowIfFailed(
        device->CreateCommittedResource(
            &defaultHeapProperties,
            D3D12_HEAP_FLAG_SHARED,
            &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr,
            IID_PPV_ARGS(m_videoTexture.ReleaseAndGetAddressOf())));

    CreateShaderResourceView(device, m_videoTexture.Get(), m_resourceDescriptors->GetCpuHandle(Descriptors::VideoTexture));

    DX::ThrowIfFailed(
        device->CreateSharedHandle(
            m_videoTexture.Get(),
            nullptr,
            GENERIC_ALL,
            nullptr,
            &m_sharedVideoTexture));

    m_world = Matrix::Identity;
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto viewPort = m_deviceResources->GetScreenViewport();
    m_batchOpaque->SetViewport(viewPort);
    m_spriteBatch->SetViewport(viewPort);

    m_view = Matrix::CreateLookAt(Vector3(2.f, 2.f, 2.f), Vector3::Zero, Vector3::UnitY);

    RECT output = m_deviceResources->GetOutputSize();

    m_proj = Matrix::CreatePerspectiveFieldOfView(XM_PI / 4.f, float(output.right) / float(output.bottom), 0.1f, 10.f);
}

void Sample::OnDeviceLost()
{
    m_player.reset();

    m_videoTexture.Reset();

    m_batchOpaque.reset();
    m_spriteBatch.reset();
    m_resourceDescriptors.reset();
    m_states.reset();
    m_cube.reset();
    m_effect.reset();
    m_smallFont.reset();
    m_ctrlFont.reset();

    m_graphicsMemory.reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
