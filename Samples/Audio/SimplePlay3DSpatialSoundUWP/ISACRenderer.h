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
		public Microsoft::WRL::RuntimeClass< Microsoft::WRL::RuntimeClassFlags< Microsoft::WRL::ClassicCom >, Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler, ISpatialAudioObjectRenderStreamNotify >
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
		UINT32			GetMaxDynamicObjects() { return m_MaxDynamicObjects; }

		STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation *operation);
		STDMETHOD(OnAvailableDynamicObjectCountChange)(ISpatialAudioObjectRenderStreamBase *sender, LONGLONG hnsComplianceDeadlineTime, UINT32 availableDynamicObjectCountChange);

		Microsoft::WRL::ComPtr<ISpatialAudioClient>					m_SpatialAudioClient;
		Microsoft::WRL::ComPtr<ISpatialAudioObjectRenderStream>		m_SpatialAudioStream;
		HANDLE m_bufferCompletionEvent;
	private:
		~ISACRenderer();


	private:
		Platform::String^		m_DeviceIdString;
		RenderState				m_ISACrenderstate;
		UINT32					m_MaxDynamicObjects;
	};



