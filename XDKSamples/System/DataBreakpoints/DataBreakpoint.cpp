//--------------------------------------------------------------------------------------
// DataBreakpoint.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "DataBreakpoint.h"
#include "ControllerFont.h"

#include "ATGColors.h"

extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_frame(0)
    , m_executionTestResult(TestStatus::TEST_NOT_RUN)
    , m_readTestResult(TestStatus::TEST_NOT_RUN)
    , m_readWriteTestResult(TestStatus::TEST_NOT_RUN)
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

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (m_gamePadButtons.a == DirectX::GamePad::ButtonStateTracker::PRESSED)
        {
            m_executionTestResult = m_dataTest.RunTest(DataBreakpointTest::WhichTest::ExecutionTest) ? TestStatus::TEST_SUCCESS : TestStatus::TEST_FAILURE;
        }
        if (m_gamePadButtons.b == DirectX::GamePad::ButtonStateTracker::PRESSED)
        {
            m_readTestResult = m_dataTest.RunTest(DataBreakpointTest::WhichTest::ReadTest) ? TestStatus::TEST_SUCCESS : TestStatus::TEST_FAILURE;
        }
        if (m_gamePadButtons.x == DirectX::GamePad::ButtonStateTracker::PRESSED)
        {
            m_readWriteTestResult = m_dataTest.RunTest(DataBreakpointTest::WhichTest::ReadWriteTest) ? TestStatus::TEST_SUCCESS : TestStatus::TEST_FAILURE;
        }
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
    outputString = L"Data Breakpoint Tests ";
    uint32_t numDots = (m_timer.GetFrameCount() % 10) + 1;
    for (uint32_t i = 0; i < numDots; i++)
    {
        outputString += L".";
    }
    m_font->DrawString(m_spriteBatch.get(), outputString.c_str(), pos);
    pos.y += m_font->GetLineSpacing() * 3;

    DrawStatusString(L"[A]", L"Execution Breakpoint", m_executionTestResult, pos);
    DrawHelpText(pos, DataBreakpointTest::WhichTest::ExecutionTest);
    pos.y += m_font->GetLineSpacing() * 3;

    DrawStatusString(L"[B]", L"Read Breakpoint", m_readTestResult, pos);
    DrawHelpText(pos, DataBreakpointTest::WhichTest::ReadTest);
    pos.y += m_font->GetLineSpacing() * 3;

    DrawStatusString(L"[X]", L"Read/Write Breakpoint", m_readWriteTestResult, pos);
    DrawHelpText(pos, DataBreakpointTest::WhichTest::ReadWriteTest);
    pos.y += m_font->GetLineSpacing() * 3;

    m_spriteBatch->End();

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(context);
}

void Sample::DrawStatusString(const std::wstring& button, const std::wstring& testName, TestStatus status, XMFLOAT2& pos)
{
    RECT stringBounds;
    std::wstring outputString;
    std::wstring resultString;
    XMVECTORF32 resultColor = ATG::Colors::OffWhite;
    outputString = L"Press ";
    outputString += button;
    outputString += L" to run ";
    outputString += testName;
    outputString += L" Test: ";
    switch (status)
    {
    case TestStatus::TEST_NOT_RUN:
        resultString += L"not run yet";
        break;
    case TestStatus::TEST_SUCCESS:
        resultString += L"success";
        resultColor = ATG::Colors::Green;
        break;
    case TestStatus::TEST_FAILURE:
        resultString += L"failure";
        resultColor = ATG::Colors::Orange;
        break;
    }
    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), outputString.c_str(), pos);
    stringBounds = DX::MeasureControllerDrawBounds(m_font.get(), m_ctrlFont.get(), outputString.c_str(), pos);
    pos.x += (stringBounds.right - stringBounds.left);
    m_font->DrawString(m_spriteBatch.get(), resultString.c_str(), pos, resultColor);
    pos.x -= (stringBounds.right - stringBounds.left);
}

void Sample::DrawHelpText(DirectX::XMFLOAT2& pos, DataBreakpointTest::WhichTest whichTest)
{
    static const wchar_t *helpText[] = {
        L"  Execution breakpoints.",
        L"    Sets a breakpoint when a particular instruction is executed, for example the entry point of a function.",
        L"    This is useful in finding specific code paths during automation, for instance calling physics outside the physics phase",
        L"  Memory read breakpoints.",
        L"    Sets a breakpoint when a particular variable is read from.",
        L"    This is useful to track down issues where data is being used after delete if the breakpoint is set during free.",
        L"  Memory read/write breakpoints",
        L"    Sets a breakpoint when a particular memory address is either written to or read from.",
        L"    This is useful to track down various memory issues like access off the end of an array. Set a breakpoint at the first address past the array.",
    };
    uint32_t startIndex(0);
    switch (whichTest)
    {
    case DataBreakpointTest::WhichTest::ExecutionTest:
        startIndex = 0;
        break;
    case DataBreakpointTest::WhichTest::ReadTest:
        startIndex = 3;
        break;
    case DataBreakpointTest::WhichTest::ReadWriteTest:
        startIndex = 6;
        break;
    }
    for (uint32_t i = 0; i < 3; i++)
    {
        pos.y += m_font->GetLineSpacing() * 1.1f;
        m_font->DrawString(m_spriteBatch.get(), helpText[i + startIndex], pos);
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
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneController.spritefont");
    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"ATGSampleBackground.DDS", nullptr, m_background.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion
