//--------------------------------------------------------------------------------------
// SimplePlay3DSpatialSound.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimplePlay3DSpatialSound.h"

#include "ATGColors.h"
#include "ControllerFont.h"
#include "WAVFileReader.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
    const wchar_t* c_WaveFile = L"ATG_SpatialMotion_monoFunkDrums1Loop.wav";
    const float c_RotateScale = 0.1f;
    const float c_MaxHeight = 100;
    const float c_MoveScale = 3.0f;
}

VOID CALLBACK SpatialWorkCallback(_Inout_ PTP_CALLBACK_INSTANCE Instance, _Inout_opt_ PVOID Context, _Inout_ PTP_WORK Work)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    Sample * Sink = (Sample *)Context;
    Work;
    Instance;

    while (Sink->m_threadActive)
    {
		while (Sink->m_renderer->IsActive())
		{
			// Wait for a signal from the audio-engine to start the next processing pass 
			if (Sink->m_renderer->m_bufferCompletionEvent)
			{
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

			//Activate the object if not yet done
			if (Sink->m_emitter.object == nullptr)
			{
				// If this method called more than activeObjectCount times 
				// It will fail with this error HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS) 
				hr = Sink->m_renderer->m_SpatialAudioStream->ActivateSpatialAudioObject(
					AudioObjectType_Dynamic,
					&Sink->m_emitter.object);
				if (FAILED(hr))
				{
					continue;
				}

			}

			//Get the object buffer
			BYTE* buffer = nullptr;
			UINT32 bytecount;
			hr = Sink->m_emitter.object->GetBuffer(&buffer, &bytecount);
			if (FAILED(hr))
			{
				continue;
			}

			Sink->m_emitter.object->SetPosition(Sink->m_emitter.posX - Sink->m_listener.posX,
				Sink->m_emitter.posZ - Sink->m_listener.posZ,
				Sink->m_listener.posY - Sink->m_emitter.posY);
			Sink->m_emitter.object->SetVolume(1.f);

			for (UINT32 i = 0; i < bytecount; i++)
			{
				UINT32 fileLoc = Sink->m_emitter.curBufferLoc;
				buffer[i] = Sink->m_emitter.wavBuffer[fileLoc];
				Sink->m_emitter.curBufferLoc++;
				if (Sink->m_emitter.curBufferLoc == Sink->m_emitter.buffersize)
				{
					Sink->m_emitter.curBufferLoc = 0;
				}
			}

			// Let the audio-engine know that the object data are available for processing now 
			hr = Sink->m_renderer->m_SpatialAudioStream->EndUpdatingAudioObjects();
			if (FAILED(hr))
			{
				//if we have an error, tell the renderer to reset
				Sink->m_renderer->Reset();
				continue;
			}
		}
    }

}

Sample::Sample() :
    m_fileLoaded(false),
    m_threadActive(false),
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

    // Create a new ISAC instance
    m_renderer = Microsoft::WRL::Make<ISACRenderer>();

    // Selects the Default Audio Device
    m_renderer->InitializeAudioDeviceAsync();

    // Load the emitter file
    m_fileLoaded = LoadFile(c_WaveFile);

    if (m_fileLoaded && m_renderer)
    {
        while (!m_renderer->IsActive())
        {
            //Wait for renderer, then start
            Sleep(5);
        }
        m_threadActive = true;
        m_workThread = CreateThreadpoolWork(SpatialWorkCallback, this, nullptr);
        SubmitThreadpoolWork(m_workThread);
    }
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
// Name: DrawEmitter()
// Desc: Draws the emitter triangle and cone
//--------------------------------------------------------------------------------------
void XM_CALLCONV Sample::DrawTriangle(AudioEmitter source, DirectX::FXMVECTOR color)
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Draw emitter");

    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullNone());

    m_batchEffect->Apply(context);

    context->IASetInputLayout(m_batchInputLayout.Get());

    //Scale for z (height)
    float triangleSize = 15.0f + (source.posZ * .1f);
    
    XMVECTOR v[3];
    v[0] = XMVectorSet(0.f, -triangleSize, 0.0f, 1.0f);
    v[1] = XMVectorSet(-triangleSize, triangleSize, 0.0f, 1.0f);
    v[2] = XMVectorSet(triangleSize, triangleSize, 0.0f, 1.0f);

    XMVECTOR vout[3];
    XMVECTOR vZero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMMATRIX finalTransform = XMMatrixTransformation2D(vZero, 0, XMVectorSet(1, 1, 1.0f, 1.0f), vZero, source.angle, XMVectorSet(source.posX, source.posY, 0.0f, 1.0f));

    for(int i = 0; i < 3; i++)
    {
        vout[i] = XMVector4Transform(v[i], finalTransform);
    }

    auto rect = m_deviceResources->GetOutputSize();

    //Convert to -1,1 space
    XMVECTORF32 vPosition = { (source.posX * 2 / rect.right) - 1, (source.posY * 2 / rect.bottom) - 1, 0.f };
    XMFLOAT3 tempv;
    for (int i = 0; i < 3; i++)
    {
        XMStoreFloat3(&tempv, vout[i]);

        tempv.x = (tempv.x * 2 / rect.right) - 1;
        tempv.y = (tempv.y * 2 / rect.bottom) - 1;

        vout[i] = XMLoadFloat3(&tempv);
    }

    m_batch->Begin();

    //Draw triangle
    VertexPositionColor v1 = VertexPositionColor(vout[0], color);
    VertexPositionColor v2 = VertexPositionColor(vout[1], color);
    VertexPositionColor v3 = VertexPositionColor(vout[2], color);
    m_batch->DrawTriangle(v1, v2, v3);
    
    m_batch->End();

    PIXEndEvent(context);
}


bool Sample::LoadFile(LPCWSTR inFile)
{
    std::unique_ptr<uint8_t[]>              m_waveFile;
    DX::WAVData  WavData;

    if (m_emitter.buffersize)
    {
        delete[] m_emitter.wavBuffer;
    }
    m_emitter.buffersize = 0;
    m_emitter.curBufferLoc = 0;

    if (DX::LoadWAVAudioFromFileEx(inFile, m_waveFile, WavData))
    {
        return false;
    }

    if ((WavData.wfx->wFormatTag == 1) && WavData.wfx->nSamplesPerSec == 48000 && WavData.wfx->nChannels == 1)
    {
        int numSamples = WavData.audioBytes / 2;
        m_emitter.wavBuffer = new char[numSamples * 4];
        m_emitter.buffersize = numSamples * 4;

        float * tempnew;
        short * tempdata = (short *)WavData.startAudio;

        for (int i = 0; i < numSamples; i++)
        {
            tempnew = (float *)m_emitter.wavBuffer;
            tempnew[i] = (float)tempdata[i] / 32768;
        }
    }
    else if ((WavData.wfx->wFormatTag == 3) && WavData.wfx->nSamplesPerSec == 48000 && WavData.wfx->nChannels == 1)
    {
        int numSamples = WavData.audioBytes / 4;
        m_emitter.wavBuffer = new char[numSamples * 4];
        m_emitter.buffersize = numSamples * 4;

        float * tempnew;
        float * tempdata = (float *)WavData.startAudio;

        for (int i = 0; i < numSamples; i++)
        {
            tempnew = (float *)m_emitter.wavBuffer;
            tempnew[i] = (float)tempdata[i];
        }
    }
    else
    {
        return false;
    }

    return true;
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

	//Are we resetting the renderer?  This will happen if we get an invalid stream 
	//  which can happen when render mode changes or device changes
	if (m_renderer->IsResetting())
	{
		//clear out renderer
		m_renderer == NULL;

		// Create a new ISAC instance
		m_renderer = Microsoft::WRL::Make<ISACRenderer>();

		// Initialize Default Audio Device
		m_renderer->InitializeAudioDeviceAsync();

		//Reset all the Objects that were being used
		m_emitter.object = nullptr;
	}

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
            Windows::ApplicationModel::Core::CoreApplication::Exit();
        }

        float height = 0.f;
        if (pad.IsLeftShoulderPressed() && m_emitter.posZ + height > -c_MaxHeight)
        {
            height -= 1.0f;
        }

        if (pad.IsRightShoulderPressed() && m_emitter.posZ + height < c_MaxHeight)
        {
            height += 1.0f;
        }

        if (pad.IsLeftStickPressed())
        {
            m_emitter.posX = float(bounds.right / 2);
            m_emitter.posY = float((bounds.bottom / 2) + 100);
            m_emitter.posZ = 0;
            m_emitter.angle = 0;
        }

        m_emitter.posX += pad.thumbSticks.leftX * c_MoveScale;
        m_emitter.posY += pad.thumbSticks.leftY * c_MoveScale;
        m_emitter.posZ += height;
    }
    else
    {
        m_gamePadButtons.Reset();

        if (kb.Escape)
        {
            if (m_threadActive)
            {
                m_threadActive = false;
                WaitForThreadpoolWorkCallbacks(m_workThread, FALSE);
                CloseThreadpoolWork(m_workThread);
            }
            m_renderer->m_SpatialAudioStream->Stop();

            Windows::ApplicationModel::Core::CoreApplication::Exit();
        }

        //Adjust emitter height
        float height = 0.f;
        if (kb.S && (m_emitter.posZ + height > -c_MaxHeight))
        {
            height -= 1.0f;
        }

        if (kb.W && (m_emitter.posZ + height < c_MaxHeight))
        {
            height += 1.0f;
        }

        if (kb.Home)
        {
            m_emitter.posX = float(bounds.right / 2);
            m_emitter.posY = float((bounds.bottom / 2) + 100);
            m_emitter.posZ = 0;
            m_emitter.angle = 0;
        }
        
        float X = (kb.Right ? -1.f : 0.f) + (kb.Left ? 1.f : 0.f);
        float Y = (kb.Up ? -1.f : 0.f) + (kb.Down ? 1.f : 0.f);
        
        m_emitter.posX -= X * c_MoveScale;
        m_emitter.posY -= Y * c_MoveScale;
        m_emitter.posZ += height;

    }

    if (m_emitter.posX < float(bounds.left))
    {
        m_emitter.posX = float(bounds.left);
    }
    else if (m_emitter.posX > float(bounds.right))
    {
        m_emitter.posX = float(bounds.right);
    }

    if (m_emitter.posY < float(bounds.top))
    {
        m_emitter.posY = float(bounds.top);
    }
    else if (m_emitter.posY > float(bounds.bottom))
    {
        m_emitter.posY = float(bounds.bottom);
    }

    if (kb.Escape)
    {
        Windows::ApplicationModel::Core::CoreApplication::Exit();
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

    m_emitter.angle += .1;
    if (m_emitter.angle >= X3DAUDIO_2PI)
    {
        m_emitter.angle = 0;
    }

    if (m_emitter.posZ >= 0)
    {
        DrawGrid(20, 20, Colors::Green);
        DrawTriangle(m_listener, Colors::White);
        DrawTriangle(m_emitter, Colors::Black);
    }
    else
    {
        DrawTriangle(m_emitter, Colors::Black);
        DrawGrid(20, 20, Colors::Green);
        DrawTriangle(m_listener, Colors::White);
    }

    auto rect = m_deviceResources->GetOutputSize();
    auto safeRect = SimpleMath::Viewport::ComputeTitleSafeArea(rect.right, rect.bottom);
    std::wstring tempString;

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

    m_spriteBatch->Begin();
    m_font->DrawString(m_spriteBatch.get(), L"SimplePlay3DSpatialSound", pos, ATG::Colors::White);
    pos.y += m_font->GetLineSpacing() * 1.1f;

    tempString = L"Emitter  ( " + std::to_wstring(m_emitter.posX - m_listener.posX) + L", " + std::to_wstring(m_emitter.posY - m_listener.posY) + L", "
        + std::to_wstring(m_emitter.posZ - m_listener.posZ) + L")";
    m_font->DrawString(m_spriteBatch.get(), tempString.c_str(), pos, ATG::Colors::White);
    pos.y += m_font->GetLineSpacing() * 1.1f;

    const wchar_t* legend = m_gamepadPresent
        ? L"[LThumb] Move   [LB]/[RB] Adjust height"
        : L"Arrows Keys: Move   W/S: Adjust height";

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
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    m_spriteBatch->SetRotation(m_deviceResources->GetRotation());

    //Start the listener and emitter in the middle of the screen
    auto rect = m_deviceResources->GetOutputSize();
    m_listener.posX = float(rect.right / 2);
    m_listener.posY = float(rect.bottom / 2);
    m_listener.posZ = 0;
    m_listener.angle = X3DAUDIO_PI;

    m_emitter.posX = float(rect.right / 2);
    m_emitter.posY = float((rect.bottom / 2) + 100);
    m_emitter.posZ = 0;
    m_emitter.angle = 0;
}

void Sample::OnDeviceLost()
{
    m_spriteBatch.reset();
    m_ctrlFont.reset();
    m_font.reset();
    m_states.reset();
    m_batch.reset();
    m_batchEffect.reset();
    m_batchInputLayout.Reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
