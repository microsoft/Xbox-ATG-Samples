//--------------------------------------------------------------------------------------
// SimplePlaySoundStreamUWP.h
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
	virtual void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
	virtual void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
	virtual void STDMETHODCALLTYPE OnStreamEnd() override {}
	virtual void STDMETHODCALLTYPE OnBufferStart(void*) override {}
	void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext)
	{
		SetEvent(m_hBufferEndEvent);
		//
		// Free up the memory chunk holding the PCM data that was read from disk earlier.
		// In a game you would probably return this memory to a pool.
		//
		free(pBufferContext);
	}
	virtual void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
	virtual void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override {}

	HANDLE m_hBufferEndEvent;

	PlaySoundStreamVoiceContext()
	{
		m_hBufferEndEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	}
	virtual ~PlaySoundStreamVoiceContext()
	{
		CloseHandle(m_hBufferEndEvent);
	}
};

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample final : public DX::IDeviceNotify
{
public:

    Sample() noexcept(false);
    ~Sample() = default;

    Sample(Sample&&) = default;
    Sample& operator= (Sample&&) = default;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

    // Initialization and management
    void Initialize(::IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

    // Basic render loop
    void Tick();

    // IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
    void ValidateDevice();

    // Properties
    void GetDefaultSize( int& width, int& height ) const;

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
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // Render objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>        m_font;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_background;
	
	// Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
    std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;

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
