//--------------------------------------------------------------------------------------
// SimpleSpatialPlaySoundUWP.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleSpatialPlaySoundUWP.h"
#include "WAVFileReader.h"

#include "ATGColors.h"

static const LPCWSTR g_FileList[] = {
	L"Jungle_RainThunder_mix714.wav",
	L"ChannelIDs714.wav",
	nullptr
};

static const int numFiles = 2;

using namespace DirectX;
using namespace Windows::System::Threading;
using namespace Windows::Media::Devices;
using Microsoft::WRL::ComPtr;



VOID CALLBACK SpatialWorkCallback(_Inout_ PTP_CALLBACK_INSTANCE Instance, _Inout_opt_ PVOID Context, _Inout_ PTP_WORK Work)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	Sample * Sink = (Sample *)Context;
	Work;
	Instance;

	while (Sink->m_bThreadActive)
	{

		while (Sink->m_bPlayingSound && Sink->m_Renderer->IsActive())
		{
			// Wait for a signal from the audio-engine to start the next processing pass 
			if (WaitForSingleObject(Sink->m_Renderer->m_bufferCompletionEvent, 100) != WAIT_OBJECT_0)
			{
				//make a call to stream to see why we didn't get a signal after 100ms
				hr = Sink->m_Renderer->m_SpatialAudioStream->Reset();

				//if we have an error, tell the renderer to reset
				if (hr != S_OK)
				{
					Sink->m_Renderer->Reset();
				}
				continue;
			}

			UINT32 frameCount;
			UINT32 availableObjectCount;


			// Begin the process of sending object data and metadata 
			// Get the number of active object that can be used to send object-data 
			// Get the number of frame count that each buffer be filled with  
			hr = Sink->m_Renderer->m_SpatialAudioStream->BeginUpdatingAudioObjects(
				&availableObjectCount,
				&frameCount);
			//if we have an error, tell the renderer to reset
			if (hr != S_OK)
			{
				Sink->m_Renderer->Reset();
			}

			for (int chan = 0; chan < MAX_CHANNELS; chan++)
			{
				//Activate the object if not yet done
				if (Sink->m_WavChannels[chan].object == nullptr)
				{
					// If this method called more than activeObjectCount times 
					// It will fail with this error HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS) 
					hr = Sink->m_Renderer->m_SpatialAudioStream->ActivateSpatialAudioObject(
						Sink->m_WavChannels[chan].objType,
						&Sink->m_WavChannels[chan].object);
					if (FAILED(hr))
					{
						continue;
					}

				}

				//Get the object buffer
				BYTE* buffer = nullptr;
				UINT32 bytecount;
				hr = Sink->m_WavChannels[chan].object->GetBuffer(&buffer, &bytecount);
				if (FAILED(hr))
				{
					continue;
				}


				Sink->m_WavChannels[chan].object->SetVolume(Sink->m_WavChannels[chan].volume);

				UINT32 readsize = bytecount;

				for (UINT32 i = 0; i < readsize; i++)
				{
					UINT32 fileLoc = Sink->m_WavChannels[chan].curBufferLoc;
					if (chan < Sink->m_numChannels)
					{
						buffer[i] = Sink->m_WavChannels[chan].wavBuffer[fileLoc];
					}
					else
					{
						buffer[i] = 0;
					}

					Sink->m_WavChannels[chan].curBufferLoc++;
					if (Sink->m_WavChannels[chan].curBufferLoc == Sink->m_WavChannels[chan].buffersize)
					{
						Sink->m_WavChannels[chan].curBufferLoc = 0;
					}


				}
			}

			// Let the audio-engine know that the object data are available for processing now 
			hr = Sink->m_Renderer->m_SpatialAudioStream->EndUpdatingAudioObjects();
			if (FAILED(hr))
			{
				//if we have an error, tell the renderer to reset
				Sink->m_Renderer->Reset();
				continue;
			}
		}
	}

}



Sample::Sample()
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
	m_fileLoaded = false;
	m_Renderer = nullptr;
	m_bThreadActive = false;
	m_bPlayingSound = false;
	m_curFile = 0;
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_keyboard = std::make_unique<Keyboard>();
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();  	
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

	InitializeSpatialStream();

	SetChannelTypesVolumes();

	m_fileLoaded = LoadFile(g_FileList[m_curFile]);
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

	float elapsedTime = float(timer.GetElapsedSeconds());
	elapsedTime;

	//Are we resetting the renderer?  This will happen if we get an invalid stream 
	//  which can happen when render mode changes or device changes
	if (m_Renderer->IsResetting())
	{
		//clear out renderer
		m_Renderer == NULL;

		// Create a new ISAC instance
		m_Renderer = Microsoft::WRL::Make<ISACRenderer>();

		// Initialize Default Audio Device
		m_Renderer->InitializeAudioDeviceAsync();

		//Reset all the Objects that were being used
		for (int chan = 0; chan < MAX_CHANNELS; chan++)
		{
			m_WavChannels[chan].object = nullptr;
		}
	}

	auto kb = m_keyboard->GetState();
	m_keyboardButtons.Update(kb);

	if (kb.Escape)
	{
		if (m_bThreadActive)
		{
			m_bThreadActive = false;
			m_bPlayingSound = false;
			WaitForThreadpoolWorkCallbacks(m_workThread, FALSE);
			CloseThreadpoolWork(m_workThread);
		}
		m_Renderer->m_SpatialAudioStream->Stop();

		Windows::ApplicationModel::Core::CoreApplication::Exit();
	}
	if (m_keyboardButtons.IsKeyReleased(DirectX::Keyboard::Keys::Space))
	{
		if (m_fileLoaded && m_Renderer && m_Renderer->IsActive())
		{
			//Start spatial worker thread
			if (!m_bThreadActive)
			{
				//reload file to start again
				m_fileLoaded = LoadFile(g_FileList[m_curFile]);
				//startup spatial thread
				m_bThreadActive = true;
				m_bPlayingSound = true;
				m_workThread = CreateThreadpoolWork(SpatialWorkCallback, this, nullptr);
				SubmitThreadpoolWork(m_workThread);
			}
			else
			{
				//stop and shutdown spatial worker thread
				m_bThreadActive = false;
				m_bPlayingSound = false;
				WaitForThreadpoolWorkCallbacks(m_workThread, FALSE);
				CloseThreadpoolWork(m_workThread);
			}
		}
	}
	if (m_keyboardButtons.IsKeyReleased(DirectX::Keyboard::Keys::P))
	{
		//change playingsound state (pause/unpause)
		m_bPlayingSound = !m_bPlayingSound;
	}
	if (m_keyboardButtons.IsKeyReleased(DirectX::Keyboard::Keys::Up))
	{
		//if spatial thread active and playing, shutdown and start new file
		if (m_bThreadActive && m_bPlayingSound)
		{
			m_bThreadActive = false;
			m_bPlayingSound = false;
			WaitForThreadpoolWorkCallbacks(m_workThread, FALSE);
			CloseThreadpoolWork(m_workThread);

			//load next file
			m_curFile++;
			if (m_curFile == numFiles) m_curFile = 0;
			m_fileLoaded = LoadFile(g_FileList[m_curFile]);

			//if successfully loaded, start up thread and playing new file
			if (m_fileLoaded)
			{
				m_bThreadActive = true;
				m_bPlayingSound = true;
				m_workThread = CreateThreadpoolWork(SpatialWorkCallback, this, nullptr);
				SubmitThreadpoolWork(m_workThread);
			}
		}
		else
		{
			//if thread active but paused, shutdown thread to load new file
			if (m_bThreadActive)
			{
				m_bThreadActive = false;
				m_bPlayingSound = false;
				WaitForThreadpoolWorkCallbacks(m_workThread, FALSE);
				CloseThreadpoolWork(m_workThread);
			}

			//load next file
			m_curFile++;
			if (m_curFile == numFiles) m_curFile = 0;
			m_fileLoaded = LoadFile(g_FileList[m_curFile]);
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

    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");
	auto rect = m_deviceResources->GetOutputSize();
	auto safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(rect.right, rect.bottom);

	XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

	m_spriteBatch->Begin();

	wchar_t str[128] = { 0 };
	swprintf_s(str, L"Simple Spatial Playback:");
	m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
	pos.y += 30;
	swprintf_s(str, L"   file: %s", g_FileList[m_curFile]);
	m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
	pos.y += 30;
	swprintf_s(str, L"   state: %s", (m_bThreadActive) ? ((m_bPlayingSound) ? L"Playing" : L"Paused") : L"Stopped");
	m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
	pos.y += 30;
	swprintf_s(str, L"Use Spacebar to start/stop playback");
	m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
	pos.y += 30;
	swprintf_s(str, L"Use 'p' to pause playback");
	m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
	pos.y += 30;
	swprintf_s(str, L"Use UP key to change to next file");
	m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
	pos.y += 30;

	m_spriteBatch->End();


    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent();
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
void Sample::OnActivated()
{
}

void Sample::OnDeactivated()
{
}

void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->ClearState();

    m_deviceResources->Trim();
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
    m_keyboardButtons.Reset();
}

void Sample::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, rotation))
        return;

    CreateWindowSizeDependentResources();
}

void Sample::ValidateDevice()
{
    m_deviceResources->ValidateDevice();
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

	auto context = m_deviceResources->GetD3DDeviceContext();

	m_spriteBatch = std::make_unique<SpriteBatch>(context);

	m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
	m_spriteBatch->SetRotation(m_deviceResources->GetRotation());
}

void Sample::OnDeviceLost()
{
	m_spriteBatch.reset();
	m_font.reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

HRESULT Sample::InitializeSpatialStream(void)
{
	HRESULT hr = S_OK;

	if (!m_Renderer)
	{
		// Create a new ISAC instance
		m_Renderer = Microsoft::WRL::Make<ISACRenderer>();
		// Selects the Default Audio Device
		hr = m_Renderer->InitializeAudioDeviceAsync();
	}

	return hr;
}

bool Sample::LoadFile(LPCWSTR inFile)
{
	//clear and reset m_WavChannels
	for (UINT32 i = 0; i < MAX_CHANNELS; i++)
	{
		if (m_WavChannels[i].buffersize)
		{
			delete[] m_WavChannels[i].wavBuffer;
		}
		m_WavChannels[i].buffersize = 0;
		m_WavChannels[i].curBufferLoc = 0;
		m_WavChannels[i].object = nullptr;
	}

	HRESULT hr = S_OK;
	std::unique_ptr<uint8_t[]>              m_waveFile;
	DirectX::WAVData  WavData;
	hr = DirectX::LoadWAVAudioFromFileEx(inFile, m_waveFile, WavData);
	if (FAILED(hr))
	{
		return false;
	}

	if ((WavData.wfx->wFormatTag == 1 || WavData.wfx->wFormatTag == 65534) && WavData.wfx->nSamplesPerSec == 48000)
	{
		m_numChannels = WavData.wfx->nChannels;

		int numSamples = WavData.audioBytes / (2 * m_numChannels);
		for (int i = 0; i < m_numChannels; i++)
		{
			m_WavChannels[i].wavBuffer = new char[numSamples * 4];
			m_WavChannels[i].buffersize = numSamples * 4;
		}

		float * tempnew;
		short * tempdata = (short *)WavData.startAudio;

		for (int i = 0; i < numSamples; i++)
		{
			for (int j = 0; j < m_numChannels; j++)
			{
				int chan = j;
				tempnew = (float *)m_WavChannels[chan].wavBuffer;
				int sample = (i * m_numChannels) + chan;
				tempnew[i] = (float)tempdata[sample] / 32768;
			}
		}
	}
	else if ((WavData.wfx->wFormatTag == 3) && WavData.wfx->nSamplesPerSec == 48000)
	{
		m_numChannels = WavData.wfx->nChannels;
		int numSamples = WavData.audioBytes / (4 * m_numChannels);
		for (int i = 0; i < m_numChannels; i++)
		{
			m_WavChannels[i].wavBuffer = new char[numSamples * 4];
			m_WavChannels[i].buffersize = numSamples * 4;
		}

		float * tempnew;
		float * tempdata = (float *)WavData.startAudio;

		for (int i = 0; i < numSamples; i++)
		{
			for (int j = 0; j < m_numChannels; j++)
			{
				int chan = j;
				tempnew = (float *)m_WavChannels[chan].wavBuffer;
				int sample = (i * m_numChannels) + chan;
				tempnew[i] = (float)tempdata[sample];
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}

//These positions are set to Atmo equivalent positions if using dynamic objects
void Sample::SetChannelTypesVolumes(void)
{
	m_WavChannels[0].volume = 1.f;
	m_WavChannels[0].objType = AudioObjectType_FrontLeft;

	m_WavChannels[1].volume = 1.f;
	m_WavChannels[1].objType = AudioObjectType_FrontRight;

	m_WavChannels[2].volume = 1.f;
	m_WavChannels[2].objType = AudioObjectType_FrontCenter;

	m_WavChannels[3].volume = 1.f;
	m_WavChannels[3].objType = AudioObjectType_LowFrequency;

	m_WavChannels[4].volume = 1.f;
	m_WavChannels[4].objType = AudioObjectType_BackLeft;

	m_WavChannels[5].volume = 1.f;
	m_WavChannels[5].objType = AudioObjectType_BackRight;

	m_WavChannels[6].volume = 1.f;
	m_WavChannels[6].objType = AudioObjectType_SideLeft;

	m_WavChannels[7].volume = 1.f;
	m_WavChannels[7].objType = AudioObjectType_SideRight;

	m_WavChannels[8].volume = 1.f;
	m_WavChannels[8].objType = AudioObjectType_TopFrontLeft;

	m_WavChannels[9].volume = 1.f;
	m_WavChannels[9].objType = AudioObjectType_TopFrontRight;

	m_WavChannels[10].volume = 1.f;
	m_WavChannels[10].objType = AudioObjectType_TopBackLeft;

	m_WavChannels[11].volume = 1.f;
	m_WavChannels[11].objType = AudioObjectType_TopBackRight;
}

