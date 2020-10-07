//--------------------------------------------------------------------------------------
// AsynchronousIO.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "AsynchronousIO.h"

#include "ATGColors.h"

extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_frame(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
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
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %llu", m_frame);

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

    if (m_frame >= 1)
    {
        m_overlappedSample.Update();
    }

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
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(1920, 1080);
    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

    m_spriteBatch->Begin();
    m_spriteBatch->Draw(m_background.Get(), m_deviceResources->GetOutputSize());

    std::wstring outputString;
    outputString = m_overlappedSample.GetCurrentTypeString();
    uint32_t numDots = (m_timer.GetFrameCount() % 10) + 1;
    for (uint32_t i = 0; i < numDots; i++)
    {
        outputString += L".";
    }

    m_font->DrawString(m_spriteBatch.get(), outputString.c_str(), pos);

    pos.y += m_font->GetLineSpacing() * 1.5f;

    DrawHelpText(pos);

    m_spriteBatch->End();

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(context);
}

void Sample::DrawHelpText(DirectX::XMFLOAT2& pos)
{
    static std::vector<std::wstring> helpText = {
        L"  Asynchronous I/O",
        L"    Asynchronous I/O is the process of issuing I/O or in the case of this sample file read requests and not expecting the results immediately.",
        L"    The OS allows multiple asynchronous requests to be ""in-flight"" at a time, up to available resources",
        L"    The main benefit for using asynchronous I/O is that it allows the OS and the hardware to optimize the order of fulfillment to reduce overall times",
        L"",
        L"  Wait Pattern",
        L"    The wait pattern issues multiple requests and then waits for any of the requests to finish using WaitForSingleObject or WaitForMultipleObjects.",
        L"    This has the effect of suspending the thread until a request has finished. The data is then available for the thread to process.",
        L"",
        L"  Query Pattern",
        L"    The query pattern is similar to the wait pattern, however it uses GetOverlappedResult to query the status of a particular read operation.",
        L"    This allows the thread to continue processing other work if a request has not completed yet.",
        L"    The main difference between wait and query is that wait can work on multiple requests at a time while query can only handle one at a time.",
        L"    They both allow multiple in-flight requests, it's just how the title chooses to continue working while the requests are in-flight",
        L"",
        L"  Alertable Pattern",
        L"    The alertable pattern uses a completion callback system. The title includes along with a specific request a callback function for when the request completes.",
        L"    The OS will queue the function call to happen after the request completes and a thread is in an alertable state.",
        L"    An alertable state is when a thread is suspended but tells the OS that is can be woken to perform other work.",
        L"    The main way for a thread to enter an alertable state is using either SleepEx, WaitForSingleObjectEx, or WaitForMultipleObjectsEx",
        L"    A side effect with using this pattern is that the callback function could be called from any thread at any time a thread is in the alertable state.",
    };

    for (uint32_t i = 0; i < helpText.size(); i++)
    {
        pos.y += m_font->GetLineSpacing() * 1.1f;
        m_font->DrawString(m_spriteBatch.get(), helpText[i].c_str(), pos);
    }
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
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Suspend(0);
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    m_spriteBatch = std::make_unique<SpriteBatch>(context);
    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"ATGSampleBackground.DDS", nullptr, m_background.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion
