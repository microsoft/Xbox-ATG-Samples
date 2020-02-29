//--------------------------------------------------------------------------------------
// SimpleMSAA.cpp
//
// This sample demonstrates setting up a MSAA render target for DirectX 12
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleMSAA.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    const DXGI_FORMAT c_backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    const DXGI_FORMAT c_depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

#if _XDK_VER < 0x3F6803F3 /* XDK Edition 170600 */
    // This is a workaround for a bug in the validation layer when resolving sRGB formats.
    const DXGI_FORMAT c_msaaFormat = DXGI_FORMAT_B8G8R8A8_TYPELESS;
    const DXGI_FORMAT c_resolveFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#else
    const DXGI_FORMAT c_msaaFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    const DXGI_FORMAT c_resolveFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
#endif

    // Xbox One supports 2x, 4x, or 8x MSAA
    const unsigned int c_sampleCount = 4;
}

Sample::Sample() :
    m_msaa(true),
    m_frame(0)
{
    unsigned int flags = 0;

#ifdef ENABLE_4K
    flags |= DX::DeviceResources::c_Enable4K_UHD;
#endif

    m_deviceResources = std::make_unique<DX::DeviceResources>(
        c_backBufferFormat,
        c_depthBufferFormat, /* If we were only doing MSAA rendering, we could skip the non-MSAA depth/stencil buffer with DXGI_FORMAT_UNKNOWN */
        2,
        flags);

    //
    // In Win32 'classic' DirectX 11, you can create the 'swapchain' backbuffer as a multisample buffer.  Present took care of the
    // resolve as part of the swapchain management.  This is not recommended as doing it explictly gives you more control
    // as well as the fact that this 'old-school' implicit resolve behavior is not supported for UWP.
    //
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_gamePad = std::make_unique<GamePad>();

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %I64u", m_frame);

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
    m_frame++;
}

// Updates the world.
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float time = float(timer.GetTotalSeconds());

    m_world = Matrix::CreateRotationZ(cosf(time / 4.f));

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }

        if (m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED)
        {
            m_msaa = !m_msaa;
        }
    }
    else
    {
        m_gamePadButtons.Reset();
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
    m_deviceResources->Prepare(m_msaa ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_PRESENT);
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Draw the scene.
    ID3D12DescriptorHeap* heaps[] = { m_modelResources->Heap(), m_states->Heap() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Must use PSOs with MSAA sample counts that match our render target.
    if (m_msaa)
    {
        Model::UpdateEffectMatrices(m_modelMSAA, m_world, m_view, m_proj);

        m_model->Draw(commandList, m_modelMSAA.cbegin());
    }
    else
    {
        Model::UpdateEffectMatrices(m_modelStandard, m_world, m_view, m_proj);

        m_model->Draw(commandList, m_modelStandard.cbegin());
    }

    PIXEndEvent(commandList);

    if (m_msaa)
    {
        // Resolve the MSAA render target.
        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Resolve");

        auto backBuffer = m_deviceResources->GetRenderTarget();

        {
            D3D12_RESOURCE_BARRIER barriers[2] =
            {
                CD3DX12_RESOURCE_BARRIER::Transition(
                    m_msaaRenderTarget.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
                CD3DX12_RESOURCE_BARRIER::Transition(
                    backBuffer,
                    D3D12_RESOURCE_STATE_PRESENT,
                    D3D12_RESOURCE_STATE_RESOLVE_DEST)
            };

            commandList->ResourceBarrier(2, barriers);
        }

        commandList->ResolveSubresource(backBuffer, 0, m_msaaRenderTarget.Get(), 0, c_resolveFormat);

        PIXEndEvent(commandList);

        // Set render target for UI which is typically rendered without MSAA.
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                backBuffer,
                D3D12_RESOURCE_STATE_RESOLVE_DEST,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            commandList->ResourceBarrier(1, &barrier);
        }
    }

    // Unbind depth/stencil for UI.
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, nullptr);

    // Draw UI.
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Draw UI");

    auto size = m_deviceResources->GetOutputSize();
    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

    heaps[0] = m_resourceDescriptors->Heap();
    commandList->SetDescriptorHeaps(1, heaps);

    m_batch->Begin(commandList);

    wchar_t str[32] = {};
    swprintf_s(str, L"Sample count: %u", m_msaa ? c_sampleCount : 1);
    m_smallFont->DrawString(m_batch.get(), str, XMFLOAT2(float(safe.left), float(safe.top)), ATG::Colors::White);

    DX::DrawControllerString(m_batch.get(),
        m_smallFont.get(), m_ctrlFont.get(),
        L"[A] Toggle MSAA   [View] Exit",
        XMFLOAT2(float(safe.left),
            float(safe.bottom) - m_smallFont->GetLineSpacing()),
        ATG::Colors::LightGrey);

    m_batch->End();

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
    PIXEndEvent();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.

    if (m_msaa)
    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_msaaRenderTarget.Get(),
            D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ResourceBarrier(1, &barrier);

        //
        // Rather than operate on the swapchain render target, we set up to render the scene to our MSAA resources instead.
        //

        auto rtvDescriptor = m_msaaRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        auto dsvDescriptor = m_msaaDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

        commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);

        commandList->ClearRenderTargetView(rtvDescriptor, ATG::ColorsLinear::Background, 0, nullptr);
        commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }
    else
    {
        auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
        auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

        commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);

        commandList->ClearRenderTargetView(rtvDescriptor, ATG::ColorsLinear::Background, 0, nullptr);
        commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

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
void Sample::OnSuspending()
{
    auto queue = m_deviceResources->GetCommandQueue();
    queue->SuspendX(0);
}

void Sample::OnResuming()
{
    auto queue = m_deviceResources->GetCommandQueue();
    queue->ResumeX();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    // Create descriptor heaps for MSAA render target views and depth stencil views.
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
    rtvDescriptorHeapDesc.NumDescriptors = 1;
    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    DX::ThrowIfFailed(device->CreateDescriptorHeap(&rtvDescriptorHeapDesc,
        IID_GRAPHICS_PPV_ARGS(m_msaaRTVDescriptorHeap.ReleaseAndGetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
    dsvDescriptorHeapDesc.NumDescriptors = 1;
    dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

    DX::ThrowIfFailed(device->CreateDescriptorHeap(&dsvDescriptorHeapDesc,
        IID_GRAPHICS_PPV_ARGS(m_msaaDSVDescriptorHeap.ReleaseAndGetAddressOf())));

    // Setup test scene.
    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);

    m_states = std::make_unique<CommonStates>(device);

    m_model = Model::CreateFromSDKMESH(device, L"CityBlockConcrete.sdkmesh");

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    m_model->LoadStaticBuffers(device, resourceUpload);

    m_modelResources = m_model->LoadTextures(device, resourceUpload);

    m_fxFactory = std::make_unique<EffectFactory>(m_modelResources->Heap(), m_states->Heap());

    {
        RenderTargetState rtStateUI(c_backBufferFormat, DXGI_FORMAT_UNKNOWN);
        SpriteBatchPipelineStateDescription pd(rtStateUI);

        m_batch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);
    }

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    {
        RenderTargetState rtState(c_backBufferFormat, c_depthBufferFormat);
        rtState.sampleDesc.Count = c_sampleCount;

        EffectPipelineStateDescription pd(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullClockwise,
            rtState);

        EffectPipelineStateDescription pdAlpha(
            nullptr,
            CommonStates::AlphaBlend,
            CommonStates::DepthDefault,
            CommonStates::CullClockwise,
            rtState);

        m_modelMSAA = m_model->CreateEffects(*m_fxFactory, pd, pdAlpha);
    }

    {
        RenderTargetState rtState(c_backBufferFormat, c_depthBufferFormat);

        EffectPipelineStateDescription pd(
            nullptr,
            CommonStates::Opaque,
            CommonStates::DepthDefault,
            CommonStates::CullClockwise,
            rtState);

        EffectPipelineStateDescription pdAlpha(
            nullptr,
            CommonStates::AlphaBlend,
            CommonStates::DepthDefault,
            CommonStates::CullClockwise,
            rtState);

        m_modelStandard = m_model->CreateEffects(*m_fxFactory, pd, pdAlpha);
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto output = m_deviceResources->GetOutputSize();

    // Determine the render target size in pixels.
    UINT backBufferWidth = std::max<UINT>(output.right - output.left, 1);
    UINT backBufferHeight = std::max<UINT>(output.bottom - output.top, 1);

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);

    // Create an MSAA render target.
    D3D12_RESOURCE_DESC msaaRTDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        c_msaaFormat,
        backBufferWidth,
        backBufferHeight,
        1, // This render target view has only one texture.
        1, // Use a single mipmap level
        c_sampleCount
    );
    msaaRTDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE msaaOptimizedClearValue = {};
    msaaOptimizedClearValue.Format = c_backBufferFormat;
    memcpy(msaaOptimizedClearValue.Color, ATG::ColorsLinear::Background, sizeof(float) * 4);

    auto device = m_deviceResources->GetD3DDevice();
    DX::ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &msaaRTDesc,
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
        &msaaOptimizedClearValue,
        IID_GRAPHICS_PPV_ARGS(m_msaaRenderTarget.ReleaseAndGetAddressOf())
    ));

    m_msaaRenderTarget->SetName(L"MSAA Render Target");

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = c_backBufferFormat;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

    device->CreateRenderTargetView(
        m_msaaRenderTarget.Get(), &rtvDesc,
        m_msaaRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // Create an MSAA depth stencil view.
    D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        c_depthBufferFormat,
        backBufferWidth,
        backBufferHeight,
        1, // This depth stencil view has only one texture.
        1, // Use a single mipmap level.
        c_sampleCount
    );
    depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = c_depthBufferFormat;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    DX::ThrowIfFailed(device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_GRAPHICS_PPV_ARGS(m_msaaDepthStencil.ReleaseAndGetAddressOf())
    ));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = c_depthBufferFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;

    device->CreateDepthStencilView(
        m_msaaDepthStencil.Get(), &dsvDesc,
        m_msaaDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // Load UI.
    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    m_smallFont = std::make_unique<SpriteFont>(device, resourceUpload,
        (output.bottom > 1080) ? L"SegoeUI_36.spritefont" : L"SegoeUI_18.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::UIFont),
        m_resourceDescriptors->GetGpuHandle(Descriptors::UIFont));

    m_ctrlFont = std::make_unique<SpriteFont>(device, resourceUpload,
        (output.bottom > 1080) ? L"XboxOneControllerLegend.spritefont" : L"XboxOneControllerLegendSmall.spritefont",
        m_resourceDescriptors->GetCpuHandle(Descriptors::CtrlFont),
        m_resourceDescriptors->GetGpuHandle(Descriptors::CtrlFont));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
    uploadResourcesFinished.wait();

    // Setup test scene.
    m_view = Matrix::CreateLookAt(Vector3(0, -211.f, -23.f), Vector3(6.f, 0.f, -37.f), -Vector3::UnitZ);

    m_proj = Matrix::CreatePerspectiveFieldOfView(XM_PI / 4.f,
        float(backBufferWidth) / float(backBufferHeight), 0.1f, 1000.f);

    auto viewport = m_deviceResources->GetScreenViewport();
    m_batch->SetViewport(viewport);
}
#pragma endregion