//--------------------------------------------------------------------------------------
// FlightStick.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "FlightStick.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;
using namespace Windows::Foundation;
using namespace Microsoft::Xbox::Input;
using namespace Windows::Xbox::Input;

namespace FlightStickManager
{
    IFlightStick^ GetFirstFlightStick()
    {
        auto allControllers = Controller::Controllers;
        for (unsigned int index = 0; index < allControllers->Size; index++)
        {
            IController^ currentController = allControllers->GetAt(index);

            if (currentController->Type == L"Microsoft.Xbox.Input.FlightStick")
            {
                return dynamic_cast<IFlightStick^>(currentController);
            }
        }

        return nullptr;
    }
};

Sample::Sample() : 
    m_extraButtonCount(0), 
    m_extraAxisCount(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_currentFlightStick = FlightStickManager::GetFirstFlightStick();

    // Call GetProperties() must be successful to get access to the number of 'extra' buttons/axes/leds; behind the 
    // scenes we query for device capabilities which can take some time.
    // Under the covers, device capabilities will always be queried and 'extra' buttons/axes will be added to the
    // report, even without calling GetProperties()
    // GetProperties() will wait up to X millseconds before returning.  If device capabilities have already been fetched,
    // will return immediately.
    // If you don't care about the number of 'extra' buttons/axes/leds, GetProperties() never needs to be called.
    if (m_currentFlightStick != nullptr)
    {
    m_currentFlightStickProperties = nullptr;
    while (m_currentFlightStickProperties == nullptr)
    {
            // Once the properties are retrieved, they will never change (for a particular FlightStick), so only
            // need to be retrieved once.
        m_currentFlightStickProperties = m_currentFlightStick->GetProperties(50);
    }

    m_extraButtonCount = m_currentFlightStickProperties->ExtraButtonCount;
    m_extraAxisCount = m_currentFlightStickProperties->ExtraAxisCount;
    }

    m_currentFlightStickNeedsRefresh = false;
    
    Controller::ControllerAdded += ref new Windows::Foundation::EventHandler<ControllerAddedEventArgs^>([=](Platform::Object^ /*sender*/, ControllerAddedEventArgs^ args)
    {
        if (args->Controller->Type == "Microsoft.Xbox.Input.FlightStick")
        {
            m_currentFlightStickNeedsRefresh = true;
        }
    });

    Controller::ControllerRemoved += ref new Windows::Foundation::EventHandler<ControllerRemovedEventArgs^>([=](Platform::Object^ /*sender*/, ControllerRemovedEventArgs^ args)
    {
        if (args->Controller->Type == "Microsoft.Xbox.Input.FlightStick")
        {
            m_currentFlightStickNeedsRefresh = true;
        }
    });
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame");

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
}

// Updates the world.
void Sample::Update(DX::StepTimer const& )
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    if (m_currentFlightStickNeedsRefresh)
    {
        auto mostRecentFlightStick = FlightStickManager::GetFirstFlightStick();
        if (m_currentFlightStick != mostRecentFlightStick)
        {
            m_currentFlightStick = mostRecentFlightStick;

            if (m_currentFlightStick != nullptr)
            {
            m_currentFlightStickProperties = nullptr;
            while (m_currentFlightStickProperties == nullptr)
            {
                    // Need to retrieve properties again as the FlightStick has changed.
                m_currentFlightStickProperties = m_currentFlightStick->GetProperties(50);
            }

            m_extraButtonCount = m_currentFlightStickProperties->ExtraButtonCount;
            m_extraAxisCount = m_currentFlightStickProperties->ExtraAxisCount;
            }
        }
        m_currentFlightStickNeedsRefresh = false;
    }

    if (m_currentFlightStick == nullptr)
    {
        m_buttonString.clear();
        PIXEndEvent();
        return;
    }
    
    m_reading = m_currentFlightStick->GetRawCurrentReading();

    m_buttonString = L"Buttons pressed:  ";

    if ((m_reading.Buttons & FlightStickButtons::A) == FlightStickButtons::A)
    {
        m_buttonString += L"[A] ";
    }

    if ((m_reading.Buttons & FlightStickButtons::B) == FlightStickButtons::B)
    {
        m_buttonString += L"[B] ";
    }

    if ((m_reading.Buttons & FlightStickButtons::X) == FlightStickButtons::X)
    {
        m_buttonString += L"[X] ";
    }

    if ((m_reading.Buttons & FlightStickButtons::Y) == FlightStickButtons::Y)
    {
        m_buttonString += L"[Y] ";
    }

    if ((m_reading.Buttons & FlightStickButtons::FirePrimary) == FlightStickButtons::FirePrimary)
    {
        m_buttonString += L"Primary ";
    }

    if ((m_reading.Buttons & FlightStickButtons::FireSecondary) == FlightStickButtons::FireSecondary)
    {
        m_buttonString += L"Secondary ";
    }

    if ((m_reading.Buttons & FlightStickButtons::HatUp) == FlightStickButtons::HatUp)
    {
        m_buttonString += L"HatUp ";
    }

    if ((m_reading.Buttons & FlightStickButtons::HatRight) == FlightStickButtons::HatRight)
    {
        m_buttonString += L"HatRight ";
    }

    if ((m_reading.Buttons & FlightStickButtons::HatDown) == FlightStickButtons::HatDown)
    {
        m_buttonString += L"HatDown ";
    }

    if ((m_reading.Buttons & FlightStickButtons::HatLeft) == FlightStickButtons::HatLeft)
    {
        m_buttonString += L"HatLeft ";
    }

    m_stickRoll = m_reading.Roll;
    m_stickPitch = m_reading.Pitch;
    m_stickYaw = m_reading.Yaw;
    m_throttle = m_reading.Throttle;

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
    wchar_t tempString[256] = {};

    m_spriteBatch->Begin();
    m_spriteBatch->Draw(m_background.Get(), rect);

    if (!m_buttonString.empty())
    {
        DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), m_buttonString.c_str(), pos);
        pos.y += m_font->GetLineSpacing() * 1.5f;

        swprintf(tempString, 255, L"Roll:  %d", m_stickRoll);
        m_font->DrawString(m_spriteBatch.get(), tempString, pos, ATG::Colors::White);
        pos.y += m_font->GetLineSpacing() * 1.5f;

        swprintf(tempString, 255, L"Pitch:  %d", m_stickPitch);
        m_font->DrawString(m_spriteBatch.get(), tempString, pos, ATG::Colors::White);
        pos.y += m_font->GetLineSpacing() * 1.5f;

        swprintf(tempString, 255, L"Yaw:  %d", m_stickYaw);
        m_font->DrawString(m_spriteBatch.get(), tempString, pos, ATG::Colors::White);
        pos.y += m_font->GetLineSpacing() * 1.5f;

        swprintf(tempString, 255, L"Throttle:  %u", m_throttle);
        m_font->DrawString(m_spriteBatch.get(), tempString, pos, ATG::Colors::White);
        pos.y += m_font->GetLineSpacing() * 1.5f;
        
        swprintf(tempString, 255, L"Extra Buttons:  %u", m_extraButtonCount);
        m_font->DrawString(m_spriteBatch.get(), tempString, pos, ATG::Colors::White);
        pos.y += m_font->GetLineSpacing() * 1.5f;

        swprintf(tempString, 255, L"Extra Axis:  %u", m_extraAxisCount);
        m_font->DrawString(m_spriteBatch.get(), tempString, pos, ATG::Colors::White);
        pos.y += m_font->GetLineSpacing() * 1.5f;
    }
    else
    {
        m_font->DrawString(m_spriteBatch.get(), L"No flight stick connected", pos, ATG::Colors::Orange);
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
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    m_spriteBatch = std::make_unique<SpriteBatch>(context);

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_24.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneController.spritefont");

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"ATGSampleBackground.DDS", nullptr, m_background.ReleaseAndGetAddressOf()));;
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion
