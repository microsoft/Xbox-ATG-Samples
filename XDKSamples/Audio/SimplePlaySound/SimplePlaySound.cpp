//--------------------------------------------------------------------------------------
// SimplePlaySound.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimplePlaySound.h"

#include "WAVFileReader.h"

static const wchar_t* g_FileList[] = {
    L"71_setup_sweep_xbox.wav",
    L"musicmono.wav",
    L"musicmono_xma.wav",
    L"musicmono_adpcm.wav",
    L"musicmono_xwma.wav",
    L"sine.wav",
    nullptr
};

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_frame(0),
    m_pMasteringVoice(nullptr),
    m_pSourceVoice(nullptr),
    m_currentFile(0),
    m_xmaMemory(nullptr)
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
    XAUDIO2_DEBUG_CONFIGURATION debug = { 0 };
    debug.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
    debug.BreakMask = XAUDIO2_LOG_ERRORS;
    m_pXAudio2->SetDebugConfiguration(&debug, 0);
#endif

    DX::ThrowIfFailed(m_pXAudio2->CreateMasteringVoice(&m_pMasteringVoice));

    // Start playing the first file
    m_currentFile = 0;
    Play(g_FileList[m_currentFile]);
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

    // Check to see if buffer has finished playing, then move on to next sound
    XAUDIO2_VOICE_STATE state;
    m_pSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    if (state.BuffersQueued == 0)
    {
        m_pSourceVoice->DestroyVoice();

        if (g_FileList[++m_currentFile] == nullptr)
        {
            m_currentFile = 0;
        }

        Play(g_FileList[m_currentFile]);
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

    wchar_t str[128] = { 0 };
    swprintf_s(str, L"Playing: %ls", g_FileList[m_currentFile]);

    m_font->DrawString(m_spriteBatch.get(), str, pos);

    if (!m_waveDesc.empty())
    {
        pos.y += m_font->GetLineSpacing();

        m_font->DrawString(m_spriteBatch.get(), m_waveDesc.c_str(), pos);
    }

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

void Sample::Play(const wchar_t* szFilename)
{
    // Free any audio data from previous play (source voice must not be playing from it!)
    m_waveFile.reset();

    if (m_xmaMemory)
    {
        ApuFree(m_xmaMemory);
        m_xmaMemory = nullptr;
    }

    // Load audio data from disk
    DX::WAVData waveData;
    DX::ThrowIfFailed(DX::LoadWAVAudioFromFileEx(szFilename, m_waveFile, waveData));

    // Set up text description
    m_waveDesc.clear();

    uint32_t tag = DX::GetFormatTag(waveData.wfx);
    switch (tag)
    {
    case WAVE_FORMAT_ADPCM:
        m_waveDesc = L"ADPCM";
        break;

    case WAVE_FORMAT_WMAUDIO2:
    case WAVE_FORMAT_WMAUDIO3:
        m_waveDesc = L"xWMA";
        break;

    case WAVE_FORMAT_PCM:
        m_waveDesc = L"PCM";
        break;

    case WAVE_FORMAT_XMA2:
        m_waveDesc = L"XMA2";
        break;
    }

    {
        wchar_t buff[64];
        swprintf_s(buff, L" (%u channels, %u bits, %u samples, %zu ms duration)",
            waveData.wfx->nChannels,
            waveData.wfx->wBitsPerSample,
            waveData.wfx->nSamplesPerSec,
            waveData.GetSampleDurationMS());

        m_waveDesc += buff;

        if (waveData.loopLength > 0)
        {
            m_waveDesc += L" [loop point]";
        }
    }

    // Create the source voice
    DX::ThrowIfFailed(m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, waveData.wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO));

    // Submit wave data
    XAUDIO2_BUFFER buffer = { 0 };
    buffer.pAudioData = waveData.startAudio;
    buffer.Flags = XAUDIO2_END_OF_STREAM;       // Indicates all the audio data is being submitted at once
    buffer.AudioBytes = waveData.audioBytes;

    if (waveData.loopLength > 0)
    {
        buffer.LoopBegin = waveData.loopStart;
        buffer.LoopLength = waveData.loopLength;
        buffer.LoopCount = 1; // We'll just assume we play the loop twice
    }

    if (tag == WAVE_FORMAT_WMAUDIO2 || tag == WAVE_FORMAT_WMAUDIO3)
    {
        //
        // xWMA includes seek tables which must be provided
        //
        XAUDIO2_BUFFER_WMA xwmaBuffer = { 0 };
        xwmaBuffer.pDecodedPacketCumulativeBytes = waveData.seek;
        xwmaBuffer.PacketCount = waveData.seekCount;

        DX::ThrowIfFailed(m_pSourceVoice->SubmitSourceBuffer(&buffer, &xwmaBuffer));
    }
    else if (tag == WAVE_FORMAT_XMA2)
    {
        //
        // To play XMA2 data, we need the audio data in APU memory rather than system memory
        //
        DX::ThrowIfFailed(ApuAlloc(&m_xmaMemory, nullptr, static_cast<UINT32>(waveData.audioBytes), SHAPE_XMA_INPUT_BUFFER_ALIGNMENT));

        memcpy(m_xmaMemory, waveData.startAudio, waveData.audioBytes);

        buffer.pAudioData = static_cast<BYTE*>( m_xmaMemory );
        DX::ThrowIfFailed(m_pSourceVoice->SubmitSourceBuffer(&buffer));
    }
    else
    {
        DX::ThrowIfFailed(m_pSourceVoice->SubmitSourceBuffer(&buffer));
    }

    // Start playing the voice
    DX::ThrowIfFailed(m_pSourceVoice->Start());
}

