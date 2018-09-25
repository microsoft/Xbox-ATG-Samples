//--------------------------------------------------------------------------------------
// SimplePlaySoundStream.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimplePlaySoundStream.h"

extern void ExitSample();

using namespace DirectX;
using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_frame(0),
    m_pMasteringVoice(nullptr),
    m_pSourceVoice(nullptr)
{
    // Renders only 2D, so no need for a depth buffer.
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

    // Initialize XAudio2 objects
    DX::ThrowIfFailed(XAudio2Create(m_pXAudio2.GetAddressOf(), 0));

#ifdef _DEBUG
    // Enable debugging features
    XAUDIO2_DEBUG_CONFIGURATION debug = {};
    debug.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
    debug.BreakMask = XAUDIO2_LOG_ERRORS;
    m_pXAudio2->SetDebugConfiguration(&debug, 0);
#endif

    DX::ThrowIfFailed(m_pXAudio2->CreateMasteringVoice(&m_pMasteringVoice));

	m_NumberOfBuffersConsumed = 0;
	m_NumberOfBuffersProduced = 0;
	m_currentPosition = 0;
	m_DoneSubmitting = false;

	// Open the file for reading and parse its header
    DX::ThrowIfFailed(
        LoadPCMFile(L"71_setup_sweep_xbox.wav")
    );

	// Start the voice.
	DX::ThrowIfFailed(m_pSourceVoice->Start(0));

	// Create the producer thread (reads PCM chunks from disk)
    if (!CreateThread(nullptr, 0, Sample::ReadFileThread, this, 0, nullptr))
    {
        throw DX::com_exception(HRESULT_FROM_WIN32(GetLastError()));
    }

	// Create the consumer thread (submits PCM chunks to XAudio2)
    if (!CreateThread(nullptr, 0, Sample::SubmitAudioBufferThread, this, 0, nullptr))
    {
        throw DX::com_exception(HRESULT_FROM_WIN32(GetLastError()));
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
void Sample::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        if (pad.IsViewPressed())
        {
            ExitSample();
        }
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

	// Check to see if buffer has finished playing
	if (!m_DoneSubmitting)
	{
		XAUDIO2_VOICE_STATE state;
		m_pSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
		bool isRunning = (state.BuffersQueued > 0);
		if (isRunning == false)
		{
			m_pSourceVoice->DestroyVoice();
			m_DoneSubmitting = false;
		}
	}
	
	// Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    auto size = m_deviceResources->GetOutputSize();

    RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

    m_spriteBatch->Begin();

    m_spriteBatch->Draw(m_background.Get(), m_deviceResources->GetOutputSize());

    m_font->DrawString(m_spriteBatch.get(), m_DoneSubmitting ? L"Stream finished" : L"Playing stream", pos);

    m_spriteBatch->End();

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

    // Clear the views
    auto renderTarget = m_deviceResources->GetRenderTargetView();

    // Don't need to clear color as the sample draws a fullscreen image background
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
    
    // Suspend audio engine
    m_pXAudio2->StopEngine();
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();
    m_timer.ResetElapsedTime();

    // Resume audio engine
    m_pXAudio2->StartEngine();
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

//--------------------------------------------------------------------------------------
// Name: ReadFileThread()
// Desc: Reads PCM chunks from disk. Blocks when the buffer queue is full
//--------------------------------------------------------------------------------------
DWORD WINAPI Sample::ReadFileThread(LPVOID lpParam)
{
    auto sample = static_cast<Sample*>(lpParam);

	while (sample->m_currentPosition < sample->m_waveSize)
	{
		while (sample->m_NumberOfBuffersProduced - sample->m_NumberOfBuffersConsumed >= MAX_BUFFER_COUNT)
		{
			//
			// We reached our capacity to stream in data - we should wait for XAudio2 to finish
			// processing at least one buffer.
			// At this point we could go to sleep, or do something else.
			// For the purposes of this sample, we'll just yield.
			//
			SwitchToThread();
		}

        uint32_t cbValid = std::min(STREAMING_BUFFER_SIZE, sample->m_waveSize - sample->m_currentPosition);

		//
		// Allocate memory to stream in data.
		// In a game you would probably acquire this from a memory pool.
		// For the purposes of this sample, we'll allocate it here and have the XAudio2 callback free it later.
		//
        auto pbBuffer = static_cast<uint8_t*>(malloc(cbValid));
        if (!pbBuffer)
            throw std::bad_alloc();

		//
		// Stream in the PCM data.
		// You could potentially use an async read for this. We are already in another thread so we choose to block.
		//
		DX::ThrowIfFailed(
            sample->m_WaveFile.ReadSample(sample->m_currentPosition, pbBuffer, cbValid, nullptr)
        );

		sample->m_currentPosition += cbValid;

		XAUDIO2_BUFFER buffer = {};
		buffer.AudioBytes = cbValid;
		buffer.pAudioData = pbBuffer;
		if (sample->m_currentPosition >= sample->m_waveSize)
			buffer.Flags = XAUDIO2_END_OF_STREAM;

		//
		// Point pContext at the allocated buffer so that we can free it in the OnBufferEnd() callback
		//
		buffer.pContext = pbBuffer;

		//
		// Make the buffer available for consumption.
		//
		sample->m_Buffers[sample->m_NumberOfBuffersProduced % MAX_BUFFER_COUNT] = buffer;

		//
		// A buffer is ready.
		//
		sample->m_NumberOfBuffersProduced++;
	}

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: SubmitAudioBufferThread()
// Desc: Submits audio buffers to XAudio2. Blocks when XAudio2's queue is full or our buffer queue is empty
//--------------------------------------------------------------------------------------

DWORD WINAPI Sample::SubmitAudioBufferThread(LPVOID lpParam)
{
    auto sample = static_cast<Sample*>(lpParam);

	for (;;)
	{
		while (sample->m_NumberOfBuffersProduced - sample->m_NumberOfBuffersConsumed == 0)
		{
			//
			// There are no buffers ready at this time - we should wait for the ReadFile thread to stream in data.
			// At this point we could go to sleep, or do something else.
			// For the purposes of this sample, we'll just yield.
			//
			SwitchToThread();
		}

		//
		// Wait for XAudio2 to be ready - we need at least one free spot inside XAudio2's queue.
		//
		for (;;)
		{
			XAUDIO2_VOICE_STATE state;

			sample->m_pSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

			if (state.BuffersQueued < MAX_BUFFER_COUNT - 1)
				break;

			WaitForSingleObject(sample->m_VoiceContext.m_hBufferEndEvent, INFINITE);
		}

		//
		// Now we have at least one spot free in our buffer queue, and at least one spot free
		// in XAudio2's queue, so submit the next buffer.
		//
		XAUDIO2_BUFFER buffer = sample->m_Buffers[sample->m_NumberOfBuffersConsumed % MAX_BUFFER_COUNT];
		DX::ThrowIfFailed(sample->m_pSourceVoice->SubmitSourceBuffer(&buffer));

		//
		// A buffer is free.
		//
		sample->m_NumberOfBuffersConsumed++;

		//
		// Check if this is the last buffer.
		//
		if (buffer.Flags == XAUDIO2_END_OF_STREAM)
		{
			//
			// We are done.
			//
			sample->m_DoneSubmitting = true;
			break;
		}
	}

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: LoadPCMFile
// Desc: Opens a PCM file for reading and parses its header
//--------------------------------------------------------------------------------------
HRESULT Sample::LoadPCMFile(const wchar_t* szFilename)
{
	HRESULT hr = S_OK;
	WAVEFORMATEXTENSIBLE wfx = {};

	//
	// Read the wave file
	//
	DX::ThrowIfFailed(m_WaveFile.Open(szFilename));

	// Read the format header
	DX::ThrowIfFailed(m_WaveFile.GetFormat(reinterpret_cast<WAVEFORMATEX*>(&wfx), sizeof(wfx)));

	// Calculate how many bytes and samples are in the wave
	m_waveSize = m_WaveFile.GetDuration();

	//
	// Create the source voice to playback the PCM content
	//
	DX::ThrowIfFailed(m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, &(wfx.Format), 0, XAUDIO2_DEFAULT_FREQ_RATIO, &m_VoiceContext));

	return hr;
}
