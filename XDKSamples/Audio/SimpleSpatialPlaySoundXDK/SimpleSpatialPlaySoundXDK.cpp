//--------------------------------------------------------------------------------------
// SimpleSpatialPlaySoundXDK.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleSpatialPlaySoundXDK.h"

#include "ATGColors.h"
#include "ControllerFont.h"
#include "WAVFileReader.h"

namespace
{
    const LPCWSTR g_FileList[] =
    {
        L"Jungle_RainThunder_mix714.wav",
        L"ChannelIDs714.wav",
    };

    const int numFiles = _countof(g_FileList);
}

extern void ExitSample();

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
    VOID CALLBACK SpatialWorkCallback(_Inout_ PTP_CALLBACK_INSTANCE Instance, _Inout_opt_ PVOID Context, _Inout_ PTP_WORK Work)
    {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        Sample * Sink = (Sample *)Context;
        Work;
        Instance;

        while (Sink->m_bThreadActive)
        {
            while (Sink->m_bPlayingSound && Sink->m_Renderer->IsActive())
            {
                // Wait for a signal from the audio-engine to start the next processing pass 
                if (WaitForSingleObject(Sink->m_Renderer->m_bufferCompletionEvent, 100) != WAIT_OBJECT_0)
                {
                    //make a call to stream to see why we didn't get a signal after 100ms
                    hr = Sink->m_Renderer->m_SpatialAudioStream->Reset();

                    //if we have a stream error, set the renderer state to reset
                    if (hr != S_OK)
                    {
                        Sink->m_Renderer->Reset();
                    }
                    continue;
                }

                UINT32 frameCount;
                UINT32 availableObjectCount;


                // Begin the process of sending object data and metadata 
                // Get the number of active object that can be used to send object-data 
                // Get the number of frame count that each buffer be filled with  
                hr = Sink->m_Renderer->m_SpatialAudioStream->BeginUpdatingAudioObjects(
                    &availableObjectCount,
                    &frameCount);

                //if we have a stream error, set the renderer state to reset
                if (hr != S_OK)
                {
                    Sink->m_Renderer->Reset();
                }

                Sink->m_availableObjects = availableObjectCount;

                for (int chan = 0; chan < MAX_CHANNELS; chan++)
                {
                    //Activate the object if not yet done
                    if (Sink->WavChannels[chan].object == nullptr)
                    {
                        // If this method called more than activeObjectCount times 
                        // It will fail with this error HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS) 
                        hr = Sink->m_Renderer->m_SpatialAudioStream->ActivateSpatialAudioObject(
                            Sink->WavChannels[chan].objType,
                            &Sink->WavChannels[chan].object);
                        if (FAILED(hr))
                        {
                            continue;
                        }

                    }

                    //Get the object buffer
                    BYTE* buffer = nullptr;
                    UINT32 bytecount;
                    hr = Sink->WavChannels[chan].object->GetBuffer(&buffer, &bytecount);
                    if (FAILED(hr))
                    {
                        continue;
                    }

                    Sink->WavChannels[chan].object->SetVolume(Sink->WavChannels[chan].volume);

                    UINT32 readsize = bytecount;

                    for (UINT32 i = 0; i < readsize; i++)
                    {
                        UINT32 fileLoc = Sink->WavChannels[chan].curBufferLoc;
                        if (chan < Sink->m_numChannels)
                        {
                            buffer[i] = Sink->WavChannels[chan].wavBuffer[fileLoc];
                        }
                        else
                        {
                            buffer[i] = 0;
                        }

                        Sink->WavChannels[chan].curBufferLoc++;
                        if (Sink->WavChannels[chan].curBufferLoc == Sink->WavChannels[chan].buffersize)
                        {
                            Sink->WavChannels[chan].curBufferLoc = 0;
                        }


                    }
                }

                // Let the audio-engine know that the object data are available for processing now 
                hr = Sink->m_Renderer->m_SpatialAudioStream->EndUpdatingAudioObjects();
                if (FAILED(hr))
                {
                    Sink->m_Renderer->Reset();
                    continue;
                }
            }
        }

    }
}

Sample::Sample() :
    m_numChannels(0),
    WavChannels{},
    m_bThreadActive(false),
    m_bPlayingSound(false),
    m_availableObjects(0),
    m_frame(0),
    m_fileLoaded(false),
    m_curFile(0),
    m_workThread(nullptr)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
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

    InitializeSpatialStream();

    SetChannelPosVolumes();

    for (UINT32 i = 0; i < MAX_CHANNELS; i++)
    {
        WavChannels[i].buffersize = 0;
        WavChannels[i].curBufferLoc = 0;
        WavChannels[i].object = nullptr;

    }

    m_fileLoaded = LoadFile(g_FileList[m_curFile]);
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

    //Are we resetting the renderer?  This will happen if we get an invalid stream 
    //  which can happen when render mode changes or device changes
    if (m_Renderer->IsResetting())
    {
        //clear out renderer
        m_Renderer.Reset();

        // Create a new ISAC instance
        m_Renderer = Microsoft::WRL::Make<ISACRenderer>();

        // Initialize Default Audio Device
        m_Renderer->InitializeAudioDeviceAsync();

        //Reset all the Objects that were being used
        for (int chan = 0; chan < MAX_CHANNELS; chan++)
        {
            WavChannels[chan].object = nullptr;
        }
    }
    
    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }
        if (m_gamePadButtons.a == m_gamePadButtons.RELEASED)
        {
            //if we have an active renderer
            if (m_Renderer && m_Renderer->IsActive())
            {
                //Start spatial worker thread
                if (!m_bThreadActive)
                {
                    //reload file to start again
                    m_fileLoaded = LoadFile(g_FileList[m_curFile]);
                    //startup spatial thread
                    m_bThreadActive = true;
                    m_bPlayingSound = true;
                    m_workThread = CreateThreadpoolWork(SpatialWorkCallback, this, nullptr);
                    SubmitThreadpoolWork(m_workThread);
                }
                else
                {
                    //stop and shutdown spatial worker thread
                    m_bThreadActive = false;
                    m_bPlayingSound = false;
                    WaitForThreadpoolWorkCallbacks(m_workThread, FALSE);
                    CloseThreadpoolWork(m_workThread);
                    m_workThread = nullptr;
                }
            }
        }
        else if (m_gamePadButtons.b == m_gamePadButtons.RELEASED)
        {
            //if we have an active renderer
            if (m_Renderer && m_Renderer->IsActive())
            {
                //if spatial thread active and playing, shutdown and start new file
                if (m_bThreadActive && m_bPlayingSound)
                {
                    m_bThreadActive = false;
                    m_bPlayingSound = false;
                    WaitForThreadpoolWorkCallbacks(m_workThread, FALSE);
                    CloseThreadpoolWork(m_workThread);
                    m_workThread = nullptr;

                    //load next file
                    m_curFile++;
                    if (m_curFile == numFiles) m_curFile = 0;
                    m_fileLoaded = LoadFile(g_FileList[m_curFile]);

                    //if successfully loaded, start up thread and playing new file
                    if (m_fileLoaded)
                    {
                        m_bThreadActive = true;
                        m_bPlayingSound = true;
                        m_workThread = CreateThreadpoolWork(SpatialWorkCallback, this, nullptr);
                        SubmitThreadpoolWork(m_workThread);
                    }
                }
                else
                {
                    //if thread active but paused, shutdown thread to load new file
                    if (m_bThreadActive)
                    {
                        m_bThreadActive = false;
                        m_bPlayingSound = false;
                        WaitForThreadpoolWorkCallbacks(m_workThread, FALSE);
                        CloseThreadpoolWork(m_workThread);
                        m_workThread = nullptr;
                    }

                    //load next file
                    m_curFile++;
                    if (m_curFile == numFiles) m_curFile = 0;
                    m_fileLoaded = LoadFile(g_FileList[m_curFile]);
                }
            }
        }

    }
    else
    {
        m_gamePadButtons.Reset();
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
    auto rect = m_deviceResources->GetOutputSize();
    auto safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(rect.right, rect.bottom);

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

    m_spriteBatch->Begin();

    float spacing = m_font->GetLineSpacing();

    m_font->DrawString(m_spriteBatch.get(), L"Simple Spatial Playback:", pos, ATG::Colors::White);
    pos.y += spacing * 1.5f;

    if (!m_Renderer->IsActive())
    {
        m_font->DrawString(m_spriteBatch.get(), L"Spatial Renderer Not Available", pos, ATG::Colors::Orange);
        pos.y += spacing * 2.f;
    }
    else
    {
        wchar_t str[256] = {};
        swprintf_s(str, L"   File: %ls", g_FileList[m_curFile]);
        m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
        pos.y += spacing;
        swprintf_s(str, L"   State: %ls", ((m_bPlayingSound) ? L"Playing" : L"Stopped"));
        m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
        pos.y += spacing * 1.5f;

        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), L"Use [A] to start/stop playback", pos, ATG::Colors::White);
        pos.y += spacing;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), L"Use [B] to change to next file", pos, ATG::Colors::White);
        pos.y += spacing;
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), L"Use [View] to exit", pos, ATG::Colors::White);
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

    // Clear the views.
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

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
}

void Sample::OnResuming()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->Resume();
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    auto context = m_deviceResources->GetD3DDeviceContext();

    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");

    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerSmall.spritefont");
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion

HRESULT Sample::InitializeSpatialStream(void)
{
    HRESULT hr = S_OK;

    if (!m_Renderer)
    {
        // Create a new ISAC instance
        m_Renderer = Microsoft::WRL::Make<ISACRenderer>();
        // Selects the Default Audio Device
        hr = m_Renderer->InitializeAudioDeviceAsync();
    }

    return hr;
}

bool Sample::LoadFile(LPCWSTR inFile)
{
    //clear and reset wavchannels
    for (UINT32 i = 0; i < MAX_CHANNELS; i++)
    {
        if (WavChannels[i].buffersize)
        {
            delete[] WavChannels[i].wavBuffer;
        }
        WavChannels[i].buffersize = 0;
        WavChannels[i].curBufferLoc = 0;
        WavChannels[i].object = nullptr;
    }

    HRESULT hr = S_OK;
    std::unique_ptr<uint8_t[]>              m_waveFile;
    DX::WAVData  WavData;
    hr = DX::LoadWAVAudioFromFileEx(inFile, m_waveFile, WavData);
    if (FAILED(hr))
    {
        return false;
    }

    if ((WavData.wfx->wFormatTag == 1 || WavData.wfx->wFormatTag == 65534) && WavData.wfx->nSamplesPerSec == 48000)
    {
        m_numChannels = WavData.wfx->nChannels;

        int numSamples = WavData.audioBytes / (2 * m_numChannels);
        for (int i = 0; i < m_numChannels; i++)
        {
            WavChannels[i].wavBuffer = new char[numSamples * 4];
            WavChannels[i].buffersize = numSamples * 4;
        }

        float * tempnew;
        short * tempdata = (short *)WavData.startAudio;

        for (int i = 0; i < numSamples; i++)
        {
            for (int j = 0; j < m_numChannels; j++)
            {
                int chan = j;
                tempnew = (float *)WavChannels[chan].wavBuffer;
                int sample = (i * m_numChannels) + chan;
                tempnew[i] = (float)tempdata[sample] / 32768;
            }
        }
    }
    else if ((WavData.wfx->wFormatTag == 3) && WavData.wfx->nSamplesPerSec == 48000)
    {
        m_numChannels = WavData.wfx->nChannels;
        int numSamples = WavData.audioBytes / (4 * m_numChannels);
        for (int i = 0; i < m_numChannels; i++)
        {
            WavChannels[i].wavBuffer = new char[numSamples * 4];
            WavChannels[i].buffersize = numSamples * 4;
        }

        float * tempnew;
        float * tempdata = (float *)WavData.startAudio;

        for (int i = 0; i < numSamples; i++)
        {
            for (int j = 0; j < m_numChannels; j++)
            {
                int chan = j;
                tempnew = (float *)WavChannels[chan].wavBuffer;
                int sample = (i * m_numChannels) + chan;
                tempnew[i] = (float)tempdata[sample];
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}



void Sample::SetChannelPosVolumes(void)
{
    WavChannels[0].volume = 1.f;
    WavChannels[0].objType = AudioObjectType_FrontLeft;

    WavChannels[1].volume = 1.f;
    WavChannels[1].objType = AudioObjectType_FrontRight;

    WavChannels[2].volume = 1.f;
    WavChannels[2].objType = AudioObjectType_FrontCenter;

    WavChannels[3].volume = 1.f;
    WavChannels[3].objType = AudioObjectType_LowFrequency;

    WavChannels[4].volume = 1.f;
    WavChannels[4].objType = AudioObjectType_BackLeft;

    WavChannels[5].volume = 1.f;
    WavChannels[5].objType = AudioObjectType_BackRight;

    WavChannels[6].volume = 1.f;
    WavChannels[6].objType = AudioObjectType_SideLeft;

    WavChannels[7].volume = 1.f;
    WavChannels[7].objType = AudioObjectType_SideRight;

    WavChannels[8].volume = 1.f;
    WavChannels[8].objType = AudioObjectType_TopFrontLeft;

    WavChannels[9].volume = 1.f;
    WavChannels[9].objType = AudioObjectType_TopFrontRight;

    WavChannels[10].volume = 1.f;
    WavChannels[10].objType = AudioObjectType_TopBackLeft;

    WavChannels[11].volume = 1.f;
    WavChannels[11].objType = AudioObjectType_TopBackRight;

}

