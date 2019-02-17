//--------------------------------------------------------------------------------------
// File: MediaEnginePlayer.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//-------------------------------------------------------------------------------------

#include "pch.h"

#include "MediaEnginePlayer.h"

using Microsoft::WRL::ComPtr;

namespace
{
    class MediaEngineNotify : public IMFMediaEngineNotify
    {
        long m_cRef;
        IMFNotify* m_pCB;

    public:
        MediaEngineNotify() noexcept :
            m_cRef(1),
            m_pCB(nullptr)
        {
        }

        STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
        {
            if (__uuidof(IMFMediaEngineNotify) == riid)
            {
                *ppv = static_cast<IMFMediaEngineNotify*>(this);
            }
            else
            {
                *ppv = nullptr;
                return E_NOINTERFACE;
            }

            AddRef();

            return S_OK;
        }

        STDMETHODIMP_(ULONG) AddRef()
        {
            return InterlockedIncrement(&m_cRef);
        }

        STDMETHODIMP_(ULONG) Release()
        {
            LONG cRef = InterlockedDecrement(&m_cRef);
            if (cRef == 0)
            {
                delete this;
            }
            return cRef;
        }

        void SetCallback(IMFNotify* pCB)
        {
            m_pCB = pCB;
        }

        // EventNotify is called when the Media Engine sends an event.
        STDMETHODIMP EventNotify(DWORD meEvent, DWORD_PTR param1, DWORD)
        {
            if (meEvent == MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE)
            {
                SetEvent(reinterpret_cast<HANDLE>(param1));
            }
            else
            {
                m_pCB->OnMediaEngineEvent(meEvent);
            }

            return S_OK;
        }
    };
}

MediaEnginePlayer::MediaEnginePlayer() noexcept :
    m_bkgColor{},
    m_isPlaying(false),
    m_isInfoReady(false),
    m_isFinished(false)
{
}

MediaEnginePlayer::~MediaEnginePlayer()
{
    Shutdown();

    MFShutdown();
}

void MediaEnginePlayer::Initialize(ID3D11Device* device, DXGI_FORMAT format)
{
    // Initialize Media Foundation (see Main.cpp for code to handle Windows N Editions)
    DX::ThrowIfFailed(MFStartup(MF_VERSION));

    // Create our own device to avoid threading issues
    ComPtr<IDXGIDevice> dxgiDevice;
    DX::ThrowIfFailed(device->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf())));

    ComPtr<IDXGIAdapter> adapter;
    DX::ThrowIfFailed(dxgiDevice->GetAdapter(&adapter));

    D3D_FEATURE_LEVEL fLevel = device->GetFeatureLevel();

    DX::ThrowIfFailed(
        D3D11CreateDevice(
        adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        0,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &fLevel,
        1,
        D3D11_SDK_VERSION,
        m_device.GetAddressOf(),
        nullptr,
        nullptr
    ));

    ComPtr<ID3D10Multithread> multithreaded;
    DX::ThrowIfFailed(m_device.As(&multithreaded));
    multithreaded->SetMultithreadProtected(TRUE);

    // Setup Media Engine
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> dxgiManager;
    UINT resetToken;
    DX::ThrowIfFailed(MFCreateDXGIDeviceManager(&resetToken, dxgiManager.GetAddressOf()));
    DX::ThrowIfFailed(dxgiManager->ResetDevice(m_device.Get(), resetToken));

    // Create our event callback object.
    ComPtr<MediaEngineNotify> spNotify = new MediaEngineNotify();
    if (spNotify == nullptr)
    {
        DX::ThrowIfFailed(E_OUTOFMEMORY);
    }

    spNotify->SetCallback(this);

    // Set configuration attribiutes.
    ComPtr<IMFAttributes> attributes;
    DX::ThrowIfFailed(MFCreateAttributes(attributes.GetAddressOf(), 1));
    DX::ThrowIfFailed(attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, reinterpret_cast<IUnknown*>(dxgiManager.Get())));
    DX::ThrowIfFailed(attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, reinterpret_cast<IUnknown*>(spNotify.Get())));
    DX::ThrowIfFailed(attributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, format));

    // Create MediaEngine.
    ComPtr<IMFMediaEngineClassFactory> mfFactory;
    DX::ThrowIfFailed(
        CoCreateInstance(CLSID_MFMediaEngineClassFactory,
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(mfFactory.GetAddressOf())));

    DX::ThrowIfFailed(
        mfFactory->CreateInstance(0,
        attributes.Get(),
        m_mediaEngine.ReleaseAndGetAddressOf()));

    // Create MediaEngineEx
    DX::ThrowIfFailed(m_mediaEngine.As(&m_engineEx));
}

void MediaEnginePlayer::Shutdown()
{
    if (m_mediaEngine)
    {
        m_mediaEngine->Shutdown();
    }
}

void MediaEnginePlayer::Play()
{
    if (m_isPlaying)
        return;

    if (m_mediaEngine)
    {
        DX::ThrowIfFailed(m_mediaEngine->Play());
        m_isPlaying = true;
        m_isFinished = false;
    }
}

void MediaEnginePlayer::SetMuted(bool muted)
{
    if (m_mediaEngine)
    {
        DX::ThrowIfFailed(m_mediaEngine->SetMuted(muted));
    }
}

void MediaEnginePlayer::SetSource(_In_z_ const wchar_t* sourceUri)
{
    static BSTR bstrURL = nullptr;

    if (bstrURL != nullptr)
    {
        ::CoTaskMemFree(bstrURL);
        bstrURL = nullptr;
    }

    size_t cchAllocationSize = 1 + ::wcslen(sourceUri);
    bstrURL = reinterpret_cast<BSTR>(::CoTaskMemAlloc(sizeof(wchar_t)*(cchAllocationSize)));

    if (bstrURL == 0)
    {
        DX::ThrowIfFailed(E_OUTOFMEMORY);
    }

    wcscpy_s(bstrURL, cchAllocationSize, sourceUri);

    m_isPlaying = false;
    m_isInfoReady = false;
    m_isFinished = false;

    if (m_mediaEngine)
    {
        DX::ThrowIfFailed(m_mediaEngine->SetSource(bstrURL));
    }
}

bool MediaEnginePlayer::TransferFrame(ID3D11Texture2D* texture, MFVideoNormalizedRect rect, RECT rcTarget)
{
    if (m_mediaEngine != nullptr && m_isPlaying)
    {
        LONGLONG pts;
        if (m_mediaEngine->OnVideoStreamTick(&pts) == S_OK)
        {
            // The texture must have been created with D3D11_RESOURCE_MISC_SHARED
            ComPtr<IDXGIResource> dxgiTexture;
            DX::ThrowIfFailed(texture->QueryInterface(IID_PPV_ARGS(dxgiTexture.GetAddressOf())));

            HANDLE textureHandle;
            DX::ThrowIfFailed(dxgiTexture->GetSharedHandle(&textureHandle));

            ComPtr<ID3D11Texture2D> mediaTexture;
            if (SUCCEEDED(m_device->OpenSharedResource(textureHandle, IID_PPV_ARGS(mediaTexture.GetAddressOf()))))
            {
                if (m_mediaEngine->TransferVideoFrame(mediaTexture.Get(), &rect, &rcTarget, &m_bkgColor) == S_OK)
                    return true;
            }
        }
    }

    return false;
}

void MediaEnginePlayer::OnMediaEngineEvent(uint32_t meEvent)
{
    switch (meEvent)
    {
        case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
            m_isInfoReady = true;
            break;

        case MF_MEDIA_ENGINE_EVENT_CANPLAY:

            // Here we auto-play when ready...
            Play();

            break;

        case MF_MEDIA_ENGINE_EVENT_PLAY:
            break;

        case MF_MEDIA_ENGINE_EVENT_PAUSE:
            break;

        case MF_MEDIA_ENGINE_EVENT_ENDED:
            m_isFinished = true;
            break;

        case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE:
            break;

        case MF_MEDIA_ENGINE_EVENT_ERROR:
            #ifdef _DEBUG
            if (m_mediaEngine)
            {
                ComPtr<IMFMediaError> error;
                m_mediaEngine->GetError(&error);
                USHORT errorCode = error->GetErrorCode();
                char buff[128] = {};
                sprintf_s(buff, "ERROR: Media Foundation Event Error %u", errorCode);
                OutputDebugStringA(buff);
            }
            #endif
            break;
    }
}

void MediaEnginePlayer::GetNativeVideoSize(uint32_t& cx, uint32_t& cy)
{
    cx = cy = 0;
    if (m_mediaEngine && m_isInfoReady)
    {
        DWORD x, y;
        DX::ThrowIfFailed(m_mediaEngine->GetNativeVideoSize(&x, &y));

        cx = x;
        cy = y;
    }
}
