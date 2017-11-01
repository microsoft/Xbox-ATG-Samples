//--------------------------------------------------------------------------------------
// SymbolProxyClient.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SymbolProxyClient.h"
#include <SymbolResolve.h>

#include "ATGColors.h"
#include "ControllerFont.h"
#include "CallStack.h"

extern void ExitSample();

using namespace DirectX;
using Microsoft::WRL::ComPtr;

using namespace CallStack;

Sample::Sample() :
    m_frame(0)
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

    m_backgroundThread = std::thread(&Sample::BackgroundLoop, this);
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
void Sample::Update(DX::StepTimer const&)
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
            CaptureRenderThreadCallstack();
        }
        if (m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED)
        {
            CaptureBackgroundThreadCallstackFromRenderThread();
        }
        if (m_gamePadButtons.y == GamePad::ButtonStateTracker::PRESSED)
        {
            LookUpSymbol();
        }
        if (m_gamePadButtons.x == GamePad::ButtonStateTracker::PRESSED)
        {
            m_console->Clear();
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

    // Render the text console
    {
        m_console->Render();
    }

    // Render the help legend
    auto size = m_deviceResources->GetOutputSize();
    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

    m_batch->Begin();
    m_smallFont->DrawString(m_batch.get(), L"Symbol Proxy Client",
        XMFLOAT2(float(safe.left), float(safe.top)), ATG::Colors::LightGrey);

    DX::DrawControllerString(m_batch.get(),
        m_smallFont.get(), m_ctrlFont.get(),
        L"[View] Exit"
        L"   [A] Capture Callstack on the Render thread"
        L"   [B] Capture Callstack on the Background thread"
        L"   [Y] Look up a symbol"
        L"   [X] Clear the console",
        XMFLOAT2(float(safe.left), float(safe.bottom) - m_smallFont->GetLineSpacing()),
        ATG::Colors::LightGrey);

    m_batch->End();

    PIXEndEvent(context);

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
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

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

    m_console = std::make_unique<DX::TextConsoleImage>(context, L"courier_16.spritefont", L"ATGSampleBackground.DDS");

    m_batch = std::make_unique<SpriteBatch>(context);
    m_smallFont = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");
}
#pragma endregion

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    RECT fullscreen = m_deviceResources->GetOutputSize();
    m_console->SetWindow(fullscreen, true);
}

void Sample::CaptureRenderThreadCallstack()
{
    BackTrace *backtrace = new BackTrace();
    size_t numFrames = backtrace->CaptureCurrentThread(); // As advertised, we are capturing this on the render thread

    // Resolving symbols must make round-trip communications with xbSymbolProxy.exe which isn't particulary fast.
    // You wouldn't really want to do this on the render thread and so to demonstrate a best practice we do
    // the BackTrace::Resolve call (and subsequent processing) on a new thread.
    std::thread([this, backtrace, numFrames]()
    {
        // backtrace gets cleaned up when uniqueBacktrace goes out of scope
        std::unique_ptr<BackTrace> uniqueBacktrace(backtrace);

        HRESULT hr = backtrace->Resolve();
        if (FAILED(hr))
        {
            m_console->Format(L"[" __FUNCTIONW__ L"] BackTrace::Resolve failed (%0#8x)\n", hr);
            DisplayFailureAdvice(hr);
        }

        DisplayBacktrace(numFrames, *backtrace);
    }).detach();
}

void Sample::CaptureBackgroundThreadCallstackFromRenderThread()
{
    HANDLE hBackgroundThread = m_backgroundThread.native_handle();
    BackTrace *backtrace = new BackTrace();

    // As advertised, we're capturing a callstack of another thread from this thread
    size_t numFrames = backtrace->CaptureCrossThread(hBackgroundThread);

    // Since Resolving symbols isn't particularly performant, we do that part (and subsequent processing)
    // on a new thread
    std::thread([this, backtrace, numFrames]()
    {
        // backtrace gets cleaned up when uniqueBacktrace goes out of scope
        std::unique_ptr<BackTrace> uniqueBacktrace(backtrace);

        HRESULT hr = backtrace->Resolve();
        if (FAILED(hr))
        {
            m_console->Format(L"[" __FUNCTIONW__ L"] BackTrace::Resolve failed (%0#8x)\n", hr);
            DisplayFailureAdvice(hr);
        }

        DisplayBacktrace(numFrames, *backtrace);
    }).detach();
}

void Sample::DisplayBacktrace(size_t numCapturedFrames, const CallStack::BackTrace &backtrace)
{
    if (numCapturedFrames > 0)
    {
        for (int i = 0; i < backtrace.size(); ++i)
        {
            const BackTrace::frame_data &fd = backtrace[i];
            if (SUCCEEDED(fd.symbolResult) && SUCCEEDED(fd.sourceResult))
            {
                m_console->Format(L"%s + %#x\n", fd.symbolName.c_str(), fd.symbolOffset);
                m_console->Format(L"(%s, %i)\n", fd.sourceFileName.c_str(), fd.sourceLineNumber);
            }
            else if (SUCCEEDED(fd.symbolResult))
            {
                m_console->Format(L"%s + %#x\n", fd.symbolName.c_str(), fd.symbolOffset);
            }
            else
            {
                m_console->Format(L"%0#16x\n", fd.relativeVirtualAddress + fd.moduleHandle);
            }
        }
    }
}

void Sample::LookUpSymbol()
{
    // The following two statics are to enable getting the real function address of
    // a class member function and then cast it to a void*
    static void(__thiscall Sample::*pMember)() = &Sample::Render;
    static void *evil = (void*&)pMember; // The compiler won't let you do reinterpret_cast,
                                         // nor will it even allow a c-style cast. Hence the name "evil".

    // A variety of symbols to look up:
    static ULONG_PTR symAddresses[] =
    {
        static_cast<ULONG_PTR>(0),                          // Intentional: this will not resolve to anything
        reinterpret_cast<ULONG_PTR>(Sample::s_SymCallback), // A static method
        reinterpret_cast<ULONG_PTR>(evil),                  // A non-static method
        reinterpret_cast<ULONG_PTR>(symAddresses)           // Not a function or method
    };

    // Incremental Linking Thunk (ILT). Sometimes you will see an address resolves to a symbol like
    // the following:
    //   ILT+16355 (?s_SymCallbackSampleCAHPEAX_KJPEB_WKZ)
    // The "ILT" indicates that this ia an Incremental Linking Thunk. It is actually the address of
    // a jmp instruction that targets the real function address. (You would probably never see this
    // in a callstack since a jmp doesn't push a return address.)

    ULONG_PTR *addresses = symAddresses;

    // Symbol resolution is not performant. Creating a new thread here to avoid stalling the render thread.
    std::thread([=]()
    {
        HRESULT hr = ::GetSymbolFromAddress(ResolveDisposition::DefaultPriority, _countof(symAddresses), addresses, s_SymCallback, this);
        if (FAILED(hr))
        {
            m_console->Format(L"]" __FUNCTIONW__ L"] GetSymbolFromAddress failed (%0#8x)\n", hr);
        }

        if (SUCCEEDED(hr))
        {
            hr = ::GetSourceLineFromAddress(ResolveDisposition::DefaultPriority, _countof(symAddresses), addresses, s_SrcCallback, this);
            if (FAILED(hr))
            {
                m_console->Format(L"[" __FUNCTIONW__ L"] GetSourceLineFromAddress failed (%0#8x)\n", hr);
            }
        }

        if (FAILED(hr))
        {
            DisplayFailureAdvice(hr);
        }
    }).detach();
}

void Sample::DisplayFailureAdvice(HRESULT hr)
{
    if (hr == HRESULT_FROM_WIN32(ERROR_NO_DATA)
        || hr == HRESULT_FROM_WIN32(ERROR_INVALID_STATE))
    {
        m_console->WriteLine(L": Please start xbSymbolProxy.exe on the development PC and restart the sample.");
    }
}

BOOL Sample::s_SymCallback(LPVOID context, ULONG_PTR symbolAddress, HRESULT innerHRESULT, PCWSTR symName, ULONG offset)
{
    Sample *thisSample = reinterpret_cast<Sample*>(context);
    return thisSample->SymCallback(symbolAddress, innerHRESULT, symName, offset);
}

BOOL Sample::SymCallback(ULONG_PTR symbolAddress, HRESULT innerHRESULT, PCWSTR symName, ULONG offset)
{
    if (FAILED(innerHRESULT))
    {
        m_console->Format(L"[" __FUNCTIONW__ L"] Not able to resolve symbol for address, %0#16x (HRESULT: %0#8x)\n", symbolAddress, innerHRESULT);
        if (innerHRESULT == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            m_console->WriteLine(L"Hint: specify the path to your .pdb folder with SymbolProxy /p <path>");
        }
    }
    else
    {
        m_console->Format(L"Resolved symbol name for address %0#16x:\n     Name: %s, Offset: %i\n", symbolAddress, symName, offset);
    }
    return TRUE;
}

BOOL Sample::s_SrcCallback(LPVOID context, ULONG_PTR symbolAddress, HRESULT innerHRESULT, PCWSTR filepath, ULONG lineNumber)
{
    Sample *thisSample = reinterpret_cast<Sample*>(context);
    return thisSample->SrcCallback(symbolAddress, innerHRESULT, filepath, lineNumber);
}

BOOL Sample::SrcCallback(ULONG_PTR symbolAddress, HRESULT innerHRESULT, PCWSTR filepath, ULONG lineNumber)
{
    if (FAILED(innerHRESULT))
    {
        m_console->Format(L"[" __FUNCTIONW__ L"] Not able to resolve source information for symbol at address, %0#16x (HRESULT: %0#8x)\n", symbolAddress, innerHRESULT);
    }
    else
    {
        m_console->Format(L"Resolved symbol source information for address %0#16x:\n     Filepath: %s, Line Number: %i\n", symbolAddress, filepath, lineNumber);
    }
    return TRUE;
}

void Sample::BackgroundLoop()
{
    while (true)
    {
        unsigned maxDepth = 1 + std::rand() % 10;
        RecursiveReporter(1, maxDepth);
    }
}

void Sample::RecursiveReporter(unsigned depth, unsigned maxDepth)
{
    if (depth == maxDepth)
    {
        return;
    }
    RecursiveReporter(depth + 1, maxDepth);

    {
        // The output tells you how many frames to expect when you press the button.
        m_console->Format(L"[%s]: depth = %i\n", __FUNCTIONW__, depth);
    }

    // This sleep is here to slow it down for the human pressing the button.
    Sleep(1000);
}
