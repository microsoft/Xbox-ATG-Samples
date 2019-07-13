//--------------------------------------------------------------------------------------
// MP4Reader.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

//
// Use one of these two definitions to see how the two different technologies perform.
//
//#define USE_XAUDIO2
#define USE_WASAPI

#include "DeviceResources.h"
#include "StepTimer.h"

#ifndef INVALID_SAMPLE_TIME
#define INVALID_SAMPLE_TIME 0x7fffffffffffffffui64
#endif

int64_t GetCurrentTimeInHNS();

#if defined(USE_WASAPI)
#include "Audioclient.h"
#elif defined(USE_XAUDIO2)
#include <xaudio2.h>

// XAudio2-specific helpers required for class definition
static const uint32_t MP4R_XA2_MAX_BUFFER_COUNT = 3;

struct AudioBufferContext
{
    AudioBufferContext(uint8_t * pData, uint32_t dwAudioBytes)
        :m_pData(pData)
        , m_dwAudioBytes(dwAudioBytes)
    {
    }

    uint8_t * m_pData;
    uint32_t m_dwAudioBytes;
};

//--------------------------------------------------------------------------------------
// Name: struct PlaySoundStreamVoiceContext
// Desc: Frees up the audio buffer after processing
//--------------------------------------------------------------------------------------
struct PlaySoundStreamVoiceContext : public IXAudio2VoiceCallback
{
    virtual void OnVoiceProcessingPassStart(uint32_t) override {}
    virtual void OnVoiceProcessingPassEnd() override {}
    virtual void OnStreamEnd() override {}
    virtual void OnBufferStart(void*)
    {
        m_llLastBufferStartTime = GetCurrentTimeInHNS();
    }

    void OnBufferEnd(void* pBufferContext)
    {
        auto pAudioBufferContext = reinterpret_cast<AudioBufferContext*>(pBufferContext);
        SetEvent(m_hBufferEndEvent);
        //
        // Free up the memory chunk holding the PCM data that was read from disk earlier.
        // In a game you would probably return this memory to a pool.
        //
        if (pBufferContext)
        {
            m_qwRenderedBytes += pAudioBufferContext->m_dwAudioBytes;
            delete[](uint8_t *)pAudioBufferContext->m_pData;
            delete pAudioBufferContext;
        }
        m_llLastBufferStartTime = GetCurrentTimeInHNS();

    }

    virtual void OnLoopEnd(void*) override {}
    virtual void OnVoiceError(void*, HRESULT) override {}

    HANDLE m_hBufferEndEvent;

    PlaySoundStreamVoiceContext() noexcept(false)
        : m_llLastBufferStartTime(INVALID_SAMPLE_TIME)
        , m_qwRenderedBytes(0)
    {
        m_hBufferEndEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
        if (!m_hBufferEndEvent)
            throw std::exception("CreateEventEx");
    }

    PlaySoundStreamVoiceContext(const PlaySoundStreamVoiceContext& other)
        : m_llLastBufferStartTime(other.m_llLastBufferStartTime)
        , m_qwRenderedBytes(other.m_qwRenderedBytes)
    {
        auto cur = GetCurrentProcess();
        (void)DuplicateHandle(cur, other.m_hBufferEndEvent, cur, &m_hBufferEndEvent, 0, FALSE, DUPLICATE_SAME_ACCESS);
    }

    PlaySoundStreamVoiceContext& operator= (const PlaySoundStreamVoiceContext& other)
    {
        if (this != &other) // prevent self-assignment
        {
            m_llLastBufferStartTime = other.m_llLastBufferStartTime;
            m_qwRenderedBytes = other.m_qwRenderedBytes;
            auto cur = GetCurrentProcess();
            (void)DuplicateHandle(cur, other.m_hBufferEndEvent, cur, &m_hBufferEndEvent, 0, FALSE, DUPLICATE_SAME_ACCESS);
        }
        return *this;
    }

    PlaySoundStreamVoiceContext(PlaySoundStreamVoiceContext&&) = default;
    PlaySoundStreamVoiceContext& operator= (PlaySoundStreamVoiceContext&&) = default;

    virtual ~PlaySoundStreamVoiceContext()
    {
        if (m_hBufferEndEvent)
        {
            CloseHandle(m_hBufferEndEvent);
            m_hBufferEndEvent = nullptr;
        }
    }

    uint64_t m_qwRenderedBytes;
    int64_t m_llLastBufferStartTime;
};

#endif

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic game loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

    // Properties
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void ConfigureSourceReaderOutput(IMFSourceReader* pReader, uint32_t dwStreamIndex);
    void RenderVideoFrame(IMFSample * pSample);
    bool RenderAudioFrame(IMFSample * pSample);
    void ProcessVideo();
    void ProcessAudio();
    int64_t GetCurrentRenderTime();

    void Screenshot();
    bool										m_bTakeScreenshot;

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::SpriteFont>        m_fontOverlay;
    std::unique_ptr<DirectX::SpriteFont>        m_fontController;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

    // object for MF interaction
    bool										m_videodone;
    bool										m_audiodone;

    Microsoft::WRL::ComPtr<IMFSample>			m_pOutputVideoSample;

    uint32_t									m_numberOfFramesDecoded;

    uint32_t									m_videoWidth;
    uint32_t									m_videoHeight;

    Microsoft::WRL::ComPtr<IMFSample>			m_pOutputAudioSample;

#ifdef USE_WASAPI

    Microsoft::WRL::ComPtr<IAudioClient>        m_pAudioClient;
    Microsoft::WRL::ComPtr<IAudioRenderClient>  m_pAudioRenderClient;
    WAVEFORMATEX*								m_pAudioClientWFX;

#endif  // USE_WASAPI

    uint32_t									m_bufferFrameCount;
    const WAVEFORMATEX*							m_pAudioReaderOutputWFX;
    Microsoft::WRL::ComPtr<IMFAudioMediaType>	m_pAudioMediaType;


    Microsoft::WRL::ComPtr<IMFSourceReader>		m_pReader;
    Microsoft::WRL::ComPtr<IXboxNV12MFSampleRenderer>	m_pVideoRender;

#ifdef USE_XAUDIO2
    
    Microsoft::WRL::ComPtr<IXAudio2>			m_pXAudio2;
    IXAudio2MasteringVoice*						m_pMasteringVoice;
    IXAudio2SourceVoice*						m_pSourceVoice;
    PlaySoundStreamVoiceContext					m_VoiceContext;
    uint32_t									m_dwCurrentPosition;
    uint32_t									m_dwAudioFramesDecoded;
    uint32_t									m_dwAudioFramesRendered;
    XAUDIO2_BUFFER								m_Buffers[MP4R_XA2_MAX_BUFFER_COUNT];

    static uint32_t WINAPI	SubmitAudioBufferThread(LPVOID lpParam);

#endif  // USE_XAUDIO2

    bool										m_fAudioStarted;
    int64_t										m_llStartTimeStamp;

};
