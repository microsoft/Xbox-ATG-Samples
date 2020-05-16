//--------------------------------------------------------------------------------------
// SimplePlaySoundStream.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "WAVStreamer.h"

//--------------------------------------------------------------------------------------
// Name: struct PlaySoundStreamVoiceContext
// Desc: Frees up the audio buffer after processing
//--------------------------------------------------------------------------------------
struct PlaySoundStreamVoiceContext : public IXAudio2VoiceCallback
{
    STDMETHOD_(void, OnVoiceProcessingPassStart) (UINT32) override {}
    STDMETHOD_(void, OnVoiceProcessingPassEnd)() override {}
    STDMETHOD_(void, OnStreamEnd)() override {}
    STDMETHOD_(void, OnBufferStart)(void*) override {}

    STDMETHOD_(void, OnBufferEnd)(void* pBufferContext) override
    {
        SetEvent(m_hBufferEndEvent);
        //
        // Free up the memory chunk holding the PCM data that was read from disk earlier.
        // In a game you would probably return this memory to a pool.
        //
        free(pBufferContext);
    }

    STDMETHOD_(void, OnLoopEnd)(void*) override {}
    STDMETHOD_(void, OnVoiceError)(void*, HRESULT) override {}

    HANDLE m_hBufferEndEvent;

    PlaySoundStreamVoiceContext() noexcept(false) : m_hBufferEndEvent(nullptr)
    {
        m_hBufferEndEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        if (!m_hBufferEndEvent)
        {
            throw std::exception("CreateEvent");
        }
    }
    virtual ~PlaySoundStreamVoiceContext()
    {
        if (m_hBufferEndEvent)
        {
            CloseHandle(m_hBufferEndEvent);
            m_hBufferEndEvent = nullptr;
        }
    }
};

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

	static const uint32_t STREAMING_BUFFER_SIZE = 65536;
	static const size_t MAX_BUFFER_COUNT = 3;

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

	static DWORD WINAPI ReadFileThread(LPVOID lpParam);
	static DWORD WINAPI SubmitAudioBufferThread(LPVOID lpParam);
	
	HRESULT LoadPCMFile(const wchar_t* szFilename);

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    // Render objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>        m_font;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_background;

    // Audio objects.
    Microsoft::WRL::ComPtr<IXAudio2>            m_pXAudio2;
    IXAudio2MasteringVoice*                     m_pMasteringVoice;
    IXAudio2SourceVoice*                        m_pSourceVoice;

	bool				                        m_DoneSubmitting;
	PlaySoundStreamVoiceContext					m_VoiceContext;
	WaveFile								    m_WaveFile;
	uint32_t                                    m_waveSize;
	uint32_t                                    m_currentPosition;
	XAUDIO2_BUFFER						        m_Buffers[MAX_BUFFER_COUNT];
	size_t                                      m_NumberOfBuffersProduced;
	size_t                                      m_NumberOfBuffersConsumed;
};
