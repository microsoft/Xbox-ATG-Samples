//--------------------------------------------------------------------------------------
// DeviceManager.h
//
// Wraps the underlying IMMNotificationClient to manage capture devices
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef DEVICE_MANAGER_H_INCLUDED
#define DEVICE_MANAGER_H_INCLUDED

#include "pch.h"

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl\implements.h>

class DeviceManager : public IMMNotificationClient
{
    
public:
    DeviceManager();
    ~DeviceManager();
            
    ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement(&_cRef);
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG ulRef = InterlockedDecrement(&_cRef);
        if (0 == ulRef)
        {
            delete this;
        }
        return ulRef;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(
                                REFIID riid, VOID **ppvInterface)
    {
        if (IID_IUnknown == riid)
        {
            AddRef();
            *ppvInterface = (IUnknown*)this;
        }
        else if (__uuidof(IMMNotificationClient) == riid)
        {
            AddRef();
            *ppvInterface = (IMMNotificationClient*)this;
        }
        else
        {
            *ppvInterface = nullptr;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    // Callback methods for device-event notifications.
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId);
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId);
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId);
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState);
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId,	const PROPERTYKEY key);

    //--------------------------------------------------------------------------------------
    //  Name: GetCaptureDevices
    //  Desc: Returns a vector of strings containing the names of the capture devices
    //--------------------------------------------------------------------------------------
    void GetCaptureDevices(std::vector<LPWSTR> &device)
    {
        EnterCriticalSection( &m_CritSec );
        device = m_CaptureDevices;
        LeaveCriticalSection( &m_CritSec );
    }

    //--------------------------------------------------------------------------------------
    //  Name: SetDeviceListReport
    //  Desc: Sets the function to call when the device list updates
    //--------------------------------------------------------------------------------------
    void SetDeviceListReport(void (*inFunc)(int))
    {
        m_deviceChangeFunc = inFunc;
    }

    //--------------------------------------------------------------------------------------
    //  Name: SetCaptureId
    //  Desc: Updates the ID of the capture device when it is changed externally
    //--------------------------------------------------------------------------------------
    void SetCaptureId(int inId)
    {
        m_CaptureIndex = inId;
    }

private:
    void UpdateCaptureDeviceList();

    LONG _cRef;

    std::vector<LPWSTR>         m_CaptureDevices;
    int							m_CaptureIndex;

	Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_deviceEnum;

    void						(*m_deviceChangeFunc)(int);
    CRITICAL_SECTION			m_CritSec;

};
    
#endif