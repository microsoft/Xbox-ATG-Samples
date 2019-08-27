//--------------------------------------------------------------------------------------
// NLSAndLocalization.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "NLSAndLocalization.h"

#include "ATGColors.h"

extern void ExitSample();

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_frame(0)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_textConsole = std::make_unique<DX::TextConsoleImage>();
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_gamePad = std::make_unique<GamePad>();

    m_deviceResources->SetWindow(window);

    int retVal = GetUserDefaultLocaleName(m_localeName, ARRAYSIZE(m_localeName));
    if (retVal == 0)
        throw std::exception("Failed to get locale in CreateDeviceDependenciesResources");
    swprintf_s(m_imagePrepend, _countof(m_imagePrepend), L"Assets\\Images\\%ls\\", m_localeName);
    swprintf_s(m_resFileName, _countof(m_resFileName), L"Assets\\Resources\\%ls.resources", m_localeName);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    InitializeLocalization();
}

void Sample::InitializeLocalization()
{

    m_textConsole->Format(L"******* Using the NLS APIs *******\n");
    m_textConsole->Format(L"Note: All the strings except the ones with\nthe IDs in the Localization section\nare hard coded in English (not localized)\n\n");

    // Get Package Details using the Package Id
    Windows::ApplicationModel::Package^ package = Windows::ApplicationModel::Package::Current;
    Windows::ApplicationModel::PackageId^ packageId = package->Id;
    m_textConsole->Format(L"The Package Full Name is: %ls\n", packageId->FullName->Data());

    // Get the locale name for the system using GetUserDefaultLocaleName(). This will return the locale selected through the 
    // Settings app only if that locale has been added to the Resources section of the application's package manifest.
    // In case the resource is absent fron the manifest, this API will return the first locale in the Resource Language.
    wchar_t userLocaleName[LOCALE_NAME_MAX_LENGTH] = { L'\0' };
    wchar_t message[1024] = { L'\0' };
    int retVal = GetUserDefaultLocaleName(userLocaleName, ARRAYSIZE(userLocaleName));
    if (FunctionSucceeded(retVal, L"GetUserDefaultLocaleName"))
    {
        swprintf_s(message, L"GetUserDefaultLocaleName succeeded: %ls\n", userLocaleName);
        OutputDebugString(message);
        m_textConsole->Write(message);
    }
    else
    {
        throw std::exception("GetUserDefaultLocaleName failed!");
    }

    unsigned long lcid = LocaleNameToLCID(userLocaleName, 0);
    unsigned short primary = 0, sublang = 0;
    if (FunctionSucceeded(lcid, L"LocalNameToLCID"))
    {
        primary = PRIMARYLANGID(lcid);
        sublang = SUBLANGID(lcid);
        swprintf_s(message, L"LocaleNameToLCID succeeded: %u\n\tPrimary ID: %u\n\tSublanguage: %u\n", lcid, primary, sublang);
        OutputDebugString(message);
        m_textConsole->Write(message);
    }
    else
    {
        throw std::exception("LocaleNameToLCID failed!");
    }

    // The GetUserGeoID() API can be used to get the actual country/region the kit is in. 
    // It gives you the country/region selected through the Settings app
    long geoID = GetUserGeoID(GEOCLASS_NATION);
    wchar_t geoData[LOCALE_NAME_MAX_LENGTH];
    retVal = GetGeoInfoW(geoID, GEO_LATITUDE, geoData, ARRAYSIZE(geoData), primary);
    if (FunctionSucceeded(retVal, L"GetGeoInfoW"))
    {
        swprintf_s(message, L"Lattitude query succeeded: %ls\n", geoData);
        OutputDebugString(message);
        m_textConsole->Write(message);
    }
    else
    {
        throw std::exception("GetGeoInfoW failed in GEO_LATTITUDE!");
    }

    retVal = GetGeoInfoW(geoID, GEO_LONGITUDE, geoData, ARRAYSIZE(geoData), primary);
    if (FunctionSucceeded(retVal, L"GetGeoInfoW"))
    {
        swprintf_s(message, L"Longitude query succeeded: %ls\n", geoData);
        OutputDebugString(message);
        m_textConsole->Write(message);
    }
    else
    {
        throw std::exception("GetGeoInfoW failed on GEO_LONGITUDE!");
    }

    retVal = GetGeoInfoW(geoID, GEO_NATION, geoData, ARRAYSIZE(geoData), primary);
    if (FunctionSucceeded(retVal, L"GetGeoInfoW"))
    {
        swprintf_s(message, L"Nation query succeeded: %ls\n", geoData);
        OutputDebugString(message);
        m_textConsole->Write(message);
    }
    else
    {
        throw std::exception("GetGeoInfoW failed on GEO_NATION!");
    }

    wchar_t iso2[3];
    retVal = GetGeoInfoW(geoID, GEO_ISO2, iso2, ARRAYSIZE(iso2), primary);
    if (FunctionSucceeded(retVal, L"GetGeoInfoW"))
    {
        swprintf_s(message, L"Iso2 query succeeded: %ls\n", iso2);
        OutputDebugString(message);
        m_textConsole->Write(message);
    }
    else
    {
        throw std::exception("GetGeoInfoW failed on GEO_ISO2!");
    }

    wchar_t iso3[4];
    retVal = GetGeoInfoW(geoID, GEO_ISO3, iso3, ARRAYSIZE(iso3), primary);
    if (FunctionSucceeded(retVal, L"GetGeoInfoW"))
    {
        swprintf_s(message, L"Iso3 query succeeded: %ls\n", iso3);
        OutputDebugString(message);
        m_textConsole->Write(message);
    }
    else
    {
        throw std::exception("GetGeoInfoW failed on GEO_ISO3!");
    }

    // The country/region values returned from GetUserDefaultLocaleName() and GetUserGeoID() can be compared
    // to determine if the country/region selected by the user is supported by the app or not
    std::wstring localeNameStr(userLocaleName);
    std::wstring::size_type pos = localeNameStr.find(L'-');
    localeNameStr = localeNameStr.substr(pos + 1);
    if (_wcsicmp(localeNameStr.c_str(), iso2) == 0 || _wcsicmp(localeNameStr.c_str(), iso3) == 0)
    {
        m_textConsole->Format(L"Selected locale in manifest. Country/region: %ls\n", iso2);
    }
    else
    {
        m_textConsole->Format(L"The selected locale (Country/region: %ls) is NOT present in the manifest, so the fallback locale (Country: %s) is selected for localization\n", iso2, localeNameStr.c_str());
    }

    wchar_t lcpData[LOCALE_NAME_MAX_LENGTH];
    retVal = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO639LANGNAME, lcpData, ARRAYSIZE(lcpData));
    if (FunctionSucceeded(retVal, L"GetLocaleInfoEx"))
    {
        m_textConsole->Format(L"GetLocaleInfoEx() - LOCALE_SISO639LANGNAME: %ls\n", lcpData);
    }
    else
    {
        throw std::exception("GetLocaleInfoEx failed on LOCALE_SISO639LANGNAME!");
    }

    retVal = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_IDEFAULTLANGUAGE, lcpData, ARRAYSIZE(lcpData));
    if (FunctionSucceeded(retVal, L"GetLocaleInfoEx"))
    {
        m_textConsole->Format(L"GetLocaleInfoEx() - LOCALE_IDEFAULTLANGUAGE: %ls\n", lcpData);
    }
    else
    {
        throw std::exception("GetLocaleInfoEx failed on LOCALE_IDEFAULTLANGUAGE!");
    }
}

bool Sample::FunctionSucceeded(int returned, const wchar_t* function)
{
    if (returned == 0)
    {
        unsigned long error = GetLastError();
        wchar_t message[1024] = { L'\0' };
        swprintf_s(message, L"%ls failed with error: %u\n", function, error);
        OutputDebugString(message);
        return false;
    }
    return true;
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

    m_textConsole->Render();

    m_sprites->Begin();
    m_sprites->Draw(m_texture.Get(), XMFLOAT2(800, 75), nullptr, Colors::White, 0.f, XMFLOAT2(0, 0), 0.2f);
    m_sprites->End();

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
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    // setup the text console for output
    m_textConsole->RestoreDevice(context, L"SegoeUI_18.spritefont");

    // parse the resource file to find the right localized image to display
    ResourceParser resParser(m_localeName, m_imagePrepend);
    DX::ThrowIfFailed(resParser.ParseFile(m_resFileName));
    Platform::String^ idImage = ref new Platform::String(L"Gamepad");
    Platform::String^ imageString = resParser.GetImage(idImage);

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, imageString->Data(), nullptr, m_texture.ReleaseAndGetAddressOf()));

    m_sprites = std::make_unique<SpriteBatch>(context);

    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto rect = m_deviceResources->GetOutputSize();
    m_textConsole->SetWindow(rect);
}
#pragma endregion
