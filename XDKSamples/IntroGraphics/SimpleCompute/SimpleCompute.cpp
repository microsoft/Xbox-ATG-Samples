//--------------------------------------------------------------------------------------
// SimpleCompute.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleCompute.h"

#include "ATGColors.h"
#include "ControllerFont.h"
#include "ReadData.h"

#define USE_FAST_SEMANTICS

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
    const uint32_t s_numShaderThreads = 8;  // make sure to update value in shader if this changes

    const wchar_t* g_SampleTitle = L"SimpleCompute";
    const wchar_t* g_SampleDescription = L"Demonstrates how to use the ID3D11ComputeContextX interface to submit asynchronous compute shader workloads";
    const ATG::HelpButtonAssignment g_HelpButtons[] = {
        { ATG::HelpID::MENU_BUTTON,         L"Show/Hide Help" },
        { ATG::HelpID::VIEW_BUTTON,         L"Exit" },
        { ATG::HelpID::LEFT_STICK,          L"Pan Viewport" },
        { ATG::HelpID::RIGHT_STICK,         L"Zoom Viewport" },
        { ATG::HelpID::RIGHT_TRIGGER,       L"Increase Zoom Speed" },
        { ATG::HelpID::A_BUTTON,            L"Toggle Async Compute" },
        { ATG::HelpID::Y_BUTTON,            L"Reset Viewport to Default" },
    };
}

Sample::Sample() :
    m_frame(0)
    , m_showHelp(false)
    , m_usingAsyncCompute(false)
    , m_requestUsingAsyncCompute(false)
    , m_asyncComputeActive(false)
    , m_renderIndex(0)
    , m_terminateThread(false)
    , m_suspendThread(false)
    , m_computeThread(nullptr)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN, 2 
#ifdef USE_FAST_SEMANTICS
        , DX::DeviceResources::c_FastSemantics
#endif
        );

    m_help = std::make_unique<ATG::Help>(g_SampleTitle, g_SampleDescription, g_HelpButtons, _countof(g_HelpButtons));
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    ResetWindow();
    m_gamePad = std::make_unique<GamePad>();

    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_computeResumeSignal.Attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
    if (!m_computeResumeSignal.IsValid())
        throw std::exception("CreateEvent");

    m_computeThread = new std::thread(&Sample::AsyncComputeThreadProc, this);
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

    float elapsedTime = float(timer.GetElapsedSeconds());
    m_renderFPS.Tick(elapsedTime);

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (m_gamePadButtons.menu == GamePad::ButtonStateTracker::PRESSED)
        {
            m_showHelp = !m_showHelp;
        }
        else if (m_showHelp && m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED)
        {
            m_showHelp = false;
        }

        if (!m_showHelp)
        {
            if (pad.IsViewPressed())
            {
                ExitSample();
            }

            if (m_gamePadButtons.a == DirectX::GamePad::ButtonStateTracker::PRESSED)
            {
                m_requestUsingAsyncCompute = !m_requestUsingAsyncCompute;

                if (!m_requestUsingAsyncCompute && m_usingAsyncCompute)
                {
                    m_usingAsyncCompute = false;
                    // wait for async compute to finish current iteration
                    while (m_asyncComputeActive)
                    {
                        SwitchToThread();
                    }
                }
                else if (m_requestUsingAsyncCompute && !m_usingAsyncCompute)
                {
                    m_usingAsyncCompute = true;
                }
            }
            const float ThumbLeftX = pad.thumbSticks.leftX;
            const float ThumbLeftY = pad.thumbSticks.leftY;
            const float ThumbRightY = pad.thumbSticks.rightY;
            const float RightTrigger = m_gamePadButtons.rightTrigger == DirectX::GamePad::ButtonStateTracker::HELD;

            if (m_gamePadButtons.y == DirectX::GamePad::ButtonStateTracker::PRESSED)
            {
                ResetWindow();
            }

            if (ThumbLeftX != 0.0f || ThumbLeftY != 0.0f || ThumbRightY != 0.0f)
            {
                const float ScaleSpeed = 1.0f + RightTrigger * 4.0f;
                const float WindowScale = 1.0f + ThumbRightY * -0.25f * ScaleSpeed * elapsedTime;
                m_window.x *= WindowScale;
                m_window.y *= WindowScale;
                m_window.z += m_window.x * ThumbLeftX * elapsedTime * 0.5f;
                m_window.w += m_window.y * ThumbLeftY * elapsedTime * 0.5f;
                m_windowUpdated = true;
            }
            if (!m_usingAsyncCompute)
            {
                m_windowUpdated = true;
            }
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

    // Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();

    if (m_showHelp)
    {
        m_help->Render();
    }
    else
    {
        // Flip colors for which async compute buffer is being rendered
        PIXBeginEvent(context, m_renderIndex ? PIX_COLOR(0, 0, 255) : PIX_COLOR(0,255,0), L"Render");

        if (!m_usingAsyncCompute)
        {
            D3D11_TEXTURE2D_DESC TexDesc = {};
            m_fractalTexture[0]->GetDesc(&TexDesc);

            const uint32_t frameIndex = m_timer.GetFrameCount();
            const uint32_t bufferIndex = frameIndex % 2;
            CB_FractalCS* cbData = &m_cbFractalData[bufferIndex];
            cbData->MaxThreadIter = XMFLOAT4((float)TexDesc.Width, (float)TexDesc.Height, (float)m_fractalMaxIterations, 0);

            cbData->Window = m_window;
            context->CSSetPlacementConstantBuffer(0, m_cbFractal.Get(), cbData);
            context->CSSetShaderResources(0, 1, m_fractalColorMapSRV[0].GetAddressOf());
            context->CSSetSamplers(0, 1, m_fractalBilinearSampler.GetAddressOf());
            context->CSSetShader(m_csFractal.Get(), nullptr, 0);
            context->CSSetUnorderedAccessViews(0, 1, m_fractalUAV[m_renderIndex].GetAddressOf(), nullptr);

            const uint32_t threadGroupX = TexDesc.Width / s_numShaderThreads;
            const uint32_t threadGroupY = TexDesc.Height / s_numShaderThreads;
            context->Dispatch(threadGroupX, threadGroupY, 1);

#ifdef USE_FAST_SEMANTICS
            context->GpuSendPipelinedEvent(D3D11X_GPU_PIPELINED_EVENT_CS_PARTIAL_FLUSH);
#endif
        }
        else
        {
            while (!m_asyncComputeActive)
            {
                SwitchToThread();
            }
        }
        RECT outputSize = m_deviceResources->GetOutputSize();

        RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(outputSize.right, outputSize.bottom);
        XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

        m_spriteBatch->Begin();
        m_spriteBatch->Draw(m_fractalSRV[m_renderIndex].Get(), outputSize);

        wchar_t outputString[256] = {};
        swprintf_s(outputString, 256, L"Simple Compute Context %0.2f fps", m_renderFPS.GetFPS());

        m_font->DrawString(m_spriteBatch.get(), outputString, pos);
        pos.y += m_font->GetLineSpacing();
        if (m_usingAsyncCompute)
        {
            swprintf_s(outputString, 256, L"Asynchronous compute %0.2f fps GPU time: %0.3f msec", m_computeFPS.GetFPS(), m_lastAsyncExecuteTimeMsec);
            m_font->DrawString(m_spriteBatch.get(), outputString, pos);
        }
        else
        {
            swprintf_s(outputString, 256, L"Synchronous compute %0.2f fps", m_renderFPS.GetFPS());
            m_font->DrawString(m_spriteBatch.get(), outputString, pos);
        }

        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(),
            L"[A] Toggle asynchronous vs. synchronous  [View] Exit   [Menu] Help",
            XMFLOAT2(float(safeRect.left), float(safeRect.bottom) - m_font->GetLineSpacing()));

        m_spriteBatch->End();

        PIXEndEvent(context);
    }

    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(context);
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto renderTarget = m_deviceResources->GetRenderTargetView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);

    context->OMSetRenderTargets(1, &renderTarget, nullptr);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    ResetEvent(m_computeResumeSignal.Get());
    m_suspendThread = true;

    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Suspend(0);
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();

    m_suspendThread = false;
    SetEvent(m_computeResumeSignal.Get());
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    D3D11_COMPUTE_CONTEXT_DESC CCDesc = {};
    DX::ThrowIfFailed(
        device->CreateComputeContextX(&CCDesc, m_computeContext.ReleaseAndGetAddressOf()));

    auto blob = DX::ReadData(L"Fractal.cso");
    DX::ThrowIfFailed(
        device->CreateComputeShader(blob.data(), blob.size(), nullptr, m_csFractal.ReleaseAndGetAddressOf()));

    RECT outputSize = m_deviceResources->GetOutputSize();
    CD3D11_TEXTURE2D_DESC TexDesc(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        outputSize.right,
        outputSize.bottom,
        1,
        1,
        D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE
    );

    DX::ThrowIfFailed(
        device->CreateTexture2D(&TexDesc, nullptr, m_fractalTexture[0].ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(
        device->CreateShaderResourceView(m_fractalTexture[0].Get(), nullptr, m_fractalSRV[0].ReleaseAndGetAddressOf()));

    CD3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc(D3D11_UAV_DIMENSION_TEXTURE2D, TexDesc.Format);
    DX::ThrowIfFailed(
        device->CreateUnorderedAccessView(m_fractalTexture[0].Get(), &UAVDesc, m_fractalUAV[0].ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(
        device->CreateTexture2D(&TexDesc, nullptr, m_fractalTexture[1].ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(
        device->CreateShaderResourceView(m_fractalTexture[1].Get(), nullptr, m_fractalSRV[1].ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(
        device->CreateUnorderedAccessView(m_fractalTexture[1].Get(), &UAVDesc, m_fractalUAV[1].ReleaseAndGetAddressOf()));

    m_cbFractalData = (CB_FractalCS*)VirtualAlloc(nullptr, 2 * sizeof(CB_FractalCS), MEM_GRAPHICS | MEM_LARGE_PAGES | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
    CD3D11_BUFFER_DESC BufDesc(sizeof(CB_FractalCS), D3D11_BIND_CONSTANT_BUFFER);
    DX::ThrowIfFailed(
        device->CreatePlacementBuffer(&BufDesc, m_cbFractalData, m_cbFractal.GetAddressOf()));

    m_fractalTimestamps = (uint64_t*)VirtualAlloc(nullptr, 64 * 1024, MEM_GRAPHICS | MEM_LARGE_PAGES | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);

    TexDesc.Width = 8;
    TexDesc.Height = 1;
    TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    const uint32_t RainbowTexels[8] = { 0xFF0000FF, 0xFF0080FF, 0xFF00FFFF, 0xFF00FF00, 0xFFFFFF00, 0xFFFF0000, 0xFF800000, 0xFFFF00FF };
    const uint32_t GradientTexels[8] = { 0xFF000040, 0xFF000080, 0xFF0000C0, 0xFF0000FF, 0xFF0040FF, 0xFF0080FF, 0xFF00C0FF, 0xFF00FFFF };
    static_assert(sizeof(RainbowTexels) == sizeof(GradientTexels), "Mismatched size");

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.SysMemPitch = sizeof(GradientTexels);
    InitData.pSysMem = GradientTexels;
    DX::ThrowIfFailed(
        device->CreateTexture2D(&TexDesc, &InitData, m_fractalColorMap[0].ReleaseAndGetAddressOf()));

    InitData.pSysMem = RainbowTexels;
    DX::ThrowIfFailed(
        device->CreateTexture2D(&TexDesc, &InitData, m_fractalColorMap[1].ReleaseAndGetAddressOf()));

    DX::ThrowIfFailed(
        device->CreateShaderResourceView(m_fractalColorMap[0].Get(), nullptr, m_fractalColorMapSRV[0].ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(
        device->CreateShaderResourceView(m_fractalColorMap[1].Get(), nullptr, m_fractalColorMapSRV[1].ReleaseAndGetAddressOf()));

    CD3D11_SAMPLER_DESC SamplerDesc(D3D11_DEFAULT);
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    DX::ThrowIfFailed(
        device->CreateSamplerState(&SamplerDesc, m_fractalBilinearSampler.ReleaseAndGetAddressOf()));

    m_fractalMaxIterations = 300;

    m_spriteBatch = std::make_unique<SpriteBatch>(context);
    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");
    m_help->RestoreDevice(context);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    m_help->SetWindow(size);

    m_spriteBatch->SetViewport(m_deviceResources->GetScreenViewport());
}
#pragma endregion

void Sample::ResetWindow()
{
    m_window = XMFLOAT4(4.0f, 2.25f, -0.65f, 0.0f);
    m_windowUpdated = true;
}

void Sample::AsyncComputeThreadProc()
{
    LARGE_INTEGER PerfFreq;
    QueryPerformanceFrequency(&PerfFreq);

    LARGE_INTEGER LastFrameTime;
    QueryPerformanceCounter(&LastFrameTime);

    D3D11_TEXTURE2D_DESC TexDesc = {};
    m_fractalTexture[0]->GetDesc(&TexDesc);
    const uint32_t ThreadGroupX = TexDesc.Width / s_numShaderThreads;
    const uint32_t ThreadGroupY = TexDesc.Height / s_numShaderThreads;
    CB_FractalCS* pCBData = &m_cbFractalData[0];
    pCBData->MaxThreadIter = XMFLOAT4((float)TexDesc.Width, (float)TexDesc.Height, (float)m_fractalMaxIterations, 0);

    auto device = m_deviceResources->GetD3DDevice();

    while (!m_terminateThread)
    {
        if (m_suspendThread)
        {
            (void)WaitForSingleObject(m_computeResumeSignal.Get(), INFINITE);
        }

        LARGE_INTEGER CurrentFrameTime;
        QueryPerformanceCounter(&CurrentFrameTime);
        double DeltaTime = (double)(CurrentFrameTime.QuadPart - LastFrameTime.QuadPart) / (double)PerfFreq.QuadPart;
        LastFrameTime = CurrentFrameTime;

        if (m_usingAsyncCompute)
        {
            if (m_windowUpdated)
            {
                if (m_suspendThread)
                {
                    (void)WaitForSingleObject(m_computeResumeSignal.Get(), INFINITE);
                }

                m_computeFPS.Tick((float)DeltaTime);

                pCBData->Window = m_window;
                PIXBeginEvent(m_computeContext.Get(), !m_renderIndex ? PIX_COLOR(0, 0, 255) : PIX_COLOR(0, 255, 0), "Compute");
                m_computeContext->FlushGpuCachesTopOfPipe(0);
                m_computeContext->CSSetPlacementConstantBuffer(0, m_cbFractal.Get(), pCBData);
                m_computeContext->CSSetShaderResources(0, 1, m_fractalColorMapSRV[1].GetAddressOf());
                m_computeContext->CSSetSamplers(0, 1, m_fractalBilinearSampler.GetAddressOf());
                m_computeContext->CSSetShader(m_csFractal.Get(), nullptr, 0);
                m_computeContext->CSSetUnorderedAccessViews(0, 1, m_fractalUAV[1 - m_renderIndex].GetAddressOf(), nullptr);
                m_computeContext->WriteTimestampToMemory(&m_fractalTimestamps[0]);
                m_computeContext->Dispatch(ThreadGroupX, ThreadGroupY, 1);
                m_computeContext->WriteTimestampToMemory(&m_fractalTimestamps[1]);
                PIXEndEvent(m_computeContext.Get());

                uint64_t fence = m_computeContext->InsertFence(0);
                while (device->IsFencePending(fence))
                {
                    SwitchToThread();
                }
                m_renderIndex = 1 - m_renderIndex;

                uint64_t GpuTicks = m_fractalTimestamps[1] - m_fractalTimestamps[0];
                double GpuMsec = (double)GpuTicks * 1e-5;
                m_lastAsyncExecuteTimeMsec = (float)GpuMsec;

                m_asyncComputeActive = true;
            }
            else
            {
                SwitchToThread();
            }
        }
        else
        {
            m_asyncComputeActive = false;
            SwitchToThread();
        }
    }
    m_asyncComputeActive = false;
}
