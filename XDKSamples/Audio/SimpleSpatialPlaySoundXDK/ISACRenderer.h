//--------------------------------------------------------------------------------------
// ISACRenderer.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "SpatialAudioClient.h"
#include <mmdeviceapi.h>

#pragma once

	// Primary ISAC Renderering Class
	class ISACRenderer :
		public Microsoft::WRL::RuntimeClass< Microsoft::WRL::RuntimeClassFlags< Microsoft::WRL::ClassicCom >, Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler >
	{
	public:
		enum RenderState {
			Inactive = 0,
			Active,
			Resetting
		};

		ISACRenderer();

		HRESULT			InitializeAudioDeviceAsync();
		bool			IsActive() { return m_ISACrenderstate == RenderState::Active; };
		bool 			IsResetting() { return m_ISACrenderstate == RenderState::Resetting; }
		void			Reset() { m_ISACrenderstate = RenderState::Resetting; }

		STDMETHOD(ActivateCompleted)( IActivateAudioInterfaceAsyncOperation *operation );

		Microsoft::WRL::ComPtr<ISpatialAudioObjectRenderStream>		m_SpatialAudioStream;

		HANDLE m_bufferCompletionEvent;
	private:
		~ISACRenderer();

		Platform::String^		m_DeviceIdString;
		RenderState				m_ISACrenderstate;

		Microsoft::WRL::ComPtr<ISpatialAudioClient>					m_SpatialAudioClient;

	};



