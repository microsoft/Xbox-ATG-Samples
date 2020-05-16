//--------------------------------------------------------------------------------------
// SymbolProxyClient.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "TextConsole.h"

namespace CallStack
{
	class BackTrace;
}

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample() noexcept(false);
    ~Sample() = default;

    Sample(Sample&&) = default;
    Sample& operator= (Sample&&) = default;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic game loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

	// Capture a callstack on the current thread and display the results to
	// the console. Capturing happens on the current thread but symbol
	// resolution happens asynchronously on a new thread.
	// Note, as the name suggests, this is called from the render thread
	void CaptureRenderThreadCallstack();

	// Capture the callstack for a thread that is not the current thread and 
	// display the results to the console. After capturing the callstack,
	// symbol resolution happens asynchronoulsy on a new thread.
	void CaptureBackgroundThreadCallstackFromRenderThread();
	
	// Helper function to display a callstack to the console.
	void DisplayBacktrace(size_t numCapturedFrames, const CallStack::BackTrace &backtrace);
	
	// Send some symbols to the symbol proxy and then display the results to the
	// console.
	void LookUpSymbol();
	
	// Helper to display diagnostics to the console in case there is a failure.
	void DisplayFailureAdvice(HRESULT hr);
	
	// The sample has an option to demonstrate capturing a callstack on a background
	// thread. The background thread runs in a loop and prints messages to the console.
	void BackgroundLoop();

	// Background loop calls this function with a depth value. The function then
	// increments the value and recursively calls itself. This allows the user to
	// capture callstacks of varying depths depending on how deep the recursion is.
	void RecursiveReporter(unsigned depth, unsigned maxDepth);

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

	// Text Conosole
	std::unique_ptr<DX::TextConsoleImage>       m_console;
	
	// Background Thread
	std::thread                                 m_backgroundThread;

	// Help Legend
	std::unique_ptr<DirectX::SpriteBatch>       m_batch;
	std::unique_ptr<DirectX::SpriteFont>        m_smallFont;
	std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;

	static BOOL CALLBACK s_SymCallback(LPVOID context, ULONG_PTR symbolAddress, HRESULT   innerHRESULT, PCWSTR symName, ULONG offset);
	BOOL SymCallback(ULONG_PTR symbolAddress, HRESULT   innerHRESULT, PCWSTR symName, ULONG offset);

	static BOOL CALLBACK s_SrcCallback( LPVOID context, ULONG_PTR symbolAddress, HRESULT innerHRESULT, PCWSTR filepath, ULONG lineNumber);
	BOOL SrcCallback(ULONG_PTR symbolAddress, HRESULT innerHRESULT, PCWSTR filepath, ULONG lineNumber);
};
