//--------------------------------------------------------------------------------------
// AdvancedSpatialSoundsUWP.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "AdvancedSpatialSoundsUWP.h"
#include "WAVFileReader.h"

#include "ATGColors.h"
#include "ControllerFont.h"

#define MAX_X   100
#define MIN_X   -100
#define MAX_Y   100
#define MIN_Y   -100
#define MAX_Z   100
#define MIN_Z   -100
#define MAX_VEL 3
#define MAX_RADIUS  90
#define MIN_RADIUS  10
#define CURVE_START 30
#define CURVE_END   170
#define DRAW_BACK_WALL .3f
#define DRAW_POINT_SCALE 40.f
#define DRAW_POINT_MIN_SCALE 2.f

namespace
{
    const LPCWSTR g_bedFileList[] = {
        L"assets\\Jungle_RainThunder_5.1_mixdown.wav",
        L"assets\\Jungle_RainThunder_SideSurroundL-R.wav",
        L"assets\\Jungle_RainThunder_TopFrontL-R.wav",
        L"assets\\Jungle_RainThunder_TopRearL-R.wav",
    };

    const LPCWSTR g_pointFileList[] = {
        L"assets\\SFX_height_birdHawk01.wav",
        L"assets\\SFX_height_birdLoon01.wav",
        L"assets\\SFX_moving_BirdFlap01.wav",
        L"assets\\SFX_moving_birdFlicker01.wav",
        L"assets\\SFX_moving_birdFlycatcher01.wav",
        L"assets\\SFX_moving_birdLark01.wav",
        L"assets\\SFX_moving_birdLoop01.wav",
        L"assets\\SFX_moving_birdLoop02.wav",
        L"assets\\SFX_moving_birdLoop03.wav",
        L"assets\\SFX_moving_birdLoop04.wav",
        L"assets\\SFX_moving_birdLoop05.wav",
        L"assets\\SFX_moving_birdLoop06.wav",
        L"assets\\SFX_moving_birdSparrow01.wav",
        L"assets\\SFX_moving_birdSparrow02.wav",
        L"assets\\SFX_moving_birdWarbler01.wav",
        L"assets\\SFX_moving_Fly01.wav",
        L"assets\\SFX_moving_Fly02.wav",
        L"assets\\SFX_stationary_cicada01.wav",
        L"assets\\SFX_stationary_grasshopper01.wav",
        L"assets\\SFX_stationary_grasshopper02.wav",
    };

    const LPCWSTR g_pointFileNames[] = {
        L"Hawk",
        L"Loon",
        L"BirdFlap",
        L"Flicker",
        L"Flycatcher",
        L"Lark",
        L"Loop 1",
        L"Loop 2",
        L"Loop 3",
        L"Loop 4",
        L"Loop 5",
        L"Loop 6",
        L"Sparrow 1",
        L"Sparrow 2",
        L"Warbler",
        L"Fly 1",
        L"Fly 2",
        L"Cicada",
        L"Grasshopper 1",
        L"Grasshopper 2",
    };

    static_assert(_countof(g_pointFileList) == _countof(g_pointFileNames), "List size mismatch");
}

extern void ExitSample() noexcept;

using namespace DirectX;
using namespace Windows::System::Threading;
using namespace Windows::Media::Devices;
using Microsoft::WRL::ComPtr;

namespace
{
    VOID CALLBACK SpatialWorkCallback(_Inout_ PTP_CALLBACK_INSTANCE Instance, _Inout_opt_ PVOID Context, _Inout_ PTP_WORK Work)
    {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        Sample * Sink = (Sample *)Context;
        Work;
        Instance;

        while (Sink->m_threadActive)
        {
            while (Sink->m_playingSound && Sink->m_renderer->IsActive())
            {
                // Wait for a signal from the audio-engine to start the next processing pass 
                if (WaitForSingleObject(Sink->m_renderer->m_bufferCompletionEvent, 100) != WAIT_OBJECT_0)
                {
                    //make a call to stream to see why we didn't get a signal after 100ms
                    hr = Sink->m_renderer->m_SpatialAudioStream->Reset();

                    //if we have an error, tell the renderer to reset
                    if (hr != S_OK)
                    {
                        Sink->m_renderer->Reset();
                    }
                    continue;
                }


                UINT32 frameCount;
                UINT32 availableObjectCount;

                // Begin the process of sending object data and metadata 
                // Get the number of active object that can be used to send object-data 
                // Get the number of frame count that each buffer be filled with  
                hr = Sink->m_renderer->m_SpatialAudioStream->BeginUpdatingAudioObjects(
                    &availableObjectCount,
                    &frameCount);
                //if we have an error, tell the renderer to reset
                if (hr != S_OK)
                {
                    Sink->m_renderer->Reset();
                }

                Sink->m_availableObjects = availableObjectCount;

                //Update the bed
                for (int chan = 0; chan < MAX_CHANNELS; chan++)
                {
                    //Activate the object if not yet done
                    if (Sink->m_bedChannels[chan].object == nullptr)
                    {
                        // If this method called more than activeObjectCount times 
                        // It will fail with this error HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS) 
                        hr = Sink->m_renderer->m_SpatialAudioStream->ActivateSpatialAudioObject(
                            Sink->m_bedChannels[chan].objType,
                            &Sink->m_bedChannels[chan].object);
                        if (FAILED(hr))
                        {
                            continue;
                        }

                    }

                    //Get the object buffer
                    BYTE* buffer = nullptr;
                    UINT32 bytecount;
                    hr = Sink->m_bedChannels[chan].object->GetBuffer(&buffer, &bytecount);
                    if (FAILED(hr))
                    {
                        continue;
                    }

                    Sink->m_bedChannels[chan].object->SetVolume(Sink->m_bedChannels[chan].volume);

                    for (UINT32 i = 0; i < frameCount * 4; i++)
                    {
                        UINT32 fileLoc = Sink->m_bedChannels[chan].curBufferLoc;
                        if (chan < Sink->m_numChannels)
                        {
                            buffer[i] = Sink->m_bedChannels[chan].wavBuffer[fileLoc];
                        }
                        else
                        {
                            buffer[i] = 0;
                        }

                        Sink->m_bedChannels[chan].curBufferLoc++;
                        if (Sink->m_bedChannels[chan].curBufferLoc == Sink->m_bedChannels[chan].bufferSize)
                        {
                            Sink->m_bedChannels[chan].curBufferLoc = 0;
                        }
                    }
                }

                //Update the point sounds
                bool deleteSound = false;
                AcquireSRWLockExclusive(&Sink->SRWLock);
                for (std::vector<PointSound>::iterator it = Sink->m_pointSounds.begin(); it != Sink->m_pointSounds.end(); it++)
                {
                    //Activate the object if not yet done
                    if (it->object == nullptr)
                    {
                        // If this method called more than activeObjectCount times 
                        // It will fail with this error HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS) 
                        hr = Sink->m_renderer->m_SpatialAudioStream->ActivateSpatialAudioObject(
                            AudioObjectType_Dynamic,
                            &it->object);
                        if (FAILED(hr))
                        {
                            continue;
                        }
                    }

                    //Get the object buffer
                    BYTE* buffer = nullptr;
                    UINT32 bytecount;
                    hr = it->object->GetBuffer(&buffer, &bytecount);
                    if (FAILED(hr))
                    {
                        continue;
                    }

                    if (!it->isPlaying)
                    {
                        // Set end of stream for the last buffer  
                        hr = it->object->SetEndOfStream(0);

                        // Last block of data in the object, release the audio object, 
                        // so the resources can be recycled and used for another object 
                        it->object = nullptr;
                        it->curBufferLoc = 0;

                        deleteSound = true;
                    }
                    else
                    {
                        it->object->SetPosition(it->posX, it->posY, it->posZ);
                        it->object->SetVolume(it->volume);

                        for (UINT32 i = 0; i < frameCount * 4; i++)
                        {
                            UINT32 fileLoc = it->curBufferLoc;
                            buffer[i] = it->wavBuffer[fileLoc];
                            it->curBufferLoc++;
                            if (it->curBufferLoc == it->bufferSize)
                            {
                                it->curBufferLoc = 0;
                            }
                        }
                    }
                }

                if (deleteSound)
                {
                    Sink->m_pointSounds.pop_back();
                }

                ReleaseSRWLockExclusive(&Sink->SRWLock);

                // Let the audio-engine know that the object data are available for processing now 
                hr = Sink->m_renderer->m_SpatialAudioStream->EndUpdatingAudioObjects();
                if (FAILED(hr))
                {
                    Sink->m_renderer->Reset();
                }
            }
        }
    }
}

Sample::Sample() :
    m_numChannels(0),
    m_bedChannels{},
    m_threadActive(false),
    m_playingSound(false),
    m_availableObjects(0),
    m_usedObjects(0),
    m_fileLoaded(false),
    m_workThread(nullptr),
    m_ctrlConnected(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();  	
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();
    
    srand((unsigned int)time(nullptr));

    m_boundingBox = BoundingBox(XMFLOAT3(), XMFLOAT3((MAX_X - MIN_X) / 2, (MAX_Y - MIN_Y) / 2, (MAX_Z - MIN_Z) / 2));

    // Create a new ISAC instance
    m_renderer = Microsoft::WRL::Make<ISACRenderer>();

    // Selects the Default Audio Device
    m_renderer->InitializeAudioDeviceAsync();

    //Load bed files
    m_fileLoaded = LoadBed();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}


//--------------------------------------------------------------------------------------
// Name: DrawRoom()
// Desc: Draws the room
//--------------------------------------------------------------------------------------
void XM_CALLCONV Sample::DrawRoom(FXMVECTOR color)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw room");

    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullNone());

    m_batchEffect->Apply(context);

    context->IASetInputLayout(m_batchInputLayout.Get());

    m_batch->Begin();

    XMVECTORF32 points[4];
    points[0] = { -DRAW_BACK_WALL, -DRAW_BACK_WALL, 0.f };
    points[1] = { -DRAW_BACK_WALL, DRAW_BACK_WALL, 0.f };
    points[2] = { DRAW_BACK_WALL, DRAW_BACK_WALL, 0.f };
    points[3] = { DRAW_BACK_WALL, -DRAW_BACK_WALL, 0.f };

    XMVECTORF32 edgePoints[4];
    edgePoints[0] = { -1.f, -1.f, 0.f };
    edgePoints[1] = { -1.f, 1.f, 0.f };
    edgePoints[2] = { 1.f, 1.f, 0.f };
    edgePoints[3] = { 1.f, -1.f, 0.f };

    VertexPositionColor v[4];
    v[0] = VertexPositionColor(points[0], color);
    v[1] = VertexPositionColor(points[1], color);
    v[2] = VertexPositionColor(points[2], color);
    v[3] = VertexPositionColor(points[3], color);
    m_batch->DrawLine(v[0], v[1]);
    m_batch->DrawLine(v[1], v[2]);
    m_batch->DrawLine(v[2], v[3]);
    m_batch->DrawLine(v[3], v[0]);

    VertexPositionColor edge[4];
    edge[0] = VertexPositionColor(edgePoints[0], color);
    edge[1] = VertexPositionColor(edgePoints[1], color);
    edge[2] = VertexPositionColor(edgePoints[2], color);
    edge[3] = VertexPositionColor(edgePoints[3], color);

    for (size_t i = 0; i <= 4; ++i)
    {
        m_batch->DrawLine(v[i], edge[i]);
    }

    m_batch->End();

    PIXEndEvent(context);
}


//--------------------------------------------------------------------------------------
// Name: DrawSound()
// Desc: Draws a sound point
//--------------------------------------------------------------------------------------
void XM_CALLCONV Sample::DrawSound(float x, float y, float z, FXMVECTOR color)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw point");

    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullNone());

    m_batchEffect->Apply(context);

    context->IASetInputLayout(m_batchInputLayout.Get());

    m_batch->Begin();

    //Z MIN = 0, MAX = 1
    x = (x - MIN_X) / (MAX_X - MIN_X);
    y = (y - MIN_Y) / (MAX_Y - MIN_Y);
    z = (z - MIN_Z) / (MAX_Z - MIN_Z);
    float scale = z * DRAW_POINT_SCALE + DRAW_POINT_MIN_SCALE;

    RECT drawspace = m_deviceResources->GetOutputSize();

    float backWallWidth = DRAW_BACK_WALL * drawspace.right;
    float backWallHeight = DRAW_BACK_WALL * drawspace.bottom;

    float lowBoundX = (drawspace.right - backWallWidth) * (1 - z) / 2;
    float lowBoundY = (drawspace.bottom - backWallHeight) * (1 - z) / 2;
    float highBoundX = drawspace.right - lowBoundX;
    float highBoundY = drawspace.bottom - lowBoundY;
    
    x = x * (highBoundX - lowBoundX) + lowBoundX;
    y = y * (highBoundY - lowBoundY) + lowBoundY;
    
    drawspace.bottom = (LONG)(y + scale);
    drawspace.top = (LONG)y;
    drawspace.left = (LONG)x;
    drawspace.right = (LONG)(x + scale);

    m_spriteBatch->Draw(m_circle.Get(), drawspace, color);

    m_batch->End();

    PIXEndEvent(context);
}

void Sample::StartLoopBack()
{
    _loopBack.Initialize(10);
    _loopBack.Start();
}

void Sample::StopLoopBack()
{
    _loopBack._isCaptureActive = false;
    _loopBack.~CLoopbackCapture();
}

//--------------------------------------------------------------------------------------
// Name: DrawListener()
// Desc: Draws a sound point
//--------------------------------------------------------------------------------------
void XM_CALLCONV Sample::DrawListener(FXMVECTOR color)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw listener");

    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullNone());

    m_batchEffect->Apply(context);

    context->IASetInputLayout(m_batchInputLayout.Get());

    m_batch->Begin();

    float scale = .035f;

    XMVECTORF32 points[3];
    points[0] = { -scale, -scale, 0 };
    points[1] = { scale, -scale, 0 };
    points[2] = { 0, .014f, 0 };

    VertexPositionColor v[3];
    v[0] = VertexPositionColor(points[0], color);
    v[1] = VertexPositionColor(points[1], color);
    v[2] = VertexPositionColor(points[2], color);
    m_batch->DrawTriangle(v[0], v[1], v[2]);

    m_batch->End();

    PIXEndEvent(context);
}


// Updates the world.
void Sample::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_ctrlConnected = true;

        m_gamePadButtons.Update(pad);
    }
    else
    {
        m_ctrlConnected = false;

        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    //Are we resetting the renderer?  This will happen if we get an invalid stream 
    //  which can happen when render mode changes or device changes
    if (m_renderer->IsResetting())
    {
        //clear out renderer
        m_renderer.Reset();

        // Create a new ISAC instance
        m_renderer = Microsoft::WRL::Make<ISACRenderer>();

        // Initialize Default Audio Device
        m_renderer->InitializeAudioDeviceAsync();

        //Reset all the Objects that were being used
        for (int chan = 0; chan < MAX_CHANNELS; chan++)
        {
            m_bedChannels[chan].object = nullptr;
        }
        for (std::vector<PointSound>::iterator it = m_pointSounds.begin(); it != m_pointSounds.end(); it++)
        {
            it->object = nullptr;
        }
    }
    else if(m_renderer->IsActive() && m_pointSounds.size() > m_renderer->GetMaxDynamicObjects())
    { 
        //If we reactivated or available object changed and had more active objects than we do now, clear out those we cannot render
        while (m_pointSounds.size() > m_renderer->GetMaxDynamicObjects())
        {
            m_pointSounds.pop_back();
            m_usedObjects--;
        }
    }

    if (kb.Escape || m_gamePadButtons.view == GamePad::ButtonStateTracker::PRESSED)
    {
        if (m_threadActive)
        {
            m_threadActive = false;
            m_playingSound = false;
            WaitForThreadpoolWorkCallbacks(m_workThread, FALSE);
            CloseThreadpoolWork(m_workThread);
            m_workThread = nullptr;
        }
        m_renderer->m_SpatialAudioStream->Stop();

        ExitSample();
    }

    if (m_keyboardButtons.IsKeyReleased(DirectX::Keyboard::Keys::P))
    {
        m_playingSound = !m_playingSound;
    }

    if (m_keyboardButtons.IsKeyReleased(DirectX::Keyboard::Keys::Space) || m_gamePadButtons.a == m_gamePadButtons.RELEASED)
    {
        if (m_fileLoaded && m_renderer && m_renderer->IsActive())
        {
            //Start spatial worker thread
            if (!m_threadActive)
            {
                m_threadActive = true;
                m_playingSound = true;
                m_workThread = CreateThreadpoolWork(SpatialWorkCallback, this, nullptr);
                SubmitThreadpoolWork(m_workThread);
                StartLoopBack();
            }
            else
            {
                m_threadActive = false;
                m_playingSound = false;
                WaitForThreadpoolWorkCallbacks(m_workThread, FALSE);
                CloseThreadpoolWork(m_workThread);
                m_workThread = nullptr;
                StopLoopBack();
                while (m_pointSounds.size() > 0)
                {
                    m_pointSounds.pop_back();
                    m_usedObjects--;
                }
            }
        }
    }

    if (m_keyboardButtons.IsKeyReleased(DirectX::Keyboard::Keys::Up) || m_gamePadButtons.dpadUp == m_gamePadButtons.RELEASED)
    {
        if (m_threadActive && m_playingSound && m_renderer->IsActive() && m_usedObjects < m_availableObjects)
        {
            PointSound tempChannel;
            int randIndex = rand() % _countof(g_pointFileList);
            if (LoadPointFile(g_pointFileList[randIndex], &tempChannel))
            {
                tempChannel.soundIndex = randIndex;
                tempChannel.travelData.travelType = TravelType(rand() % 3);
                tempChannel.travelData.boundingBox = m_boundingBox;
                tempChannel.travelData.vel = static_cast <float> (rand()) / static_cast <float> (RAND_MAX/ MAX_VEL);
                tempChannel.posX = static_cast <float> (rand()) / static_cast <float> (RAND_MAX / MAX_X - MIN_X);
                tempChannel.posY = static_cast <float> (rand()) / static_cast <float> (RAND_MAX / MAX_Y - MIN_Y);
                tempChannel.posZ = static_cast <float> (rand()) / static_cast <float> (RAND_MAX / MAX_Z - MIN_Z);

                if (tempChannel.travelData.travelType == Round)
                {
                    tempChannel.travelData.radius = static_cast <float> (rand() % (MAX_RADIUS - MIN_RADIUS) + MIN_RADIUS);
                }
                else
                {
                    float x = static_cast <float> (rand()) / static_cast <float> (RAND_MAX / MAX_X - MIN_X);
                    float y = static_cast <float> (rand()) / static_cast <float> (RAND_MAX / MAX_Y - MIN_Y);
                    float z = static_cast <float> (rand()) / static_cast <float> (RAND_MAX / MAX_Z - MIN_Z);

                    XMVECTORF32 temp = { x, y, z, 0 };
                    XMVECTOR vtemp = XMVector3Normalize(temp);
                    XMStoreFloat3(&tempChannel.travelData.direction, vtemp);
                }

                tempChannel.isPlaying = true;

                AcquireSRWLockExclusive(&SRWLock);
                m_pointSounds.emplace_back(tempChannel);
                ReleaseSRWLockExclusive(&SRWLock);

                m_usedObjects++;
            }
        }
    }
    else if (m_keyboardButtons.IsKeyReleased(DirectX::Keyboard::Keys::Down) || m_gamePadButtons.dpadDown == m_gamePadButtons.RELEASED)
    {
        if (m_pointSounds.size() > 0)
        {
            m_pointSounds.back().isPlaying = false;
            m_usedObjects--;
        }
    }

    //Update the point sounds
    for (std::vector<PointSound>::iterator it = m_pointSounds.begin(); it != m_pointSounds.end(); it++)
    {
        switch (it->travelData.travelType)
        {
        case Linear:
            LinearTravel(&(*it));
            break;
        case Bounce:
            BounceTravel(&(*it));
            break;
        case Round:
            RoundTravel(&(*it));
            break;
        }

        float distance = sqrtf(powf(it->posX, 2.0) + powf(it->posY, 2.0) + powf(it->posZ, 2.0));
        float volume = 1.f - ((distance - CURVE_START) / (CURVE_END - CURVE_START));

        if(volume < 0)
        {
            it->volume = 0;
        }
        else if (volume > 1.f)
        {
            it->volume = 1.f;
        }
        else
        {
            it->volume = volume;
        }
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

    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");
    auto rect = m_deviceResources->GetOutputSize();
    auto safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(rect.right, rect.bottom);

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

    m_spriteBatch->Begin();

    float spacing = m_font->GetLineSpacing();

    m_font->DrawString(m_spriteBatch.get(), L"Advanced Spatial Playback", pos, ATG::Colors::White);
    pos.y += spacing * 1.5f;

    if (!m_renderer->IsActive())
    {
        m_font->DrawString(m_spriteBatch.get(), L"Spatial Renderer Not Available", pos, ATG::Colors::White);
        pos.y += spacing * 2.f;
    }
    else
    {
        wchar_t str[256] = {};
        swprintf_s(str, L"State: %ls", (m_threadActive) ? ((m_playingSound) ? L"Playing" : L"Paused") : L"Stopped");
        m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
        pos.y += spacing * 1.5f;

        const wchar_t* str1 = m_ctrlConnected
            ? L"Use [A] button to start/stop playback"
            : L"Use Spacebar to start/stop playback and 'p' to pause/unpause";
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), str1, pos, ATG::Colors::White);
        pos.y += spacing;

        const wchar_t* str2 = m_ctrlConnected
            ? L"Use [Dpad] UP/DOWN to add/remove a sound"
            : L"Use UP/DOWN to add/remove a sound";
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), str2, pos, ATG::Colors::White);
        pos.y += spacing;

        const wchar_t* str3 = m_ctrlConnected
            ? L"Use [View] to exit"
            : L"Use Esc to exit";
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), str3, pos, ATG::Colors::White);
        pos.y += spacing * 2.f;

        swprintf_s(str, L"Available Dynamic Objects: %d", m_availableObjects - m_usedObjects);
        m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
        pos.y += spacing;
        swprintf_s(str, L"Used Dynamic Objects: %d", m_usedObjects);
        m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
        pos.y += spacing;
        swprintf_s(str, L"Total Objects: %d", 12 + m_usedObjects);
        m_font->DrawString(m_spriteBatch.get(), str, pos, ATG::Colors::White);
        pos.y += spacing;
    }

    //float tempx = pos.x;
    
    DrawRoom(Colors::Green);

    for (std::vector<PointSound>::iterator it = m_pointSounds.begin(); it != m_pointSounds.end(); it++)
    {
        if (it->posZ < 0)
        {
            DrawSound(it->posX, it->posY, it->posZ, Colors::Blue);
        }
    }

    DrawListener(Colors::Yellow);

    for (std::vector<PointSound>::iterator it = m_pointSounds.begin(); it != m_pointSounds.end(); it++)
    {
        if (it->posZ >= 0)
        {
            DrawSound(it->posX, it->posY, it->posZ, Colors::Blue);
        }
    }
    
    m_spriteBatch->End();
   
    PIXEndEvent(context);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    PIXEndEvent();
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
void Sample::OnActivated()
{
}

void Sample::OnDeactivated()
{
}

void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->ClearState();

    m_deviceResources->Trim();
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
}

void Sample::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, rotation))
        return;

    CreateWindowSizeDependentResources();
}

void Sample::ValidateDevice()
{
    m_deviceResources->ValidateDevice();
}

// Properties
void Sample::GetDefaultSize(int& width, int& height) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    auto context = m_deviceResources->GetD3DDeviceContext();

    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    m_states = std::make_unique<CommonStates>(device);

    m_batchEffect = std::make_unique<BasicEffect>(device);
    m_batchEffect->SetVertexColorEnabled(true);

    {
        void const* shaderByteCode;
        size_t byteCodeLength;

        m_batchEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

        DX::ThrowIfFailed(
            device->CreateInputLayout(VertexPositionColor::InputElements,
                VertexPositionColor::InputElementCount,
                shaderByteCode, byteCodeLength,
                m_batchInputLayout.ReleaseAndGetAddressOf())
        );
    }

    m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(m_deviceResources->GetD3DDeviceContext());

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");

    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerSmall.spritefont");

    CreateDDSTextureFromFile(device, L"callout_circle.DDS", nullptr, m_circle.ReleaseAndGetAddressOf());
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    m_spriteBatch->SetRotation(m_deviceResources->GetRotation());
}

void Sample::OnDeviceLost()
{
    m_spriteBatch.reset();
    m_font.reset();
    m_ctrlFont.reset();
    m_states.reset();
    m_batch.reset();
    m_batchEffect.reset();
    m_batchInputLayout.Reset();
    m_circle.Reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

bool Sample::LoadBed()
{
    for (UINT32 i = 0; i < MAX_CHANNELS; i++)
    {
        if (m_bedChannels[i].bufferSize)
        {
            delete[] m_bedChannels[i].wavBuffer;
        }
        m_bedChannels[i].bufferSize = 0;
        m_bedChannels[i].curBufferLoc = 0;
        m_bedChannels[i].volume = 1.f;
    }

    HRESULT hr = S_OK;
    std::unique_ptr<uint8_t[]> m_waveFile;
    DirectX::WAVData  WavData;
    LPCWSTR inFile;
    int channelCount = 0;

    for (int fileIndex = 0; fileIndex < _countof(g_bedFileList); fileIndex++)
    {
        inFile = g_bedFileList[fileIndex];

        hr = DirectX::LoadWAVAudioFromFileEx(inFile, m_waveFile, WavData);
        if (FAILED(hr))
        {
            return false;
        }

        if ((WavData.wfx->wFormatTag == 1 || WavData.wfx->wFormatTag == 65534) && WavData.wfx->nSamplesPerSec == 48000)
        {
            m_numChannels += WavData.wfx->nChannels;

            int numSamples = WavData.audioBytes / (2 * WavData.wfx->nChannels);
            for (int i = 0; i < WavData.wfx->nChannels; i++)
            {
                m_bedChannels[channelCount + i].wavBuffer = new char[numSamples * 4];
                m_bedChannels[channelCount + i].bufferSize = numSamples * 4;
            }

            float * tempnew;
            short * tempdata = (short *)WavData.startAudio;

            for (int i = 0; i < numSamples; i++)
            {
                for (int j = 0; j < WavData.wfx->nChannels; j++)
                {
                    tempnew = (float *)m_bedChannels[channelCount + j].wavBuffer;
                    tempnew[i] = (float)tempdata[(i * WavData.wfx->nChannels) + j] / 32768;
                }
            }

            channelCount += WavData.wfx->nChannels;
        }
        else if ((WavData.wfx->wFormatTag == 3) && WavData.wfx->nSamplesPerSec == 48000)
        {
            m_numChannels += WavData.wfx->nChannels;

            int numSamples = WavData.audioBytes / (4 * WavData.wfx->nChannels);
            for (int i = 0; i < WavData.wfx->nChannels; i++)
            {
                m_bedChannels[channelCount + i].wavBuffer = new char[numSamples * 4];
                m_bedChannels[channelCount + i].bufferSize = numSamples * 4;
            }

            float * tempnew;
            short * tempdata = (short *)WavData.startAudio;

            for (int i = 0; i < numSamples; i++)
            {
                for (int j = 0; j < WavData.wfx->nChannels; j++)
                {
                    tempnew = (float *)m_bedChannels[j].wavBuffer;
                    tempnew[i] = (float)tempdata[(i * WavData.wfx->nChannels) + j];
                }
            }

            channelCount += WavData.wfx->nChannels;
        }
        else
        {
            return false;
        }
    }

    m_bedChannels[0].objType = AudioObjectType_FrontLeft;
    m_bedChannels[1].objType = AudioObjectType_FrontRight;
    m_bedChannels[2].objType = AudioObjectType_FrontCenter;
    m_bedChannels[3].objType = AudioObjectType_LowFrequency;
    m_bedChannels[4].objType = AudioObjectType_BackLeft;
    m_bedChannels[5].objType = AudioObjectType_BackRight;
    m_bedChannels[6].objType = AudioObjectType_SideLeft;
    m_bedChannels[7].objType = AudioObjectType_SideRight;
    m_bedChannels[8].objType = AudioObjectType_TopFrontLeft;
    m_bedChannels[9].objType = AudioObjectType_TopFrontRight;
    m_bedChannels[10].objType = AudioObjectType_TopBackLeft;
    m_bedChannels[11].objType = AudioObjectType_TopBackRight;
    
    return true;
}

bool Sample::LoadPointFile(LPCWSTR inFile, PointSound* outChannel)
{
    std::unique_ptr<uint8_t[]> m_waveFile;
    DirectX::WAVData  WavData;

    outChannel->bufferSize = 0;
    outChannel->curBufferLoc = 0;
    outChannel->volume = 1.f;

    if (DirectX::LoadWAVAudioFromFileEx(inFile, m_waveFile, WavData))
    {
        return false;
    }

    if ((WavData.wfx->wFormatTag == 1 || WavData.wfx->wFormatTag == 65534) && WavData.wfx->nSamplesPerSec == 48000 && WavData.wfx->nChannels == 1)
    {
        int numSamples = WavData.audioBytes / 2;
        outChannel->wavBuffer = new char[numSamples * 4];
        outChannel->bufferSize = numSamples * 4;

        float * tempnew;
        short * tempdata = (short *)WavData.startAudio;

        for (int i = 0; i < numSamples; i++)
        {
            tempnew = (float *)outChannel->wavBuffer;
            tempnew[i] = (float)tempdata[i] / 32768;
        }
    }
    else if ((WavData.wfx->wFormatTag == 3) && WavData.wfx->nSamplesPerSec == 48000 && WavData.wfx->nChannels == 1)
    {
        int numSamples = WavData.audioBytes / 4;
        outChannel->wavBuffer = new char[numSamples * 4];
        outChannel->bufferSize = numSamples * 4;

        float * tempnew;
        float * tempdata = (float *)WavData.startAudio;

        for (int i = 0; i < numSamples; i++)
        {
            tempnew = (float *)outChannel->wavBuffer;
            tempnew[i] = (float)tempdata[i];
        }
    }
    else
    {
        return false;
    }

    return true;
}

void Sample::LinearTravel(PointSound* inSound)
{
    //Travel in one direction until hitting a wall, then reverse
    XMVECTORF32 staringPosition = { inSound->posX, inSound->posY, inSound->posZ, 0 };
    XMVECTOR direction = XMLoadFloat3(&inSound->travelData.direction);
    XMVECTOR newPoint = XMVectorAdd(XMVectorScale(direction, inSound->travelData.vel), staringPosition);

    if (inSound->travelData.boundingBox.Contains(newPoint) == DISJOINT)
    {
        //Find the intersection point
        float distance = 0;
        XMVECTOR newDirection = XMVectorNegate(direction);
        inSound->travelData.boundingBox.Intersects(newPoint, newDirection, distance);
        XMVECTOR intersectPoint = XMVectorMultiplyAdd(direction, XMVectorReplicate(inSound->travelData.vel - distance), staringPosition);
        
        //Bounce back the way we came
        newPoint = XMVectorAdd(XMVectorScale(newDirection, distance), intersectPoint);
        XMStoreFloat3(&inSound->travelData.direction, newDirection);
    }

    inSound->posX = XMVectorGetX(newPoint);
    inSound->posY = XMVectorGetY(newPoint);
    inSound->posZ = XMVectorGetZ(newPoint);
}

void Sample::BounceTravel(PointSound* inSound)
{
    //Travel in one direction until hitting a wall, then bounce
    XMVECTORF32 staringPosition = { inSound->posX, inSound->posY, inSound->posZ, 0 };
    XMVECTOR direction = XMLoadFloat3(&inSound->travelData.direction);
    XMVECTOR newPoint = XMVectorAdd(XMVectorScale(direction, inSound->travelData.vel), staringPosition);

    if (inSound->travelData.boundingBox.Contains(newPoint) == DISJOINT)
    {
        //Find the intersection point
        float distance = 0;
        XMVECTOR newDirection = XMVectorNegate(direction);
        inSound->travelData.boundingBox.Intersects(newPoint, newDirection, distance);
        XMVECTOR intersectPoint = XMVectorMultiplyAdd(direction, XMVectorReplicate(inSound->travelData.vel - distance), staringPosition);
        
        //Build the vector to multiply
        XMFLOAT3 tempPoint;
        XMStoreFloat3(&tempPoint, intersectPoint);
        
        float checkPointHigh = inSound->travelData.boundingBox.Center.x + inSound->travelData.boundingBox.Extents.x;
        float checkPointLow = inSound->travelData.boundingBox.Center.x - inSound->travelData.boundingBox.Extents.x;

        if (tempPoint.x == checkPointHigh || tempPoint.x == checkPointLow)
        {
            tempPoint.x = -1.f;
        }
        else
        {
            tempPoint.x = 1.f;
        }

        checkPointHigh = inSound->travelData.boundingBox.Center.x + inSound->travelData.boundingBox.Extents.x;
        checkPointLow = inSound->travelData.boundingBox.Center.x - inSound->travelData.boundingBox.Extents.x;

        if (tempPoint.y == checkPointHigh || tempPoint.y == checkPointLow)
        {
            tempPoint.y = -1.f;
        }
        else
        {
            tempPoint.y = 1.f;
        }

        checkPointHigh = inSound->travelData.boundingBox.Center.x + inSound->travelData.boundingBox.Extents.x;
        checkPointLow = inSound->travelData.boundingBox.Center.x - inSound->travelData.boundingBox.Extents.x;

        if (tempPoint.z == checkPointHigh || tempPoint.z == checkPointLow)
        {
            tempPoint.z = -1.f;
        }
        else
        {
            tempPoint.z = 1.f;
        }

        direction = XMVectorMultiply(direction, XMLoadFloat3(&tempPoint));
        newPoint = XMVectorAdd(XMVectorScale(direction, distance), intersectPoint);
        XMStoreFloat3(&inSound->travelData.direction, direction);
    }

    inSound->posX = XMVectorGetX(newPoint);
    inSound->posY = XMVectorGetY(newPoint);
    inSound->posZ = XMVectorGetZ(newPoint);
}

void Sample::RoundTravel(PointSound* inSound)
{
    //Travel in a cirle around the listener
    XMVECTORF32 start = { inSound->posX, inSound->posY, inSound->posZ, 0 };
    XMMATRIX transform = XMMatrixRotationZ(inSound->travelData.vel / inSound->travelData.radius);
    XMVECTOR newPoint = XMVector3Transform(start, transform);
    inSound->posX = XMVectorGetX(newPoint);
    inSound->posY = XMVectorGetY(newPoint);
    inSound->posZ = XMVectorGetZ(newPoint);
}
