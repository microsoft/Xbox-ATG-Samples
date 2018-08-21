//--------------------------------------------------------------------------------------
// SimpleWASAPICaptureUWP.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SimpleWASAPICaptureUWP.h"

#include "ATGColors.h"
#include "ControllerFont.h"

using namespace DirectX;

extern void ExitSample();

using Microsoft::WRL::ComPtr;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Media::Devices;
using namespace Windows::Foundation;

Sample::Sample() :
    m_ctrlConnected(false),
    m_renderFormat(nullptr),
    m_readInput(true),
    m_finishInit(true),
    m_isRendererSet(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
	
	m_renderEventToken = MediaDevice::DefaultAudioRenderDeviceChanged += 
		ref new TypedEventHandler<Platform::Object^, DefaultAudioRenderDeviceChangedEventArgs^>([this](Platform::Object^ sender, Platform::Object^ args)
		{
			m_renderInterface = nullptr;
			m_renderInterface = Microsoft::WRL::Make<WASAPIRenderer>();
			m_renderInterface->InitializeAudioDeviceAsync();
		}, Platform::CallbackContext::Same);
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

    // Initialize MF
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    DX::ThrowIfFailed(hr);
    
    // Set up device watchers for audio capture devices
    m_captureWatcher = DeviceInformation::CreateWatcher(Windows::Media::Devices::MediaDevice::GetAudioCaptureSelector());
    
    m_captureWatcher->Added += ref new TypedEventHandler<DeviceWatcher ^, DeviceInformation ^>([=](DeviceWatcher ^source, DeviceInformation ^device)
    {
        //Just add this to our local device list
        m_captureDevices.emplace_back(device);
    });

    m_captureWatcher->Removed += ref new TypedEventHandler<DeviceWatcher ^, DeviceInformationUpdate ^>([=](DeviceWatcher ^source, DeviceInformationUpdate ^device)
    {
        bool deviceChanged = false;

        //Find and delete the device from our local list
        if (m_captureDevices.size() == 1)
        {
            //This is our last device
            deviceChanged = true;
            m_currentId = nullptr;
            m_captureDevices.clear();
        }

        for (std::vector<DeviceInformation^>::iterator it = m_captureDevices.begin(); it != m_captureDevices.end(); it++)
        {
            if (device->Id == (*it)->Id)
            {
                if (m_currentId == device->Id)
                {
                    //We lost the device we were capturing from, so reset
                    m_currentId = m_captureDevices[0]->Id;
                    deviceChanged = true;
                }

                m_captureDevices.erase(it);
            }
        }

        if (deviceChanged)
        {
            if (m_captureInterface->GetDeviceStateEvent()->GetState() == DeviceState::DeviceStateCapturing)
            {
                //Stop
                m_captureInterface->StopCaptureAsync();
            }

            if (m_currentId != nullptr)
            {
                m_captureInterface->InitializeAudioDeviceAsync(m_currentId);
                m_finishInit = true;
            }
        }
    });

    m_captureWatcher->Start();
    m_finishInit = true;
    m_captureBuffer.reset(new CBuffer(32768));

    //Start default capture device
    m_currentId = MediaDevice::GetDefaultAudioCaptureId(AudioDeviceRole::Default);
    m_captureInterface = Microsoft::WRL::Make<WASAPICapture>();
    m_captureInterface->InitializeAudioDeviceAsync(m_currentId);

    //Start default render device
    m_renderInterface = Microsoft::WRL::Make<WASAPIRenderer>();
    m_renderInterface->InitializeAudioDeviceAsync();
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
void Sample::Update(DX::StepTimer const&)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_ctrlConnected = true;

        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
        }
    }
    else
    {
        m_ctrlConnected = false;

        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitSample();
    }

    if (m_finishInit)
    {
        if (m_currentId == nullptr)
        {
            if (m_captureWatcher->Status == DeviceWatcherStatus::EnumerationCompleted)
            {
                m_currentId = MediaDevice::GetDefaultAudioCaptureId(AudioDeviceRole::Default);

                m_captureInterface = Microsoft::WRL::Make<WASAPICapture>();
                m_captureInterface->InitializeAudioDeviceAsync(m_currentId);
            }
        }
        else
        {
            if (m_renderInterface->GetDeviceStateEvent()->GetState() == DeviceState::DeviceStateInitialized)
            {
                m_renderFormat = m_renderInterface->GetMixFormat();
                m_renderInterface->StartPlaybackAsync(m_captureBuffer.get());
                m_isRendererSet = true;
            }

            if (m_isRendererSet && m_captureInterface->GetDeviceStateEvent()->GetState() == DeviceState::DeviceStateInitialized)
            {
                m_captureBuffer->SetSourceFormat(m_captureInterface->GetMixFormat());
                m_captureBuffer->SetRenderFormat(m_renderFormat);
                m_captureInterface->StartCaptureAsync(m_captureBuffer.get());

                m_finishInit = false;
            }
        }
    }

    int newDeviceIndex = -1;

    // Account for UI input
    if (m_readInput && m_captureDevices.size() > 1)
    {
        if (pad.IsDPadUpPressed() || kb.Up)
        {
            m_readInput = false;

            for (uint32_t i = 0; i < m_captureDevices.size(); i++)
            {
                if (m_captureDevices[i]->Id == m_currentId)
                {
                    if (i == 0)
                    {
                        newDeviceIndex = (int)m_captureDevices.size() - 1;
                    }
                    else
                    {
                        newDeviceIndex = i-1;
                    }
                    break;
                }
            }
        }
        else if (pad.IsDPadDownPressed() || kb.Down)
        {
            m_readInput = false;

            for (uint32_t i = 0; i < m_captureDevices.size(); i++)
            {
                if (m_captureDevices[i]->Id == m_currentId)
                {
                    if (i == m_captureDevices.size() - 1)
                    {
                        newDeviceIndex = 0;
                    }
                    else
                    {
                        newDeviceIndex = i+1;
                    }
                    break;
                }
            }
        }

        if(newDeviceIndex > -1)
        {
            //A new device has been selected, so stop the old one and init the new one
            if (m_captureInterface->GetDeviceStateEvent()->GetState() == DeviceState::DeviceStateCapturing)
            {
                //Stop
                m_captureInterface->StopCaptureAsync();
            }
            
            m_currentId = m_captureDevices[newDeviceIndex]->Id;
            m_captureInterface->InitializeAudioDeviceAsync(m_currentId);
            m_finishInit = true;
        }
    }
    else
    {
        if (!pad.IsDPadUpPressed() && !kb.Up && !pad.IsDPadDownPressed() && !kb.Down)
        {
            m_readInput = true;
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

    m_font->DrawString(m_spriteBatch.get(), L"Audio captured from the selected mic is looped to the default output", pos, ATG::Colors::OffWhite);
    pos.y += spacing;
    m_font->DrawString(m_spriteBatch.get(), L"Note that no sample conversion is done!", pos, ATG::Colors::OffWhite);
    pos.y += spacing;

    // Draw the sample rates
    wchar_t rateString[32] = {};
	if (m_renderFormat != nullptr)
	{
		swprintf_s(rateString, L"Render rate: %dHz", m_renderFormat->nSamplesPerSec);
	}
	else
	{
		swprintf_s(rateString, L"Render rate: -----Hz");
	}
    m_font->DrawString(m_spriteBatch.get(), rateString, pos, ATG::Colors::Orange);
    pos.y += spacing;

    if (m_captureDevices.empty())
    {
        swprintf_s(rateString, L"Capture rate: N/A");
    }
    else
    {
        swprintf_s(rateString, L"Capture rate: %dHz", m_captureInterface->GetMixFormat()->nSamplesPerSec);
    }
    
    m_font->DrawString(m_spriteBatch.get(), rateString, pos, ATG::Colors::Orange);
    pos.y += spacing * 1.5f;

    m_font->DrawString(m_spriteBatch.get(), L"Select your capture device:", pos, ATG::Colors::OffWhite);
    pos.y += spacing;

    if (m_captureDevices.empty())
    {
        m_font->DrawString(m_spriteBatch.get(), L"No capture devices!", pos, ATG::Colors::Orange);
        pos.y += spacing;
    }
    else
    {
        // Draw list of capture devices
        for (std::vector<DeviceInformation^>::iterator it = m_captureDevices.begin(); it != m_captureDevices.end(); it++)
        {
            if ((*it)->Id == m_currentId)
            {
                pos.x = float(safeRect.left);
                m_font->DrawString(m_spriteBatch.get(), L"> ", pos, ATG::Colors::Green);
            }

            pos.x = float(safeRect.left + 36);
            m_font->DrawString(m_spriteBatch.get(), (*it)->Name->Data(), pos, ATG::Colors::Green);
            pos.y += spacing * 1.1f;
        }
    }

    pos.y += spacing * .5f;

    const wchar_t* str = m_ctrlConnected
        ? L"Press [DPad] Up/Down to change capture device"
        : L"Press Up/Down to change capture device";

    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), str, pos, ATG::Colors::OffWhite);

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
    if (m_captureInterface->GetDeviceStateEvent()->GetState() == DeviceState::DeviceStateCapturing)
    {
        //Stop
        m_captureInterface->StopCaptureAsync();
    }
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
    m_font->SetDefaultCharacter(L' ');

    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerSmall.spritefont");
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
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
