//--------------------------------------------------------------------------------------
// SimplePlay3DSound.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimplePlay3DSound.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
    const wchar_t* c_WaveFile = L"ATG_SpatialMotion_monoFunkDrums1Loop.wav";
    const float c_RotateScale = 0.1f;
    const float c_MaxHeight = 100;
    const float c_MoveScale = 3.0f;

    const XAUDIO2FX_REVERB_I3DL2_PARAMETERS g_ReverbPreset[] =
    {
        XAUDIO2FX_I3DL2_PRESET_FOREST,
        XAUDIO2FX_I3DL2_PRESET_DEFAULT,
        XAUDIO2FX_I3DL2_PRESET_GENERIC,
        XAUDIO2FX_I3DL2_PRESET_PADDEDCELL,
        XAUDIO2FX_I3DL2_PRESET_ROOM,
        XAUDIO2FX_I3DL2_PRESET_BATHROOM,
        XAUDIO2FX_I3DL2_PRESET_LIVINGROOM,
        XAUDIO2FX_I3DL2_PRESET_STONEROOM,
        XAUDIO2FX_I3DL2_PRESET_AUDITORIUM,
        XAUDIO2FX_I3DL2_PRESET_CONCERTHALL,
        XAUDIO2FX_I3DL2_PRESET_CAVE,
        XAUDIO2FX_I3DL2_PRESET_ARENA,
        XAUDIO2FX_I3DL2_PRESET_HANGAR,
        XAUDIO2FX_I3DL2_PRESET_CARPETEDHALLWAY,
        XAUDIO2FX_I3DL2_PRESET_HALLWAY,
        XAUDIO2FX_I3DL2_PRESET_STONECORRIDOR,
        XAUDIO2FX_I3DL2_PRESET_ALLEY,
        XAUDIO2FX_I3DL2_PRESET_CITY,
        XAUDIO2FX_I3DL2_PRESET_MOUNTAINS,
        XAUDIO2FX_I3DL2_PRESET_QUARRY,
        XAUDIO2FX_I3DL2_PRESET_PLAIN,
        XAUDIO2FX_I3DL2_PRESET_PARKINGLOT,
        XAUDIO2FX_I3DL2_PRESET_SEWERPIPE,
        XAUDIO2FX_I3DL2_PRESET_UNDERWATER,
        XAUDIO2FX_I3DL2_PRESET_SMALLROOM,
        XAUDIO2FX_I3DL2_PRESET_MEDIUMROOM,
        XAUDIO2FX_I3DL2_PRESET_LARGEROOM,
        XAUDIO2FX_I3DL2_PRESET_MEDIUMHALL,
        XAUDIO2FX_I3DL2_PRESET_LARGEHALL,
        XAUDIO2FX_I3DL2_PRESET_PLATE,
    };

    const wchar_t* g_PresetNames[] =
    {
        L"Forest",
        L"Default",
        L"Generic",
        L"Padded cell",
        L"Room",
        L"Bathroom",
        L"Living room",
        L"Stone room",
        L"Auditorium",
        L"Concert hall",
        L"Cave",
        L"Arena",
        L"Hangar",
        L"Carpeted hallway",
        L"Hallway",
        L"Stone Corridor",
        L"Alley",
        L"City",
        L"Mountains",
        L"Quarry",
        L"Plain",
        L"Parking lot",
        L"Sewer pipe",
        L"Underwater",
        L"Small room",
        L"Medium room",
        L"Large room",
        L"Medium hall",
        L"Large hall",
        L"Plate",
    };

    static_assert(_countof(g_ReverbPreset) == _countof(g_PresetNames), "Preset array mismatch");
}

Sample::Sample() :
    m_masteringVoice(nullptr),
    m_sourceVoice(nullptr),
    m_submixVoice(nullptr),
    m_reverbIndex(0),
    m_gamepadPresent(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
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

    //Start XAudio2
    DX::ThrowIfFailed(XAudio2Create(m_XAudio2.GetAddressOf(), 0));
    DX::ThrowIfFailed(m_XAudio2->CreateMasteringVoice(&m_masteringVoice));

    DWORD channelMask;
    DX::ThrowIfFailed(m_masteringVoice->GetChannelMask(&channelMask));

    m_masteringVoice->GetVoiceDetails(&m_deviceDetails);

    //Add reverb on a submix
    DX::ThrowIfFailed(XAudio2CreateReverb(&m_reverbEffect, 0));

    XAUDIO2_EFFECT_DESCRIPTOR effects[] = { { m_reverbEffect.Get(), TRUE, 1 } };
    XAUDIO2_EFFECT_CHAIN effectChain = { 1, effects };

    DX::ThrowIfFailed(m_XAudio2->CreateSubmixVoice(&m_submixVoice, 1, m_deviceDetails.InputSampleRate, 0, 0, nullptr, &effectChain));

    SetReverb(0);

    //Start X3DAudio
    DX::ThrowIfFailed(X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, m_X3DInstance));

    memset(&m_X3DDSPSettings, 0, sizeof(X3DAUDIO_DSP_SETTINGS));

    //Setup DSP
    m_matrix = std::make_unique<float[]>(m_deviceDetails.InputChannels);
    m_X3DDSPSettings.SrcChannelCount = 1;
    m_X3DDSPSettings.DstChannelCount = m_deviceDetails.InputChannels;
    m_X3DDSPSettings.pMatrixCoefficients = m_matrix.get();

    SetupEnvironment();

    //Start the audio playback
    PlayFile(c_WaveFile);
}


//--------------------------------------------------------------------------------------
// Name: SetupEnvironment()
// Desc: Sets the environment for audio playback
//--------------------------------------------------------------------------------------

void Sample::SetupEnvironment()
{
    memset(&m_X3DListener, 0, sizeof(X3DAUDIO_LISTENER));
    memset(&m_X3DEmitter, 0, sizeof(X3DAUDIO_EMITTER));

    //Listener is facing the top of the screen
    m_X3DListener.OrientFront.y = -1.0f;
    m_X3DListener.OrientTop.z = 1.0f;
    m_listenerAngle = 0.0f;

    //Emitter is facing the bottom of the screen
    m_X3DEmitter.OrientFront.y = 1.0f;
    m_X3DEmitter.OrientTop.z = 1.0f;
    m_emitterAngle = X3DAUDIO_PI;

    //Audio in use is in mono
    m_X3DEmitter.ChannelCount = 1;

    //Volume attenuation curve
    m_volumePoints[0].Distance = 0.0f;
    m_volumePoints[0].DSPSetting = 1.0f;
    m_volumePoints[1].Distance = 0.2f;
    m_volumePoints[1].DSPSetting = 1.0f;
    m_volumePoints[2].Distance = 0.3f;
    m_volumePoints[2].DSPSetting = 0.5f;
    m_volumePoints[3].Distance = 0.4f;
    m_volumePoints[3].DSPSetting = 0.35f;
    m_volumePoints[4].Distance = 0.5f;
    m_volumePoints[4].DSPSetting = 0.23f;
    m_volumePoints[5].Distance = 0.6f;
    m_volumePoints[5].DSPSetting = 0.16f;
    m_volumePoints[6].Distance = 0.7f;
    m_volumePoints[6].DSPSetting = 0.1f;
    m_volumePoints[7].Distance = 0.8f;
    m_volumePoints[7].DSPSetting = 0.06f;
    m_volumePoints[8].Distance = 0.9f;
    m_volumePoints[8].DSPSetting = 0.04f;
    m_volumePoints[9].Distance = 1.0f;
    m_volumePoints[9].DSPSetting = 0.0f;
    m_volumeCurve.PointCount = 10;
    m_volumeCurve.pPoints = m_volumePoints;

    //Reverb attenuation curve
    m_reverbPoints[0].Distance = 0.0f;
    m_reverbPoints[0].DSPSetting = 0.7f;
    m_reverbPoints[1].Distance = 0.2f;
    m_reverbPoints[1].DSPSetting = 0.78f;
    m_reverbPoints[2].Distance = 0.3f;
    m_reverbPoints[2].DSPSetting = 0.85f;
    m_reverbPoints[3].Distance = 0.4f;
    m_reverbPoints[3].DSPSetting = 1.0f;
    m_reverbPoints[4].Distance = 0.5f;
    m_reverbPoints[4].DSPSetting = 1.0f;
    m_reverbPoints[5].Distance = 0.6f;
    m_reverbPoints[5].DSPSetting = 0.6f;
    m_reverbPoints[6].Distance = 0.7f;
    m_reverbPoints[6].DSPSetting = 0.4f;
    m_reverbPoints[7].Distance = 0.8f;
    m_reverbPoints[7].DSPSetting = 0.25f;
    m_reverbPoints[8].Distance = 0.9f;
    m_reverbPoints[8].DSPSetting = 0.11f;
    m_reverbPoints[9].Distance = 1.0f;
    m_reverbPoints[9].DSPSetting = 0.0f;
    m_reverbCurve.PointCount = 10;
    m_reverbCurve.pPoints = m_reverbPoints;

    //Add the curves to every emitter
    m_X3DEmitter.pVolumeCurve = &m_volumeCurve;
    m_X3DEmitter.pReverbCurve = &m_reverbCurve;

    //Emitter cone (only for the first "mode")
    m_emitterCone.InnerAngle = X3DAUDIO_PI / 2;
    m_emitterCone.OuterAngle = X3DAUDIO_PI;
    m_emitterCone.InnerVolume = 1.0f;
    m_emitterCone.OuterVolume = 0.0f;
    m_emitterCone.InnerReverb = 1.0f;
    m_emitterCone.OuterReverb = 0.0f;
    m_X3DEmitter.pCone = &m_emitterCone;

    //Set how much distance influences the sound
    m_X3DEmitter.CurveDistanceScaler = 300;

    //Start the listener and emitter in the middle of the screen
    auto rect = m_deviceResources->GetOutputSize();
    m_X3DListener.Position = X3DAUDIO_VECTOR(float(rect.right / 2), float(rect.bottom / 2), 0);
    m_X3DEmitter.Position = X3DAUDIO_VECTOR(float(rect.right / 2), float((rect.bottom / 2) - 100), 0);
}

//--------------------------------------------------------------------------------------
// Name: PlayFile()
// Desc: Starts playback from the given wavefile
//--------------------------------------------------------------------------------------
void Sample::PlayFile(const wchar_t* filename)
{
    // Read the wave file
    DX::WAVData waveData;
    DX::ThrowIfFailed(DX::LoadWAVAudioFromFileEx(filename, m_waveFile, waveData));

    XAUDIO2_SEND_DESCRIPTOR sendDescriptors[2] = {};
    sendDescriptors[0].Flags = XAUDIO2_SEND_USEFILTER;
    sendDescriptors[0].pOutputVoice = m_masteringVoice;
    sendDescriptors[1].Flags = XAUDIO2_SEND_USEFILTER;
    sendDescriptors[1].pOutputVoice = m_submixVoice;
    const XAUDIO2_VOICE_SENDS sendList = { 2, sendDescriptors };

    // Create the source voice
    DX::ThrowIfFailed(m_XAudio2->CreateSourceVoice(&m_sourceVoice, waveData.wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sendList));

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {};
    buffer.pAudioData = waveData.startAudio;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = waveData.audioBytes;
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;

    DX::ThrowIfFailed(m_sourceVoice->SubmitSourceBuffer(&buffer));
    DX::ThrowIfFailed(m_sourceVoice->Start(0));
}

//--------------------------------------------------------------------------------------
// Name: SetReverb()
// Desc: Sets the reverb settings
//--------------------------------------------------------------------------------------
void Sample::SetReverb(int index)
{
    XAUDIO2FX_REVERB_PARAMETERS native;
    ReverbConvertI3DL2ToNative(&g_ReverbPreset[index], &native);

    //Override rear for mono
    native.RearDelay = 5;
    m_submixVoice->SetEffectParameters(0, &native, sizeof(native));
}

//--------------------------------------------------------------------------------------
// Name: AdjustFront()
// Desc: Adjusts the front orientation and angle of the body based on an input value
//--------------------------------------------------------------------------------------
void Sample::AdjustFront(float value, X3DAUDIO_VECTOR *orientFront, float *angle)
{
    float temp = *angle;
    temp += value*c_RotateScale;

    //Keep between 0 and 2 pi
    if (temp >= X3DAUDIO_2PI)
    {
        temp -= X3DAUDIO_2PI;
    }
    else if (temp < 0)
    {
        temp += X3DAUDIO_2PI;
    }

    //Update orientation as a unit vector based on the angle
    orientFront->x = sinf(temp);
    orientFront->y = -cosf(temp);

    CopyMemory(angle, &temp, sizeof(float));
}

//--------------------------------------------------------------------------------------
// Name: DrawGrid()
// Desc: Draws a grid
//--------------------------------------------------------------------------------------
void XM_CALLCONV Sample::DrawGrid(size_t xdivs, size_t ydivs, FXMVECTOR color)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw grid");

    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullNone());

    m_batchEffect->Apply(context);

    context->IASetInputLayout(m_batchInputLayout.Get());

    m_batch->Begin();

    xdivs = std::max<size_t>(1, xdivs);
    ydivs = std::max<size_t>(1, ydivs);
    XMVECTORF32 point1, point2;

    for (size_t i = 0; i <= xdivs; ++i)
    {
        float fPercent = float(i) / float(xdivs);
        fPercent = (fPercent * 2.0f) - 1.0f;
        point1 = { fPercent, -1.f, 0.f };
        point2 = { fPercent, 1.f, 0.f };

        VertexPositionColor v1(point1, color);
        VertexPositionColor v2(point2, color);
        m_batch->DrawLine(v1, v2);
    }

    for (size_t i = 0; i <= ydivs; i++)
    {
        float fPercent = float(i) / float(ydivs);
        fPercent = (fPercent * 2.0f) - 1.0f;
        point1 = { -1.f, fPercent, 0.f };
        point2 = { 1.f, fPercent, 0.f };

        VertexPositionColor v1(point1, color);
        VertexPositionColor v2(point2, color);
        m_batch->DrawLine(v1, v2);
    }

    m_batch->End();

    PIXEndEvent(context);
}

//--------------------------------------------------------------------------------------
// Name: DrawCircle()
// Desc: Draws a circle
//--------------------------------------------------------------------------------------
void Sample::DrawCircle(X3DAUDIO_VECTOR position, float radius)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw emitter");

    XMFLOAT2 pos(position.x, m_deviceResources->GetOutputSize().bottom - position.y);
    
    //Circle has a default radius of 390 (based on the size of the texture)
    //Also account for z(height) in the scale
    float scale = (radius / 390) + (position.z / 200);

    m_spriteBatch->Begin();
    m_spriteBatch->Draw(m_circleTexture.Get(), pos, nullptr, ATG::Colors::White, 0, XMFLOAT2(394, 394), scale);
    m_spriteBatch->End();

    PIXEndEvent(context);
}

//--------------------------------------------------------------------------------------
// Name: DrawEmitter()
// Desc: Draws the emitter triangle and cone
//--------------------------------------------------------------------------------------
void XM_CALLCONV Sample::DrawEmitter(X3DAUDIO_CONE* cone, X3DAUDIO_VECTOR position, float angle, DirectX::FXMVECTOR color, UINT size)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw emitter");

    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullNone());

    m_batchEffect->Apply(context);

    context->IASetInputLayout(m_batchInputLayout.Get());

    //Scale for z (height)
    float localSize = size + (position.z * 2);
    float triangleSize = 15.0f + (position.z * .1f);
    
    float outerX = sinf(cone->OuterAngle / 2) * localSize;
    float innerX = sinf(cone->InnerAngle / 2) * localSize;
    float outerY = -cosf(cone->OuterAngle / 2) * localSize;
    float innerY = -cosf(cone->InnerAngle / 2) * localSize;

    XMVECTOR v[7];
    //Outside cone
    v[0] = XMVectorSet(-outerX, outerY, 0.0f, 1.0f);
    v[1] = XMVectorSet(outerX, outerY, 0.0f, 1.0f);
    //Inside cone
    v[2] = XMVectorSet(-innerX, innerY, 0.0f, 1.0f);
    v[3] = XMVectorSet(innerX, innerY, 0.0f, 1.0f);
    //Emitter Triangle
    v[4] = XMVectorSet(0.f, -triangleSize, 0.0f, 1.0f);
    v[5] = XMVectorSet(-triangleSize, triangleSize, 0.0f, 1.0f);
    v[6] = XMVectorSet(triangleSize, triangleSize, 0.0f, 1.0f);

    XMVECTOR vout[7];
    XMVECTOR vZero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMMATRIX finalTransform = XMMatrixTransformation2D(vZero, 0, XMVectorSet(1, 1, 1.0f, 1.0f), vZero, angle, XMVectorSet(position.x, position.y, 0.0f, 1.0f));

    for(int i=0; i< 7; i++)
    {
        vout[i] = XMVector4Transform(v[i], finalTransform);
    }

    auto rect = m_deviceResources->GetOutputSize();

    //Convert to -1,1 space
    XMVECTORF32 vPosition = { (position.x * 2 / rect.right) - 1, (position.y * 2 / rect.bottom) - 1, 0.f };
    XMFLOAT3 tempv;
    for (int i = 0; i < 7; i++)
    {
        XMStoreFloat3(&tempv, vout[i]);

        tempv.x = (tempv.x * 2 / rect.right) - 1;
        tempv.y = (tempv.y * 2 / rect.bottom) - 1;

        vout[i] = XMLoadFloat3(&tempv);
    }

    m_batch->Begin();

    VertexPositionColor v1(vPosition, color);
    VertexPositionColor v2;

    //Draw cones
    v2 = VertexPositionColor(vout[0], color);
    m_batch->DrawLine(v1, v2);
    v2 = VertexPositionColor(vout[1], color);
    m_batch->DrawLine(v1, v2);
    v2 = VertexPositionColor(vout[2], color);
    m_batch->DrawLine(v1, v2);
    v2 = VertexPositionColor(vout[3], color);
    m_batch->DrawLine(v1, v2);

    //Draw triangle
    v1 = VertexPositionColor(vout[4], color);
    v2 = VertexPositionColor(vout[5], color);
    VertexPositionColor v3 = VertexPositionColor(vout[6], color);
    m_batch->DrawTriangle(v1, v2, v3);
    
    m_batch->End();

    PIXEndEvent(context);
}

//--------------------------------------------------------------------------------------
// Name: DrawListener()
// Desc: Draws the emitter
//--------------------------------------------------------------------------------------
void XM_CALLCONV Sample::DrawListener(X3DAUDIO_VECTOR position, DirectX::FXMVECTOR color)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw listener");

    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullNone());

    m_batchEffect->Apply(context);

    context->IASetInputLayout(m_batchInputLayout.Get());

    XMVECTOR vout[3];

    auto rect = m_deviceResources->GetOutputSize();

    vout[0] = XMVECTORF32{ position.x, position.y - 15.0f, 0.0f, 1.0f };
    vout[1] = XMVECTORF32{ position.x - 15.0f, position.y + 15.0f, 0.0f, 1.0f };
    vout[2] = XMVECTORF32{ position.x + 15.0f, position.y + 15.0f, 0.0f, 1.0f };

    //Convert to -1,1 space
    XMFLOAT3 tempv;
    for (int i = 0; i < 3; i++)
    {
        XMStoreFloat3(&tempv, vout[i]);

        tempv.x = (tempv.x * 2 / rect.right) - 1;
        tempv.y = (tempv.y * 2 / rect.bottom) - 1;

        vout[i] = XMLoadFloat3(&tempv);
    }

    m_batch->Begin();

    VertexPositionColor v1(vout[0], color);
    VertexPositionColor v2(vout[1], color);
    VertexPositionColor v3(vout[2], color);
    m_batch->DrawTriangle(v1, v2, v3);

    m_batch->End();

    PIXEndEvent(context);
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

// Updates the world.
void Sample::Update(DX::StepTimer const& )
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto bounds = m_deviceResources->GetOutputSize();

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    auto pad = m_gamePad->GetState(0);
    m_gamepadPresent = pad.IsConnected();
    if (m_gamepadPresent)
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }

        float height = 0.f;
        if (pad.IsLeftShoulderPressed() && m_X3DEmitter.Position.z + height > -c_MaxHeight)
        {
            height -= 1.0f;
        }

        if (pad.IsRightShoulderPressed() && m_X3DEmitter.Position.z + height < c_MaxHeight)
        {
            height += 1.0f;
        }

        if (pad.IsLeftStickPressed())
        {
            m_X3DEmitter.Position = X3DAUDIO_VECTOR(float(bounds.right / 2), float((bounds.bottom / 2) - 100), 0);
            m_X3DEmitter.Velocity = X3DAUDIO_VECTOR(0, 0, 0);
        }

        if (pad.IsRightStickPressed())
        {
            m_emitterAngle = X3DAUDIO_PI;
        }
        
        float X = -pad.thumbSticks.leftX;
        float Y = -pad.thumbSticks.leftY;
        m_X3DEmitter.Position = X3DAUDIO_VECTOR(m_X3DEmitter.Position.x - X * c_MoveScale, m_X3DEmitter.Position.y - Y * c_MoveScale, m_X3DEmitter.Position.z + height);
        m_X3DEmitter.Velocity = X3DAUDIO_VECTOR(X * c_MoveScale, Y * c_MoveScale, height);

        AdjustFront(-pad.thumbSticks.rightX, &m_X3DEmitter.OrientFront, &m_emitterAngle);
    }
    else
    {
        m_gamePadButtons.Reset();

        //Adjust what direction the emitter is facing
        if (kb.D || kb.A)
        {
            AdjustFront(kb.A ? 1.f : -1.f, &m_X3DEmitter.OrientFront, &m_emitterAngle);
        }

        //Adjust emitter height
        float height = 0.f;
        if (kb.S && (m_X3DEmitter.Position.z + height > -c_MaxHeight))
        {
            height -= 1.0f;
        }

        if (kb.W && (m_X3DEmitter.Position.z + height < c_MaxHeight))
        {
            height += 1.0f;
        }

        if (kb.Home)
        {
            m_X3DEmitter.Position = X3DAUDIO_VECTOR(float(bounds.right / 2), float((bounds.bottom / 2) - 100), 0);
            m_X3DEmitter.Velocity = X3DAUDIO_VECTOR(0, 0, 0);
            m_emitterAngle = X3DAUDIO_PI;
        }
        float X = (kb.Right ? -1.f : 0.f) + (kb.Left ? 1.f : 0.f);
        float Y = (kb.Up ? -1.f : 0.f) + (kb.Down ? 1.f : 0.f);
        m_X3DEmitter.Position = X3DAUDIO_VECTOR(m_X3DEmitter.Position.x - X * c_MoveScale, m_X3DEmitter.Position.y - Y * c_MoveScale, m_X3DEmitter.Position.z + height);
        m_X3DEmitter.Velocity = X3DAUDIO_VECTOR(X * c_MoveScale, Y * c_MoveScale, height);
    }

    if (m_X3DEmitter.Position.x < float(bounds.left))
    {
        m_X3DEmitter.Position.x = float(bounds.left);
        m_X3DEmitter.Velocity.x = 0;
    }
    else if (m_X3DEmitter.Position.x > float(bounds.right))
    {
        m_X3DEmitter.Position.x = float(bounds.right);
        m_X3DEmitter.Velocity.x = 0;
    }

    if (m_X3DEmitter.Position.y < float(bounds.top))
    {
        m_X3DEmitter.Position.y = float(bounds.top);
        m_X3DEmitter.Velocity.y = 0;
    }
    else if (m_X3DEmitter.Position.y > float(bounds.bottom))
    {
        m_X3DEmitter.Position.y = float(bounds.bottom);
        m_X3DEmitter.Velocity.y = 0;
    }

    if (kb.Escape)
    {
        ExitSample();
    }

    //Adjust reverb setting
    if (m_gamePadButtons.dpadUp == GamePad::ButtonStateTracker::PRESSED
        || m_keyboardButtons.IsKeyPressed(Keyboard::E))
    {
        m_reverbIndex++;

        if (m_reverbIndex >= _countof(g_ReverbPreset))
        {
            m_reverbIndex = 0;
        }

        SetReverb(m_reverbIndex);
    }
    else if (m_gamePadButtons.dpadDown == GamePad::ButtonStateTracker::PRESSED
             || m_keyboardButtons.IsKeyPressed(Keyboard::Q))
    {
        m_reverbIndex--;

        if (m_reverbIndex < 0)
        {
            m_reverbIndex = _countof(g_ReverbPreset) - 1;
        }

        SetReverb(m_reverbIndex);
    }

    // Compute positional audio settings
    X3DAudioCalculate(m_X3DInstance, &m_X3DListener, &m_X3DEmitter,
                      X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER | X3DAUDIO_CALCULATE_REVERB,
                      &m_X3DDSPSettings);

    // Update source voice with positional audio settings
    m_sourceVoice->SetOutputMatrix(m_masteringVoice, 1, m_deviceDetails.InputChannels, m_X3DDSPSettings.pMatrixCoefficients);
    m_sourceVoice->SetFrequencyRatio(m_X3DDSPSettings.DopplerFactor);

    m_sourceVoice->SetOutputMatrix(m_submixVoice, 1, 1, &m_X3DDSPSettings.ReverbLevel);

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

    if (m_X3DEmitter.Position.z > 0)
    {
        // Draw the grid
        DrawListener(m_X3DListener.Position, ATG::Colors::White);
        DrawGrid(20, 20, ATG::Colors::Green);

        // Draw emitter
        DrawEmitter(m_X3DEmitter.pCone, m_X3DEmitter.Position, m_emitterAngle, Colors::Black, (UINT)m_X3DEmitter.CurveDistanceScaler);
        DrawCircle(m_X3DEmitter.Position, m_X3DEmitter.CurveDistanceScaler);
    }
    else
    {
        // Draw emitter
        DrawEmitter(m_X3DEmitter.pCone, m_X3DEmitter.Position, m_emitterAngle, Colors::Black, (UINT)m_X3DEmitter.CurveDistanceScaler);
        DrawCircle(m_X3DEmitter.Position, m_X3DEmitter.CurveDistanceScaler);

        // Draw the grid
        DrawListener(m_X3DListener.Position, ATG::Colors::White);
        DrawGrid(20, 20, ATG::Colors::Green);
    }

    auto rect = m_deviceResources->GetOutputSize();
    auto safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(rect.right, rect.bottom);
    std::wstring tempString;

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

    m_spriteBatch->Begin();
    m_font->DrawString(m_spriteBatch.get(), L"SimplePlay3DSound", pos, ATG::Colors::White);
    pos.y += m_font->GetLineSpacing() * 1.1f;

    tempString = L"Listener ( " + std::to_wstring(m_X3DListener.Position.x) + L", " + std::to_wstring(m_X3DListener.Position.y) + L", "
        + std::to_wstring(m_X3DListener.Position.z) + L") Angle: " + std::to_wstring(m_listenerAngle) + L" rad";
    m_font->DrawString(m_spriteBatch.get(), tempString.c_str(), pos, ATG::Colors::White);
    pos.y += m_font->GetLineSpacing() * 1.1f;

    tempString = L"Emitter  ( " + std::to_wstring(m_X3DEmitter.Position.x) + L", " + std::to_wstring(m_X3DEmitter.Position.y) + L", "
        + std::to_wstring(m_X3DEmitter.Position.z) + L") Angle: " + std::to_wstring(m_emitterAngle) + L" rad";
    m_font->DrawString(m_spriteBatch.get(), tempString.c_str(), pos, ATG::Colors::White);
    pos.y += m_font->GetLineSpacing() * 1.1f;

    tempString = L"Reverb: " + std::wstring(g_PresetNames[m_reverbIndex]);
    m_font->DrawString(m_spriteBatch.get(), tempString.c_str(), pos, ATG::Colors::White);

    const wchar_t* legend = m_gamepadPresent
        ? L"[LThumb] Move   [RThumb] Rotate   [LB]/[RB] Adjust height   [DPad] Reverb"
        : L"Arrows Keys: Move   A/D: Rotate   W/S: Adjust height   Q/E: Reverb";

    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), legend,
        XMFLOAT2(float(safeRect.left), float(safeRect.bottom) - m_font->GetLineSpacing()),
        ATG::Colors::LightGrey);

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

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);

    context->OMSetRenderTargets(1, &renderTarget, nullptr);

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
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");

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

    // Load textures
    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, L"circle.dds", nullptr, m_circleTexture.ReleaseAndGetAddressOf())
    );
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    m_spriteBatch->SetRotation(m_deviceResources->GetRotation());

    //Start the listener and emitter in the middle of the screen
    auto rect = m_deviceResources->GetOutputSize();
    m_X3DListener.Position = X3DAUDIO_VECTOR(float(rect.right / 2), float(rect.bottom / 2), 0);
    m_X3DEmitter.Position = X3DAUDIO_VECTOR(float(rect.right / 2), float((rect.bottom / 2) - 100), 0);
}

void Sample::OnDeviceLost()
{
    m_spriteBatch.reset();
    m_ctrlFont.reset();
    m_font.reset();
    m_states.reset();
    m_batch.reset();
    m_batchEffect.reset();
    m_circleTexture.Reset();
    m_batchInputLayout.Reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
