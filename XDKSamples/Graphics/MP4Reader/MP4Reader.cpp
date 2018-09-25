//--------------------------------------------------------------------------------------
// MP4Reader.cpp
//
// Demonstrates how to read an MP4 file containing an H264 video stream using hardware acceleration.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MP4Reader.h"

#include "ATGColors.h"
#include "ControllerFont.h"
#include "ScreenGrab.h"
#include "wincodec.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

// Define the input url path here, use local file by default. 
// Can also change the URL to remote MP4 or smoothstreaming or http live streaming URLs as below:
//
// Http live streaming url example:
//#define INPUT_FILE_PATH L"https://devstreaming-cdn.apple.com/videos/streaming/examples/bipbop_4x3/bipbop_4x3_variant.m3u8"
//
// Smooth streaming url example:
// #define INPUT_FILE_PATH L"http://playready.directtaps.net/smoothstreaming/SSWSS720H264/SuperSpeedway_720.ism/Manifest"
//

#define INPUT_FILE_PATH  L"G:\\SampleVideo.mp4"

#define REFTIMES_PER_SEC  10000000

int64_t GetCurrentTimeInHNS()
{
    LARGE_INTEGER   m_liCurrent;
    static LARGE_INTEGER s_liFrequency = {};
    if (s_liFrequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&s_liFrequency);
    }

    QueryPerformanceCounter(&m_liCurrent);
    return MFllMulDiv(m_liCurrent.QuadPart, 10000000ll, s_liFrequency.QuadPart, 0);
}

Sample::Sample() :
	m_frame(0)
	, m_videodone(false)
	, m_audiodone(false)
	, m_pOutputVideoSample(nullptr)
	, m_numberOfFramesDecoded(0)
	, m_videoWidth(0)
	, m_videoHeight(0)
	, m_pVideoRender(nullptr)
	, m_llStartTimeStamp(INVALID_SAMPLE_TIME)
	, m_pOutputAudioSample(nullptr)

#ifdef USE_WASAPI

	, m_pAudioClient(nullptr)
	, m_pAudioRenderClient(nullptr)
	, m_pAudioClientWFX(nullptr)

#elif defined(USE_XAUDIO2)

	, m_pXAudio2(nullptr)
	, m_pSourceVoice(nullptr)
	, m_dwAudioFramesDecoded(0)
	, m_dwAudioFramesRendered(0)

#endif

	, m_fAudioStarted(false)
	, m_pAudioReaderOutputWFX(nullptr)
	, m_pAudioMediaType(nullptr)
	, m_bTakeScreenshot(false)
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

	// Now initialize Audio
	{
		// start audio render using WSAPI. 
		const CLSID clsidDeviceEnumerator = __uuidof(MMDeviceEnumerator);
		const IID iidDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

#ifdef USE_WASAPI

		const IID iidIAudioClient = __uuidof(IAudioClient);
		const IID iidIAudioRenderClient = __uuidof(IAudioRenderClient);

		REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC / 2;

#endif  // USE_WASAPI

		ComPtr<IMMDeviceEnumerator> pAudioEnumerator;
		ComPtr<IMMDevice>           pAudioDevice;
		ComPtr<IMFMediaType>        pMediaType;

		DX::ThrowIfFailed(CoCreateInstance(
			clsidDeviceEnumerator, nullptr,
			CLSCTX_ALL, iidDeviceEnumerator,
			(void **)&pAudioEnumerator));

		DX::ThrowIfFailed(pAudioEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pAudioDevice));

#ifdef USE_WASAPI

		DX::ThrowIfFailed(pAudioDevice->Activate(iidIAudioClient, CLSCTX_ALL, nullptr, (void **)&m_pAudioClient));

		DX::ThrowIfFailed(m_pAudioClient->GetMixFormat(&m_pAudioClientWFX));

		DX::ThrowIfFailed(m_pAudioClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			0,
			hnsRequestedDuration,
			0,
			m_pAudioClientWFX,
			nullptr));

		// Get the actual size of the allocated buffer.
		DX::ThrowIfFailed(m_pAudioClient->GetBufferSize(&m_bufferFrameCount));
		DX::ThrowIfFailed(m_pAudioClient->GetService(iidIAudioRenderClient, (void**)&m_pAudioRenderClient));

#endif  // USE_WASAPI

		ConfigureSourceReaderOutput(m_pReader.Get(), (uint32_t)MF_SOURCE_READER_FIRST_AUDIO_STREAM);

		DX::ThrowIfFailed(m_pReader->GetCurrentMediaType((uint32_t)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pMediaType));
		DX::ThrowIfFailed(pMediaType.As(&m_pAudioMediaType));

		m_pAudioReaderOutputWFX = m_pAudioMediaType->GetAudioFormat();

#ifdef USE_WASAPI

		if (m_pAudioReaderOutputWFX->nSamplesPerSec != m_pAudioClientWFX->nSamplesPerSec && m_pAudioReaderOutputWFX->wBitsPerSample != m_pAudioClientWFX->wBitsPerSample)
		{
			// currently, WSAPI only supposts 48hz float, Title os does not have resampler for now, so we can only render 48khz content. 
			// title can use Xaudio2 for rendering, and xaudio2 has ability to convert different sample rate. 
			DX::ThrowIfFailed(MF_E_UNSUPPORTED_RATE);
		}

#endif  // USE_WASAPI

#ifdef USE_XAUDIO2

		DX::ThrowIfFailed(XAudio2Create(&m_pXAudio2, 0));
		DX::ThrowIfFailed(m_pXAudio2->CreateMasteringVoice(&m_pMasteringVoice));
		DX::ThrowIfFailed(m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, m_pAudioReaderOutputWFX, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &m_VoiceContext));

		//
		// Create the consumer thread (submits PCM chunks to XAudio2)
		//
		CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Sample::SubmitAudioBufferThread, (LPVOID)this, 0, nullptr);

#endif  // USE_XAUDIO2
	}
}

//--------------------------------------------------------------------------------------
// Name: ConfigureSourceReaderOutput()
// Desc: Configure the MFSourceReader output type
//--------------------------------------------------------------------------------------
void Sample::ConfigureSourceReaderOutput(IMFSourceReader* pReader, uint32_t dwStreamIndex)
{
	ComPtr<IMFMediaType> pNativeType;
	ComPtr<IMFMediaType> pType;
	GUID majorType, subtype;

	// Find the native format of the stream.
	DX::ThrowIfFailed(pReader->GetNativeMediaType(dwStreamIndex, 0, &pNativeType));

	// Find the major type.
	DX::ThrowIfFailed(pNativeType->GetGUID(MF_MT_MAJOR_TYPE, &majorType));

	// Define the output type.
	DX::ThrowIfFailed(MFCreateMediaType(&pType));

	DX::ThrowIfFailed(pType->SetGUID(MF_MT_MAJOR_TYPE, majorType));

	// Select a subtype.
	if (majorType == MFMediaType_Video)
	{
		// NV12 is the only supported output type of Xbox One HW decoders
		// Don't set the subtype to RGB32. It is too slow.
		subtype = MFVideoFormat_NV12;
	}
	else if (majorType == MFMediaType_Audio)
	{
		subtype = MFAudioFormat_Float;
	}
	else
	{
		// Unrecognized type. Skip.
		return;
	}

	DX::ThrowIfFailed(pType->SetGUID(MF_MT_SUBTYPE, subtype));

	// Set the uncompressed format.
	DX::ThrowIfFailed(pReader->SetCurrentMediaType(dwStreamIndex, nullptr, pType.Get()));
}

#ifdef USE_XAUDIO2

//--------------------------------------------------------------------------------------
// Name: SubmitAudioBufferThread()
// Desc: Submits audio buffers to XAudio2. Blocks when XAudio2's queue is full or our buffer queue is empty
//--------------------------------------------------------------------------------------
uint32_t WINAPI Sample::SubmitAudioBufferThread(LPVOID lpParam)
{
	Sample* sample = (Sample*)lpParam;

	bool done = false;

	while (!done)
	{
		if (sample->m_dwAudioFramesRendered == sample->m_dwAudioFramesDecoded || !sample->m_fAudioStarted)
		{
			SwitchToThread();
		}
		else
		{
			//
			// Wait for XAudio2 to be ready - we need at least one free spot inside XAudio2's queue.
			//
			for (;;)
			{
				XAUDIO2_VOICE_STATE state;
				sample->m_pSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
				if (state.BuffersQueued < MP4R_XA2_MAX_BUFFER_COUNT - 1)
					break;

				WaitForSingleObject(sample->m_VoiceContext.m_hBufferEndEvent, INFINITE);
			}



			//
			// Now we have at least one spot free in our buffer queue, and at least one spot free
			// in XAudio2's queue, so submit the next buffer.
			//
			XAUDIO2_BUFFER buffer = sample->m_Buffers[sample->m_dwAudioFramesRendered % MP4R_XA2_MAX_BUFFER_COUNT];
			DX::ThrowIfFailed(sample->m_pSourceVoice->SubmitSourceBuffer(&buffer));
#ifdef _DEBUG
			OutputDebugString(L"Buffer submitted\n");
#endif

			++sample->m_dwAudioFramesRendered;

			if (sample->m_audiodone)
			{
				done = true;
			}
		}
	}

	return S_OK;
}

#endif  // USE_XAUDIO2


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
	timer;
	PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

	ProcessAudio();
	ProcessVideo();

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }
		if (pad.IsAPressed())
		{
			m_bTakeScreenshot = true;
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

	if (m_pOutputVideoSample)
	{
		// Must occur first since video processor writes to entire visible screen area
		RenderVideoFrame(m_pOutputVideoSample.Get());
	}

	m_spriteBatch->Begin();

    auto size = m_deviceResources->GetOutputSize();

    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(size.right, size.bottom);

    DX::DrawControllerString(m_spriteBatch.get(), m_fontOverlay.get(), m_fontController.get(), L"Press [A] to take a screenshot, [view] to exit the sample...\n",
        XMFLOAT2(float(safe.left), float(safe.bottom) - m_fontOverlay->GetLineSpacing()), ATG::Colors::LightGrey);

    const float yInc = m_fontOverlay->GetLineSpacing() * 1.5f;
    DirectX::SimpleMath::Vector2 overlayPos(float(safe.left), float(safe.top));

    wchar_t buffer[128] = {};
	if (m_pOutputVideoSample)
	{
		if (m_videodone && m_audiodone)
		{
			m_fontOverlay->DrawString(m_spriteBatch.get(), L"Decoding has finished.", overlayPos);
		}
		else
		{
			m_fontOverlay->DrawString(m_spriteBatch.get(), L"Decoding is in progress.", overlayPos);
		}
		overlayPos.y += yInc;

		swprintf_s(buffer, _countof(buffer), L"Video frame size is %dx%d", m_videoWidth, m_videoHeight);
		m_fontOverlay->DrawString(m_spriteBatch.get(), buffer, overlayPos);

        overlayPos.y += yInc;
	}
	else
	{
		m_fontOverlay->DrawString(m_spriteBatch.get(), L"Decoding has not yet started.", overlayPos);
		overlayPos.y += yInc;
	}


	swprintf_s(buffer, _countof(buffer), L"Number of decoded frames received = %d", m_numberOfFramesDecoded);
	m_fontOverlay->DrawString(m_spriteBatch.get(), buffer, overlayPos);

	m_spriteBatch->End();

    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit();
    PIXEndEvent(context);

	if (m_bTakeScreenshot)
	{
		PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Screenshot");
		Screenshot();
		PIXEndEvent(context);
		m_bTakeScreenshot = false;
	}

}

//--------------------------------------------------------------------------------------
// Name: ~GetCurrentRenderTime()
// Desc: calculate current audio clock 
//--------------------------------------------------------------------------------------
int64_t Sample::GetCurrentRenderTime()
{
	int64_t llMasterClock = 0;
#ifdef USE_WASAPI
	uint64_t llClockTime = 0;
	uint64_t llQPC = 0;
	uint64_t llFrequence = 1;
	IAudioClock * pAudioClock = nullptr;

	DX::ThrowIfFailed(m_pAudioClient->GetService(_uuidof(IAudioClock), (void**)&pAudioClock));
	DX::ThrowIfFailed(pAudioClock->GetPosition(&llClockTime, &llQPC));
	DX::ThrowIfFailed(pAudioClock->GetFrequency(&llFrequence));

	llMasterClock = llClockTime * 10000000ll / llFrequence + GetCurrentTimeInHNS() - llQPC;
	pAudioClock->Release();

#elif defined(USE_XAUDIO2)
	if (m_pAudioReaderOutputWFX)
	{
		//Take a snapshot of the voice context before processing to ensure it does not change between operations
		PlaySoundStreamVoiceContext voiceContextSnap = m_VoiceContext;
		
		if (voiceContextSnap.m_llLastBufferStartTime != 0)
		{
			llMasterClock = (voiceContextSnap.m_qwRenderedBytes * 10000000ll
				/ (m_pAudioReaderOutputWFX->nSamplesPerSec * m_pAudioReaderOutputWFX->wBitsPerSample * m_pAudioReaderOutputWFX->nChannels / 8))
				+ GetCurrentTimeInHNS() - voiceContextSnap.m_llLastBufferStartTime;
		}
	}
#endif
	return m_llStartTimeStamp + llMasterClock;
}

//--------------------------------------------------------------------------------------
// Name: RenderVideoFrame()
// Desc: Helper method used by Render
//--------------------------------------------------------------------------------------
void Sample::RenderVideoFrame(IMFSample* pSample)
{
	auto pDev = m_deviceResources->GetD3DDevice();
	ID3D11DeviceContext* pImmediateContext = m_deviceResources->GetD3DDeviceContext();
	IDXGISwapChain* pSwapChain = m_deviceResources->GetSwapChain();
	ComPtr<ID3D11Texture2D> pBackTexture;
	D3D11_RENDER_TARGET_VIEW_DESC renderViewDesc = {};
	renderViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	renderViewDesc.Texture2D.MipSlice = 0;
	D3D11_TEXTURE2D_DESC descBack;
	D3D11_VIEWPORT viewPort;
	const FLOAT backColor[4] = { 0.0, 0.0, 0.0, 1.0 };

	DX::ThrowIfFailed(pSwapChain->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(pBackTexture.GetAddressOf())));
	pBackTexture->GetDesc(&descBack);
	viewPort.TopLeftX = viewPort.TopLeftY = .0f;
	viewPort.MinDepth = .0f;
	viewPort.MaxDepth = 1.0f;
	viewPort.Width = (float)descBack.Width;
	viewPort.Height = (float)descBack.Height;

	ComPtr<ID3D11RenderTargetView> renderTargetView;
	DX::ThrowIfFailed(pDev->CreateRenderTargetView(pBackTexture.Get(), &renderViewDesc, renderTargetView.GetAddressOf()));
	pImmediateContext->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
	pImmediateContext->RSSetViewports(1, &viewPort);
	pImmediateContext->ClearRenderTargetView(renderTargetView.Get(), backColor);

#if _XDK_VER >= 0x3AD703ED /* XDK Edition 170300 */
	m_pVideoRender->SetViewports(1, &viewPort);
#endif

	DX::ThrowIfFailed(m_pVideoRender->RenderDecodedSample(pImmediateContext, pSample, m_videoWidth, m_videoHeight));
}

//--------------------------------------------------------------------------------------
// Name: ProcessVideo()
// Desc: read from video stream
//--------------------------------------------------------------------------------------
void Sample::ProcessVideo()
{
	DWORD streamIndex = MAXDWORD;
	DWORD dwStreamFlags = 0;
	int64_t llTimestamp = 0;
	bool  readNewSample = true;
	HRESULT hr = S_OK;

	if (m_pOutputVideoSample && SUCCEEDED(m_pOutputVideoSample->GetSampleTime(&llTimestamp)))
	{
		// 
		// audio is not rendered in this sample, just simply sync the video sample time to system clock. 
		// sample time is in hundred nanoseconds and system clock is in millseconds. 
		// 
		if (llTimestamp  > GetCurrentRenderTime())
		{
			// the previous sample has not expired, don't read new sample for now. 
			readNewSample = false;
		}
	}

	if (!m_videodone && readNewSample)
	{
		// Retreive sample from source reader
		Microsoft::WRL::ComPtr<IMFSample> pOutputSample;

		hr = m_pReader->ReadSample(
			(uint32_t)MF_SOURCE_READER_FIRST_VIDEO_STREAM,    // Stream index.
			0,                                             // Flags.
			&streamIndex,                                  // Receives the actual stream index. 
			&dwStreamFlags,                                // Receives status flags.
			&llTimestamp,                                  // Receives the time stamp.
			&pOutputSample                                 // Receives the sample or nullptr.  If this parameter receives a non-NULL pointer, the caller must release the interface.
		);

		if (SUCCEEDED(hr))
		{
			if (dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM)
			{
				m_videodone = true;
			}

			if (dwStreamFlags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
			{
				// The format changed. Reconfigure the decoder.
				ConfigureSourceReaderOutput(m_pReader.Get(), streamIndex);
			}

			if (pOutputSample)
			{
				if (m_videoWidth == 0 || m_videoHeight == 0
					|| (dwStreamFlags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED) || (dwStreamFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED))
				{
					// Update video width and height
					Microsoft::WRL::ComPtr<IMFMediaType> pMediaType;

					if (SUCCEEDED(m_pReader->GetCurrentMediaType((uint32_t)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pMediaType)))
					{
						MFVideoArea videoArea = {};
						DX::ThrowIfFailed(pMediaType->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&videoArea, sizeof(MFVideoArea), nullptr));
						m_videoWidth = videoArea.Area.cx;
						m_videoHeight = videoArea.Area.cy;
					}
				}

				m_pOutputVideoSample = pOutputSample;

				++m_numberOfFramesDecoded;
			}
		}
	}
}

//--------------------------------------------------------------------------------------
// Name: RenderAudioFrame()
// Desc: Helper method used by Render
//--------------------------------------------------------------------------------------
bool Sample::RenderAudioFrame(IMFSample* pSample)
{

#ifdef USE_XAUDIO2

	// don't queue up more samples if there are already enough ready to be played
	if (m_dwAudioFramesDecoded - m_dwAudioFramesRendered >= MP4R_XA2_MAX_BUFFER_COUNT)
	{
		if (!m_fAudioStarted)
		{
			DX::ThrowIfFailed(m_pSourceVoice->Start());
			m_fAudioStarted = true;
		}

		return false;
	}

	uint8_t * pOutputData;
	uint8_t * pDecompressedData;

	ComPtr<IMFMediaBuffer> pUncompressedBuf;
	DWORD dwBufferLen;
	DWORD dwMaxBufferLen;

	if (m_llStartTimeStamp == INVALID_SAMPLE_TIME)
	{
		DX::ThrowIfFailed(pSample->GetSampleTime(&m_llStartTimeStamp));
	}

	int64_t timeStamp;
	DX::ThrowIfFailed(pSample->GetSampleTime(&timeStamp));

	DX::ThrowIfFailed(pSample->ConvertToContiguousBuffer(pUncompressedBuf.GetAddressOf()));
	DX::ThrowIfFailed(pUncompressedBuf->GetCurrentLength(&dwBufferLen));

	pOutputData = new uint8_t[dwBufferLen];

	DX::ThrowIfFailed(pUncompressedBuf->Lock(&pDecompressedData, &dwMaxBufferLen, &dwBufferLen));
	memcpy(pOutputData, pDecompressedData, dwBufferLen);

	memset(&m_Buffers[m_dwAudioFramesDecoded % MP4R_XA2_MAX_BUFFER_COUNT], 0, sizeof(XAUDIO2_BUFFER));
	m_Buffers[m_dwAudioFramesDecoded % MP4R_XA2_MAX_BUFFER_COUNT].pAudioData = pOutputData;
	m_Buffers[m_dwAudioFramesDecoded % MP4R_XA2_MAX_BUFFER_COUNT].AudioBytes = dwBufferLen;
	m_Buffers[m_dwAudioFramesDecoded % MP4R_XA2_MAX_BUFFER_COUNT].pContext = new AudioBufferContext(pOutputData, dwBufferLen);

	// if this is the last audio sample, make sure to enable to END_OF_STREAM flag
	if (m_audiodone)
		m_Buffers[m_dwAudioFramesDecoded % MP4R_XA2_MAX_BUFFER_COUNT].Flags |= XAUDIO2_END_OF_STREAM;

	++m_dwAudioFramesDecoded;

	return true;

#endif  // USE_XAUDIO2

#ifdef USE_WASAPI

	UINT32 numFramesPadding = 0;
	UINT32 numFramesAvailable = 0;
	uint8_t * pOutputData;
	uint8_t * pDecompressedData;

	ComPtr<IMFMediaBuffer> pUncompressedBuf;
	DWORD dwBufferLen;
	DWORD dwMaxBufferLen;
	uint32_t dwUncompressedFrameCount;
	uint32_t frameSizeFromDecoder = m_pAudioReaderOutputWFX->wBitsPerSample * m_pAudioReaderOutputWFX->nChannels / 8;
	uint32_t frameSizeFromOutput = m_pAudioClientWFX->wBitsPerSample * m_pAudioClientWFX->nChannels / 8;

	if (m_llStartTimeStamp == INVALID_SAMPLE_TIME)
	{
		DX::ThrowIfFailed(pSample->GetSampleTime(&m_llStartTimeStamp));
	}

	DX::ThrowIfFailed(pSample->ConvertToContiguousBuffer(pUncompressedBuf.GetAddressOf()));
	DX::ThrowIfFailed(pUncompressedBuf->GetCurrentLength(&dwBufferLen));
	DX::ThrowIfFailed(m_pAudioClient->GetCurrentPadding(&numFramesPadding));

	dwUncompressedFrameCount = dwBufferLen / frameSizeFromDecoder;
	numFramesAvailable = m_bufferFrameCount - numFramesPadding;

	if (numFramesAvailable < dwUncompressedFrameCount)
	{
		if (!m_fAudioStarted)
		{
			m_pAudioClient->Start();
			m_fAudioStarted = true;
		}

		return false;
	}

	// Grab all the available space in the shared buffer.
	DX::ThrowIfFailed(m_pAudioRenderClient->GetBuffer(dwUncompressedFrameCount, &pOutputData));
	// don't have resampler, just render the first few channels from the 7.1 client. 
	DX::ThrowIfFailed(pUncompressedBuf->Lock(&pDecompressedData, &dwMaxBufferLen, &dwBufferLen));
	for (uint32_t f = 0; f < dwUncompressedFrameCount; f++)
	{
		if (frameSizeFromDecoder >= frameSizeFromOutput)
		{
			memcpy(pOutputData, pDecompressedData, frameSizeFromOutput);
		}
		else
		{
			memcpy(pOutputData, pDecompressedData, frameSizeFromDecoder);

			// put other channels to 0
			memset(pOutputData + frameSizeFromDecoder, 0, frameSizeFromOutput - frameSizeFromDecoder);
		}
		pOutputData += frameSizeFromOutput;
		pDecompressedData += frameSizeFromDecoder;
	}
	pUncompressedBuf->Unlock();
	DX::ThrowIfFailed(m_pAudioRenderClient->ReleaseBuffer(dwUncompressedFrameCount, 0));

	return true;

#endif  // USE_WASAPI
}

//--------------------------------------------------------------------------------------
// Name: ProcessAudio()
// Desc: read and render from audio stream
//--------------------------------------------------------------------------------------
void Sample::ProcessAudio()
{
	DWORD streamIndex = MAXDWORD;
	DWORD dwStreamFlags = 0;
	int64_t llTimestamp = 0;
	bool  readNewSample = true;
	HRESULT hr = S_OK;

	if (m_pOutputAudioSample)
	{
		if (RenderAudioFrame(m_pOutputAudioSample.Get()))
		{
			m_pOutputAudioSample.Reset();
		}
		else
		{
			//  audio buffer is full, wait for next update
			return;
		}
	}

	while (!m_audiodone && readNewSample)
	{
		// Retreive sample from source reader
		ComPtr<IMFSample> pOutputSample;

		hr = m_pReader->ReadSample(
			(uint32_t)MF_SOURCE_READER_FIRST_AUDIO_STREAM,    // Stream index.
			0,                                             // Flags.
			&streamIndex,                                  // Receives the actual stream index. 
			&dwStreamFlags,                                // Receives status flags.
			&llTimestamp,                                  // Receives the time stamp.
			&pOutputSample                                 // Receives the sample or nullptr.  If this parameter receives a non-NULL pointer, the caller must release the interface.
		);

		if (SUCCEEDED(hr))
		{
			if (dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM)
			{
				m_audiodone = true;
			}

			if (dwStreamFlags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED)
			{
				// The format changed. Reconfigure the decoder.
				ConfigureSourceReaderOutput(m_pReader.Get(), streamIndex);
			}

			if (pOutputSample)
			{
				readNewSample = RenderAudioFrame(pOutputSample.Get());

				if (!readNewSample)
				{
					m_pOutputAudioSample = pOutputSample;
				}
				++m_numberOfFramesDecoded;
			}
		}
		else
		{
			readNewSample = false;
			if (MF_E_END_OF_STREAM == hr)
			{
				m_audiodone = true;
			}
		}
	}
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

//--------------------------------------------------------------------------------------
// Name: Screenshot
// Desc: Take a screenshot of the backbuffer
//--------------------------------------------------------------------------------------
void Sample::Screenshot()
{
	static UINT iCount = 0;
	wchar_t wstrFilename[1024] = L""; // The first shot will end up as screenshot000.png in the app's startup folder
	_snwprintf_s(wstrFilename, _countof(wstrFilename), _TRUNCATE, L"%s%3.3d%s", L"d:\\screenshot", iCount++, L".png");

	ID3D11Texture2D* pBackBuffer;
	m_deviceResources->GetSwapChain()->GetBuffer(0, IID_GRAPHICS_PPV_ARGS(&pBackBuffer));
	assert(pBackBuffer != nullptr);

	DX::ThrowIfFailed(DirectX::SaveWICTextureToFile(m_deviceResources->GetD3DDeviceContext(), pBackBuffer, GUID_ContainerFormatPng, wstrFilename));
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
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

	m_fontOverlay = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
	m_fontController = std::make_unique<SpriteFont>(device, L"XboxOneControllerSmall.spritefont");

	auto context = m_deviceResources->GetD3DDeviceContext();
	m_spriteBatch = std::make_unique<SpriteBatch>(context);

	ComPtr<IMFDXGIDeviceManager> pDXVAManager;
	ComPtr<IUnknown> punkDeviceMgr;
	ComPtr<IMFAttributes> pMFAttributes;

	UINT32 uResetToken = 0;

	// Initialize the Media Foundation platform.
	DX::ThrowIfFailed(MFStartup(MF_VERSION));

	// Call the MFCreateDXGIDeviceManager function to create the Direct3D device manager
	DX::ThrowIfFailed(MFCreateDXGIDeviceManager(&uResetToken, &pDXVAManager));

	// Call the MFResetDXGIDeviceManagerX function with a pointer to the Direct3D device
	DX::ThrowIfFailed(MFResetDXGIDeviceManagerX(pDXVAManager.Get(), device, uResetToken));

	// Create an attribute store
	DX::ThrowIfFailed(pDXVAManager.AsIID(__uuidof(IUnknown), &punkDeviceMgr));
	DX::ThrowIfFailed(MFCreateAttributes(&pMFAttributes, 3));
	DX::ThrowIfFailed(pMFAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, punkDeviceMgr.Get()));
	DX::ThrowIfFailed(pMFAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE));
	DX::ThrowIfFailed(pMFAttributes->SetUINT32(MF_SOURCE_READER_DISABLE_DXVA, FALSE));
	// Don't set the MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING MFAttribute to TRUE. It is too slow.

	// Create the source reader.
	DX::ThrowIfFailed(MFCreateSourceReaderFromURL(INPUT_FILE_PATH, pMFAttributes.Get(), &m_pReader));

	ConfigureSourceReaderOutput(m_pReader.Get(), (uint32_t)MF_SOURCE_READER_FIRST_VIDEO_STREAM);

	DX::ThrowIfFailed(MFCreateDxvaSampleRendererX(device, nullptr, &m_pVideoRender));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto viewport = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewport);
}
#pragma endregion
