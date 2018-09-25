//--------------------------------------------------------------------------------------
// AsyncPresent.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"

#include "AsyncPresent.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Sample::Sample() noexcept(false) :
    m_frame(0),
    m_asyncPresent(true),
    m_syncInterval(1),
    m_vsyncEvent(nullptr)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
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

    m_renderThreadTimes = std::make_unique<Timeline>(0xff00ff00, Colors::Green, L"Render thread");
    m_yieldTimes = std::make_unique<Timeline>(0xffffff00, Colors::Yellow, L"Yield");
    m_swapThrottleTimes = std::make_unique<Timeline>(0xffff0000, Colors::Red, L"Swap throttle");

    m_renderThreadTimes->Start();
    m_renderThreadTimes->End();
    m_renderThreadTimes->Start();

    // Register vsync callback
    m_vsyncEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_vsyncEvent)
    {
        throw std::exception("CreateEvent");
    }
    DX::ThrowIfFailed(DXGIXSetVLineNotification(VLINECOUNTER0, 0, m_vsyncEvent));

    // create dummy worker thread
    auto ThreadProc = [] (LPVOID)->DWORD
    {
        while (true)
        {
            PIXBeginEvent(3, L"Other thread");    // Make a visible block for PIX Timing captures
            for (volatile UINT i = 0; i < 1000000; ++i) {}
            PIXEndEvent();
        }
    };
    auto hThread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
    if (!hThread)
    {
        throw DX::com_exception(HRESULT_FROM_WIN32(GetLastError()));
    }
    if (0 == SetThreadAffinityMask(hThread, 0x1))
    {
        throw std::exception("Could not set thread affinity");
    }
    if(0 == SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL))  // Make sure this thread only runs when the render thread yields
    {
        throw std::exception("Could not set thread priority");
    }
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
void Sample::Update(DX::StepTimer const& /*timer*/)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

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
            m_asyncPresent = !m_asyncPresent;
        }
        if (m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED)
        {
            m_syncInterval = ++m_syncInterval % 5;
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
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();

    // UI
    {
        PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render UI");

        auto size = m_deviceResources->GetOutputSize();
        RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);
        XMFLOAT2 position(float(safeRect.left), float(safeRect.top));
        auto yInc = m_fontUI->GetLineSpacing();
        auto viewport = m_deviceResources->GetScreenViewport();

        // Controls
        m_spriteBatch->Begin(commandList);

        m_fontUI->DrawString(m_spriteBatch.get(), L"AsyncPresent sample", position);
        position.y += yInc;
        position.y += yInc;
        std::wostringstream stringAsyncPresent; 
        stringAsyncPresent << L"[A] Async present = " << (m_asyncPresent ? L"true" : L"false");
        DX::DrawControllerString(m_spriteBatch.get(), m_fontUI.get(), m_fontController.get(), stringAsyncPresent.str().c_str(), position);
        position.y += yInc;
        std::wostringstream stringSyncInterval;
        stringSyncInterval << L"[B] Sync interval = " << m_syncInterval;
        DX::DrawControllerString(m_spriteBatch.get(), m_fontUI.get(), m_fontController.get(), stringSyncInterval.str().c_str(), position);
        position.y += yInc;

        m_spriteBatch->End();

        // Timeline
        auto latest = m_renderThreadTimes->intervals.front().second;
        auto yIncTimeline = 40.0f;
        position.y += yIncTimeline;
        position.y += yIncTimeline;
        m_renderThreadTimes->Render(commandList, m_timelineEffect.get(), m_primitiveBatch.get(), m_spriteBatch.get(), m_fontTimeline.get(), viewport, position, latest);
        position.y += yIncTimeline;
        m_swapThrottleTimes->Render(commandList, m_timelineEffect.get(), m_primitiveBatch.get(), m_spriteBatch.get(), m_fontTimeline.get(), viewport, position, latest);
        position.y += yIncTimeline;
        m_yieldTimes->Render(commandList, m_timelineEffect.get(), m_primitiveBatch.get(), m_spriteBatch.get(), m_fontTimeline.get(), viewport, position, latest);
        position.y += yIncTimeline;

        PIXEndEvent(commandList);
    }

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->CompleteFrame();
    DX::ThrowIfFailed(Present());
    m_deviceResources->MoveToNextFrame();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
    PIXEndEvent();
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

// The heart of the sample is a different method for Present
HRESULT Sample::Present()
{
    m_renderThreadTimes->End();

    HRESULT hr = S_OK;
    while (true)
    {
        m_swapThrottleTimes->Start();
        hr = m_deviceResources->GetSwapChain()->Present(m_syncInterval, m_asyncPresent ? DXGI_PRESENT_DO_NOT_WAIT : 0);
        m_swapThrottleTimes->End();

        if (DXGI_ERROR_WAS_STILL_DRAWING == hr)    // All swap chain buffers are still in use until the next Flip
        {
            m_yieldTimes->Start();
            WaitForSingleObject(m_vsyncEvent, INFINITE);   // Let the other thread run until vsync
            m_yieldTimes->End();
        }
        else
        {
            break;
        }
    }

    m_renderThreadTimes->Start();

    return hr;
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

    m_resourceDescriptorHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, ResourceDescriptors::Count);

    m_primitiveBatch = std::make_unique<DirectX::PrimitiveBatch<VertexPositionColor>>(device);

    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    RenderTargetState renderTargetState(m_deviceResources->GetBackBufferFormat(), m_deviceResources->GetDepthBufferFormat());

    {
        SpriteBatchPipelineStateDescription pipelineDescription(renderTargetState);
        m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pipelineDescription);
    }

    m_fontController = std::make_unique<SpriteFont>(device, 
        resourceUpload, 
        L"XboxOneControllerSmall.spritefont", 
        m_resourceDescriptorHeap->GetCpuHandle(ResourceDescriptors::FontController), 
        m_resourceDescriptorHeap->GetGpuHandle(ResourceDescriptors::FontController));
    m_fontTimeline = std::make_unique<SpriteFont>(device, 
        resourceUpload, 
        L"Courier_16.spritefont", 
        m_resourceDescriptorHeap->GetCpuHandle(ResourceDescriptors::FontTimeline), 
        m_resourceDescriptorHeap->GetGpuHandle(ResourceDescriptors::FontTimeline));
    m_fontUI = std::make_unique<SpriteFont>(device, 
        resourceUpload, 
        L"SegoeUI_18.spritefont", 
        m_resourceDescriptorHeap->GetCpuHandle(ResourceDescriptors::FontUI), 
        m_resourceDescriptorHeap->GetGpuHandle(ResourceDescriptors::FontUI));

    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());   
    uploadResourcesFinished.wait();     // Wait for resources to upload

    {
        EffectPipelineStateDescription pd(&VertexPositionColor::InputLayout,
            CommonStates::Opaque,
            CommonStates::DepthNone,
            CommonStates::CullNone,
            renderTargetState,
            D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

        m_timelineEffect = std::make_unique<BasicEffect>(device, EffectFlags::VertexColor, pd);
    }
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto viewport = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewport);
}
#pragma endregion