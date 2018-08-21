//--------------------------------------------------------------------------------------
// SimplePlayTextToSpeechXDK.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimplePlayTextToSpeechXDK.h"

#include "WAVFileReader.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

using namespace Windows::Media::SpeechSynthesis;
using namespace Windows::Storage::Streams;

Sample::Sample() :
    m_frame(0),
    m_pMasteringVoice(nullptr),
    m_pSourceVoice(nullptr),
	m_wavMemory(nullptr)
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

		if (pad.IsAPressed())
		{
			if (m_pSourceVoice != nullptr)
			{
				// Check to see if buffer has finished playing, then move on to next sound
				XAUDIO2_VOICE_STATE state;
				m_pSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
				if (state.BuffersQueued == 0)
				{
					m_pSourceVoice->DestroyVoice();

					Play(L"Hello World");
				}
			}
			else
			{
				Play(L"Hello World");
			}
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

    // Prepare the render target to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    RECT safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(1920, 1080);

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

    m_spriteBatch->Begin();

    m_spriteBatch->Draw(m_background.Get(), m_deviceResources->GetOutputSize());

    wchar_t str[128] = {};
    swprintf_s(str, L"Press A to play text to speech for 'Hello World'");

    m_font->DrawString(m_spriteBatch.get(), str, pos);

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

void Sample::Play(const wchar_t* szSpeechText)
{
	// The object for controlling the speech synthesis engine (voice).
	auto synth = ref new SpeechSynthesizer();

	// The string to speak.
	Platform::String^ text = ref new Platform::String(szSpeechText);
	
	// Generate the audio stream from plain text.
	concurrency::create_task(synth->SynthesizeTextToStreamAsync(text)).then([this, text](SpeechSynthesisStream ^speechStream)
	{
		//Data reader for stream
		DataReader^ reader = ref new DataReader(speechStream);
		reader->UnicodeEncoding = UnicodeEncoding::Utf8;
		reader->ByteOrder = ByteOrder::LittleEndian;

		concurrency::create_task(reader->LoadAsync((unsigned int)speechStream->Size)).then([this, reader, speechStream](concurrency::task<unsigned int> bytesLoaded)
		{
			//Get buffer from stream reader
			IBuffer^ tempBuffer = reader->ReadBuffer((unsigned int)speechStream->Size);

			// Query the IBufferByteAccess interface
			ComPtr<IBufferByteAccess> bufferByteAccess;
			reinterpret_cast<IInspectable*>(tempBuffer)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));

			// Retrieve the raw buffer data
			byte* audioData = nullptr;
			bufferByteAccess->Buffer(&audioData);

			// Data is WAV formatted, so read directly from memory
			// The audio format will always be 32bit 22khz mono ADPCM
			DX::WAVData waveData;
			DX::ThrowIfFailed(DX::LoadWAVAudioInMemoryEx(audioData, speechStream->Size, waveData));

			// Cache wave data
			free(m_wavMemory);
			m_wavMemory = malloc(waveData.audioBytes);
			memcpy(m_wavMemory, waveData.startAudio, waveData.audioBytes);

			// Create the source voice
			DX::ThrowIfFailed(m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, waveData.wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO));

			// Submit wave data
			XAUDIO2_BUFFER buffer = {};
			buffer.pAudioData = static_cast<BYTE*>(m_wavMemory);
			buffer.Flags = XAUDIO2_END_OF_STREAM;       // Indicates all the audio data is being submitted at once
			buffer.AudioBytes = waveData.audioBytes;

			if (waveData.loopLength > 0)
			{
				buffer.LoopBegin = waveData.loopStart;
				buffer.LoopLength = waveData.loopLength;
				buffer.LoopCount = 1; // We'll just assume we play the loop twice
			}

			DX::ThrowIfFailed(m_pSourceVoice->SubmitSourceBuffer(&buffer));

			// Start playing the voice
			DX::ThrowIfFailed(m_pSourceVoice->Start());
		});
	});
}

