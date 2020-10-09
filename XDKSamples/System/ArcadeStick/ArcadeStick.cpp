//--------------------------------------------------------------------------------------
// ArcadeStick.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "ArcadeStick.h"

#include "AggregateNavigation.h"
#include "TestController.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample() noexcept;

using namespace std;
using namespace Windows::Xbox::Input;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::Xbox::Input;
using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

namespace
{
    wstring MbcsToWide(const char* mbcs)
    {
        string strMbcs(mbcs);
        return wstring(strMbcs.begin(), strMbcs.end());
    }

    wstring GetVersionString()
    {
        wstring strVersion;
        wchar_t buffer[32] = {};
        auto package = Windows::ApplicationModel::Package::Current;
        auto version = package->Id->Version;

        _itow_s(version.Major, buffer, sizeof(buffer) / sizeof(wchar_t), 10);
        strVersion += buffer;
        strVersion += L'.';
        _itow_s(version.Minor, buffer, sizeof(buffer) / sizeof(wchar_t), 10);
        strVersion += buffer;
        strVersion += L'.';
        _itow_s(version.Build, buffer, sizeof(buffer) / sizeof(wchar_t), 10);
        strVersion += buffer;
        strVersion += L'.';
        _itow_s(version.Revision, buffer, sizeof(buffer) / sizeof(wchar_t), 10);
        strVersion += buffer;

        return strVersion;
    }

    //
    // Colors
    //
    const Vector4 SCREEN_BACKGROUND(0.34f, 0.34f, 0.28f, 0.0f);
    const Vector4 ACTIVE_SECTION_BOX_COLOR(0.5f, 0.5f, 0.5f, 0.0f);
    const Vector4 FOCUS_BOX_COLOR(0.9f, 0.9f, 0.9f, 0.0f);

    const wchar_t* IN_TEST_COLOR_NAME = L"Green";
    const wchar_t* DISCONNECTED_COLOR_NAME = L"Red";

    //
    // Section Locations
    //
    const float CONTROLLER_LIST_START_X = 0.0f;
    const float CONTROLLER_LIST_START_Y = 50.0f;

    const float TEST_NAV_START_X = 500.0f;
    const float TEST_NAV_START_Y = 50.0f;

    const float INFORMATION_START_X = 500.0f;
    const float INFORMATION_START_Y = 500.0f;

    const float TEXT_PADDING_X = 5.0f;
    const float TEXT_PADDING_Y = 5.0f;
    const float GLYPH_PADDING_X = 2.0f;

    template<typename T>std::wstring itow_X(T val)
    {
        size_t digits = sizeof(T) * 2;
        size_t i = 0;
        std::vector<wchar_t> result(digits, '0');
        while (val)
        {
            uint8_t nibble = static_cast<uint8_t>(val & 0x0F);
            if (nibble > 9)
            {
                result[i++] = 'A' + (nibble - 10);
            }
            else
            {
                result[i++] = '0' + nibble;
            }
            val >>= 4;
        }
        return std::wstring(result.rbegin(), result.rend());
    }

    template<typename T>std::wstring itow_T(T val)
    {
        if (val == 0)
        {
            return L"0";
        }

        std::vector<wchar_t> result;
        while (val)
        {
            T nextVal = val / 10;
            T digit = val - (nextVal * 10);

            result.push_back(static_cast<wchar_t>('0' + digit));

            val = nextVal;
        }
        return std::wstring(result.rbegin(), result.rend());
    }

}

Sample::Sample() :
    m_frame(0)
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);
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

    //
    // Setup the controller list
    //
    Controller::ControllerAdded +=
        ref new Windows::Foundation::EventHandler<ControllerAddedEventArgs^>(
            [=](Platform::Object^, ControllerAddedEventArgs^ args) {
        this->OnControllerAdded(args->Controller);
    }
    );

    Controller::ControllerRemoved +=
        ref new Windows::Foundation::EventHandler<ControllerRemovedEventArgs^>(
            [=](Platform::Object^, ControllerRemovedEventArgs^ args) {
        this->OnControllerRemoved(args->Controller);
    }
    );

    //
    // Iterate through the initial group as they do call the add function
    //
    auto controllers = Controller::Controllers;
    for (unsigned int i = 0; i < controllers->Size; ++i)
    {
        OnControllerAdded(controllers->GetAt(i));
    }
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frame %llu", m_frame);

    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();

    PIXEndEvent();
    m_frame++;
}

// Updates the world.
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());
    elapsedTime;

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            ExitSample();
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

    m_spriteBatch->Begin();

    RenderControllerList();
    RenderInformation();
    RenderTestData();

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
    m_gamePadButtons.Reset();
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    m_spriteBatch = std::make_unique<SpriteBatch>(m_deviceResources->GetD3DDeviceContext());

    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_24.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneController.spritefont");
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto vp = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(vp);
}

#pragma endregion

void Sample::OnControllerAdded(IController^ pIController)
{
    if (pIController->Type != "Microsoft.Xbox.Input.ArcadeStick")
    {
        //
        // Only arcade sticks
        //
        return;
    }

    //
    // A controller has been added, check to see if it is in the list already
    //
    controllerLock_.lock();


    auto iController = find(controllers_.begin(), controllers_.end(), pIController);
    if (iController == controllers_.end())
    {
        //
        // Not in list, add
        //
        TestController tc(pIController);
        controllers_.push_back(tc);
    }
    else
    {
        //
        // Was in list, mark as connected
        //
        iController->OnConnect();
    }
    controllerLock_.unlock();
}

void Sample::OnControllerRemoved(IController^ pIController)
{
    //
    // A controller has been added, check to see if it is in the list already
    //
    controllerLock_.lock();

    auto iController = find(controllers_.begin(), controllers_.end(), pIController);
    if (iController != controllers_.end())
    {
        //
        // Was in list, remove
        //
        iController->OnDisconnect();
        controllers_.erase(iController);
    }
    controllerLock_.unlock();
}

void Sample::RenderControllerList()
{
    float xOffset = CONTROLLER_LIST_START_X;
    float yOffset = CONTROLLER_LIST_START_Y;

    //
    // Draw the controller list
    //

    XMFLOAT2 pos = XMFLOAT2(float(xOffset), float(yOffset));

    m_font->DrawString(m_spriteBatch.get(), L"ArcadeStick(s)", pos, ATG::Colors::Orange);
    pos.y += m_font->GetLineSpacing() + TEXT_PADDING_Y;

    controllerLock_.lock();
    if (controllers_.empty())
    {
        pos.x += TEXT_PADDING_X;
        m_font->DrawString(m_spriteBatch.get(), L"No ArcadeSticks Connected", pos, ATG::Colors::Orange);
    }
    else
    {
        for (auto iController : controllers_)
        {
            std::wstring info(L"0x");
            info += itow_X(static_cast<uint16_t>(iController.ArcadeStick()->DeviceVidPid >> 16));
            info += L" 0x";
            info += itow_X(static_cast<uint16_t>(iController.ArcadeStick()->DeviceVidPid & 0x0000FFFF));
            info += L" HW[";
            info += itow_T(iController.ArcadeStick()->HardwareVersionMajor);
            info += L".";
            info += itow_T(iController.ArcadeStick()->HardwareVersionMinor);
            info += L"] PROD[";
            info += itow_T(iController.ArcadeStick()->ProductVersionMajor);
            info += L".";
            info += itow_T(iController.ArcadeStick()->ProductVersionMinor);
            info += L".";
            info += itow_T(iController.ArcadeStick()->ProductVersionBuild);
            info += L".";
            info += itow_T(iController.ArcadeStick()->ProductVersionRevision);
            info += L"]";

            pos.x += TEXT_PADDING_X;
            m_font->DrawString(m_spriteBatch.get(), info.c_str(), pos, ATG::Colors::Orange);
            yOffset += m_font->GetLineSpacing() + TEXT_PADDING_Y;
        }
    }
    controllerLock_.unlock();
}

void Sample::RenderInformation()
{
    CONST float xOffset = INFORMATION_START_X;
    CONST float yOffset = INFORMATION_START_Y;
    
    //
    // Version Information
    //
    XMFLOAT2 pos = XMFLOAT2(float(xOffset), float(yOffset));
    pos.y += m_font->GetLineSpacing() + (TEXT_PADDING_Y * 2);
    pos.x = INFORMATION_START_X;

    m_font->DrawString(m_spriteBatch.get(), L"Version Information", pos, ATG::Colors::Orange);

    pos.x += TEXT_PADDING_X * 3;
    pos.y += m_font->GetLineSpacing() + TEXT_PADDING_Y;

    m_font->DrawString(m_spriteBatch.get(), L"XDK Version", pos, ATG::Colors::Orange);

    XMFLOAT2 indentedPos = XMFLOAT2(float(xOffset) + XMVectorGetX(m_font->MeasureString(L"XDK Version")) + (TEXT_PADDING_X*5.f), pos.y);
    m_font->DrawString(m_spriteBatch.get(), MbcsToWide(_XDK_VER_STRING).c_str(), indentedPos, ATG::Colors::Orange);
    

    pos.y += m_font->GetLineSpacing() + TEXT_PADDING_Y;
    m_font->DrawString(m_spriteBatch.get(), L"Application Version ", pos, ATG::Colors::Orange);

    indentedPos = XMFLOAT2(float(xOffset) + XMVectorGetX(m_font->MeasureString(L"Application Version ")) + (TEXT_PADDING_X*5.f), pos.y);
    m_font->DrawString(m_spriteBatch.get(), GetVersionString().c_str(), indentedPos, ATG::Colors::Orange);

    //
    // DLL Version information
    //
    controllerLock_.lock();
    if (!controllers_.empty())
    {
        auto stick = controllers_[0].ArcadeStick();
        std::wstring dllVersion = L"DLL Version [";
        dllVersion += itow_T(stick->DllVersionMajor);
        dllVersion += L".";
        dllVersion += itow_T(stick->DllVersionMinor);
        dllVersion += L".";
        dllVersion += itow_T(stick->DllVersionBuild);
        if (stick->DllVersionIsDebug)
        {
            dllVersion += L"] Debug";
        }
        else
        {
            dllVersion += L"] Release";
        }
        pos.y += m_font->GetLineSpacing() + TEXT_PADDING_Y;

        m_font->DrawString(m_spriteBatch.get(), dllVersion.c_str(), pos, ATG::Colors::Orange);
    }
    controllerLock_.unlock();

    //
    // Exit information
    //
    pos.x = INFORMATION_START_X;
    pos.y += m_font->GetLineSpacing() + (TEXT_PADDING_Y * 3);

    m_font->DrawString(m_spriteBatch.get(), L"To exit the application, press [MENU]+[VIEW]+A" , pos, ATG::Colors::Orange);
}

void Sample::RenderTestData()
{
    XMFLOAT2 pos = XMFLOAT2(float(TEST_NAV_START_X), float(TEST_NAV_START_Y + (m_font->GetLineSpacing() * 5.f)));
    //
    // Draw the navigation header
    //
    m_font->DrawString(m_spriteBatch.get(), L"Navigation Information", pos, ATG::Colors::Orange);
    pos.y += TEXT_PADDING_Y + m_font->GetLineSpacing();

    pos.x += RenderGlyph(L"[A]", testNavigation_.Accept(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[B]", testNavigation_.Cancel(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[X]", testNavigation_.X(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[Y]", testNavigation_.Y(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[DPad]", testNavigation_.Left(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[DPad]", testNavigation_.Up(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[DPad]", testNavigation_.Right(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[DPad]", testNavigation_.Down(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[MENU]", testNavigation_.Menu(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[VIEW]", testNavigation_.View(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[PREV]", testNavigation_.PreviousPage(), pos.x, pos.y);
    pos.x += RenderGlyph(L"[NEXT]", testNavigation_.NextPage(), pos.x, pos.y);

    //
    // Draw the button information for each device
    //
    pos.x = TEST_NAV_START_X;
    pos.y += (TEXT_PADDING_Y * 3) + m_font->GetLineSpacing();
    m_font->DrawString(m_spriteBatch.get(), L"Button States", pos, ATG::Colors::Orange);
    pos.x += TEXT_PADDING_Y + m_font->GetLineSpacing();

    controllerLock_.lock();
    for (auto iController : controllers_)
    {
        std::wstring indexStr(L"  ");
        IArcadeStickReading^ reading = iController.ArcadeStick()->GetCurrentReading();

        for (uint8_t i = 0; i < 16; ++i)
        {
            if (i < 10)
            {
                indexStr[0] = '0';
                indexStr[1] = i + '0';
            }
            else
            {
                indexStr[0] = '1';
                indexStr[1] = i - 10 + '0';
            }
            RenderGlyph(indexStr.c_str(), reading->IsButtonPressed(i), pos.x, pos.y + TEXT_PADDING_Y + m_font->GetLineSpacing());

            pos.x += RenderGlyph(L" ", reading->IsButtonPressed(i), pos.x, pos.y);
        }
        pos.x = TEST_NAV_START_X;
        pos.y += (TEXT_PADDING_Y + m_font->GetLineSpacing() * 2);
    }
    controllerLock_.unlock();
}

float Sample::RenderGlyph(const wchar_t* glyph, bool active, float x, float y)
{
    auto pos = XMFLOAT2(float(x), float(y));

    DX::DrawControllerString(m_spriteBatch.get(), m_font.get(), m_ctrlFont.get(), glyph, pos, active ? DirectX::Colors::Green : DirectX::Colors::Gray);

    return GLYPH_PADDING_X * 25.f;
}
