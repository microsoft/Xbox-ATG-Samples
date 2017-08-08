//--------------------------------------------------------------------------------------
// WASAPIRenderer.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include <AudioClient.h>
#include <mmdeviceapi.h>
#include <mutex>

#include "DeviceState.h"
#include "common.h"
#include "ToneSampleGenerator.h"



#pragma once
	// User Configurable Arguments for Scenario
	struct DEVICEPROPS
	{
		bool					IsHWOffload;
		bool					IsBackground;
		bool					IsRawSupported;
		bool					IsRawChosen;
		REFERENCE_TIME          hnsBufferDuration;
		unsigned long           Frequency;
	};

	// Primary WASAPI Renderering Class
	class WASAPIRenderer :
		public Microsoft::WRL::RuntimeClass< Microsoft::WRL::RuntimeClassFlags< Microsoft::WRL::ClassicCom >, Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler >
	{
	public:
		WASAPIRenderer();

		HRESULT SetProperties( DEVICEPROPS props );
		HRESULT InitializeAudioDeviceAsync();
		HRESULT StartPlaybackAsync();
		HRESULT StopPlaybackAsync();
		HRESULT PausePlaybackAsync();

		HRESULT SetVolumeOnSession( unsigned int volume );
		DeviceStateChangedEvent^ GetDeviceStateEvent() { return m_DeviceStateChanged; };

		METHODASYNCCALLBACK( WASAPIRenderer, StartPlayback, OnStartPlayback );
		METHODASYNCCALLBACK( WASAPIRenderer, StopPlayback, OnStopPlayback );
		METHODASYNCCALLBACK( WASAPIRenderer, PausePlayback, OnPausePlayback );
		METHODASYNCCALLBACK( WASAPIRenderer, SampleReady, OnSampleReady );

		// IActivateAudioInterfaceCompletionHandler
		STDMETHOD(ActivateCompleted)( IActivateAudioInterfaceAsyncOperation *operation );

	private:
		~WASAPIRenderer();

		HRESULT OnStartPlayback( IMFAsyncResult* pResult );
		HRESULT OnStopPlayback( IMFAsyncResult* pResult );
		HRESULT OnPausePlayback( IMFAsyncResult* pResult );
		HRESULT OnSampleReady( IMFAsyncResult* pResult );

		HRESULT ConfigureDeviceInternal();
		HRESULT ValidateBufferValue();
		HRESULT OnAudioSampleRequested( bool IsSilence = false );
		HRESULT ConfigureSource();
		UINT32 GetBufferFramesPerPeriod();

		HRESULT GetToneSample( unsigned int FramesAvailable );

	private:
		Platform::String^			m_DeviceIdString;
		unsigned int				m_BufferFrames;
		HANDLE						m_SampleReadyEvent;
		MFWORKITEM_KEY				m_SampleReadyKey;
		std::mutex                  m_mutex;

		WAVEFORMATEX			   *m_MixFormat;
		
		Microsoft::WRL::ComPtr<IAudioClient2>       m_AudioClient;
		Microsoft::WRL::ComPtr<IAudioRenderClient>  m_AudioRenderClient;
		Microsoft::WRL::ComPtr<IMFAsyncResult>      m_SampleReadyAsyncResult;

		DeviceStateChangedEvent^					m_DeviceStateChanged;
		DEVICEPROPS									m_DeviceProps;

		std::unique_ptr<ToneSampleGenerator>		m_ToneSource;
	};



