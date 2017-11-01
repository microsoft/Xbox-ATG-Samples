//--------------------------------------------------------------------------------------
// WASAPIManager.h
//
// Wraps the underlying WASAPIRenderer in a simple WinRT class that can receive
// the DeviceStateChanged events.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef WASAPI_MANAGER_H_INCLUDED
#define WASAPI_MANAGER_H_INCLUDED

#include "pch.h"
#include "DeviceState.h"
#include "WASAPIRenderer.h"
#include "WASAPICapture.h"
#include "CBuffer.h"
#include "DeviceManager.h"

static const LPCWSTR g_FileName = L"Recording.wav";

struct ManagerStatus
{
	bool bCapturing;
	bool bPlaying;
	bool bLoopback;
};
		
ref class WASAPIManager sealed
{
public:
	WASAPIManager();

	void StartDevice();
	void PlayPauseToggle();
			
	void RecordToggle();
	void LoopbackToggle();
	void SetCaptureDevice(int index);

	void UpdateStatus();

internal:
	void GetStatus(ManagerStatus *inStatus);

	//--------------------------------------------------------------------------------------
	//  Name: SetDeviceChangeCallback
	//  Desc: Sets the callback when capture devices change
	//--------------------------------------------------------------------------------------
	void SetDeviceChangeCallback(void (*inFunc)(int))
	{
		m_deviceManager.SetDeviceListReport(inFunc);
	}

	void GetCaptureDevices(std::vector<LPWSTR> &device)
	{
		m_deviceManager.GetCaptureDevices(device);
	}

private:
	~WASAPIManager();

	void OnDeviceStateChange( Object^ sender, DeviceStateChangedEventArgs^ e );

	void InitializeRenderDevice();
	void InitializeCaptureDevice();

	Windows::Foundation::EventRegistrationToken     m_RenderDeviceStateChangeToken;
	Windows::Foundation::EventRegistrationToken     m_CaptureDeviceStateChangeToken;

	DeviceStateChangedEvent^    m_RenderStateChangedEvent;
	DeviceStateChangedEvent^    m_CaptureStateChangedEvent;
	Microsoft::WRL::ComPtr<WASAPIRenderer>      m_Renderer;
	Microsoft::WRL::ComPtr<WASAPICapture>       m_Capture;

	DeviceManager				m_deviceManager;

	WAVEFORMATEX		        m_CaptureWfx;
	WAVEFORMATEX		        m_RenderWfx;
	bool						m_bUseLoopback;
	UINT						m_CaptureIndex;

	CBuffer* 					m_captureBuffer;

	CRITICAL_SECTION			m_CritSec;
	ManagerStatus				m_Status;
};

#endif