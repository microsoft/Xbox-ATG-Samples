//--------------------------------------------------------------------------------------
// SystemInfo.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SystemInfo.h"

#include "ATGColors.h"
#include "ControllerFont.h"

extern void ExitSample();

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

#include <libloaderapi2.h>
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    inline float XM_CALLCONV DrawStringCenter(SpriteBatch* batch, SpriteFont* font, const wchar_t* text, float mid, float y, FXMVECTOR color, float scale)
    {
        XMVECTOR size = font->MeasureString(text);
        XMFLOAT2 pos(mid - XMVectorGetX(size)*scale / 2.f, y);
        font->DrawString(batch, text, pos, color, 0.f, Vector2::Zero, scale);
        return font->GetLineSpacing() * scale;
    }

    inline void DrawStringLeft(SpriteBatch* batch, SpriteFont* font, const wchar_t* text, float mid, float y, float scale)
    {
        XMVECTOR size = font->MeasureString(text);
        XMFLOAT2 pos(mid - XMVectorGetX(size)*scale, y);
        font->DrawString(batch, text, pos, ATG::Colors::Blue, 0.f, Vector2::Zero, scale);
    }

    inline float DrawStringRight(SpriteBatch* batch, SpriteFont* font, const wchar_t* text, float mid, float y, float scale)
    {
        XMFLOAT2 pos(mid, y);
        font->DrawString(batch, text, pos, ATG::Colors::White, 0.f, Vector2::Zero, scale);
        return font->GetLineSpacing()*scale;
    }
}

Sample::Sample() :
    m_scale(1.f),
    m_current(0),
    m_gamepadPresent(false)
{
    // Renders only 2D, so no need for a depth buffer.
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
    m_gamepadPresent = pad.IsConnected();
    if (m_gamepadPresent)
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

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        ExitSample();
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Right)
        || m_gamePadButtons.a == GamePad::ButtonStateTracker::PRESSED
        || m_gamePadButtons.dpadRight == GamePad::ButtonStateTracker::PRESSED)
    {
        ++m_current;
        if (m_current >= int(InfoPage::MAX))
            m_current = 0;
    }

    if (m_keyboardButtons.IsKeyPressed(Keyboard::Left)
        || m_gamePadButtons.b == GamePad::ButtonStateTracker::PRESSED
        || m_gamePadButtons.dpadLeft == GamePad::ButtonStateTracker::PRESSED)
    {
        --m_current;
        if (m_current < 0)
            m_current = int(InfoPage::MAX) - 1;
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
    
    auto fullscreen = m_deviceResources->GetOutputSize();

    auto safeRect = Viewport::ComputeTitleSafeArea(fullscreen.right - fullscreen.left, fullscreen.bottom - fullscreen.top);

    float mid = float(safeRect.left) + float(safeRect.right - safeRect.left) / 2.f;

    m_batch->Begin();
    m_batch->Draw(m_background.Get(), fullscreen);

    float y = float(safeRect.top);

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.bottom) - m_smallFont->GetLineSpacing());
    if (m_gamepadPresent)
    {
        DX::DrawControllerString(m_batch.get(), m_smallFont.get(), m_ctrlFont.get(), L"Use [A], [B], or [DPad] to cycle pages", pos, ATG::Colors::LightGrey, m_scale);
    }
    else
    {
        m_smallFont->DrawString(m_batch.get(), L"Use Left/Right to cycle pages", pos, ATG::Colors::LightGrey, 0, Vector2::Zero, m_scale);
    }

    float spacer = XMVectorGetX(m_smallFont->MeasureString(L"X")*m_scale);

    float left = mid - spacer;
    float right = mid + spacer;

    switch (static_cast<InfoPage>(m_current))
    {
    case InfoPage::SYSTEMINFO:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"GetNativeSystemInfo", mid, y, ATG::Colors::LightGrey, m_scale);

            SYSTEM_INFO info = {};
            GetNativeSystemInfo(&info);

            const wchar_t* arch = L"UNKNOWN";
            switch (info.wProcessorArchitecture)
            {
            case PROCESSOR_ARCHITECTURE_AMD64:  arch = L"AMD64"; break;
            case PROCESSOR_ARCHITECTURE_ARM:    arch = L"ARM"; break;
            case PROCESSOR_ARCHITECTURE_INTEL:  arch = L"INTEL"; break;
            }

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"wProcessorArchitecture", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), arch, right, y, m_scale);

            wchar_t buff[128] = {};
            swprintf_s(buff, L"%u", info.wProcessorLevel);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"wProcessorLevel", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            swprintf_s(buff, L"%04X", info.wProcessorRevision);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"wProcessorRevision", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            swprintf_s(buff, L"%zX", info.dwActiveProcessorMask);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"dwActiveProcessorMask", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            swprintf_s(buff, L"%u", info.dwNumberOfProcessors);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"dwNumberOfProcessors", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            swprintf_s(buff, L"%u", info.dwPageSize);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"dwPageSize", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            swprintf_s(buff, L"%u", info.dwAllocationGranularity);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"dwAllocationGranularity", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            swprintf_s(buff, L"%p", info.lpMinimumApplicationAddress);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"lpMinimumApplicationAddress", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            swprintf_s(buff, L"%p", info.lpMaximumApplicationAddress);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"lpMaximumApplicationAddress", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
        }
        break;

    case InfoPage::GETPROCESSINFO:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"GetProcessInformation", mid, y, ATG::Colors::LightGrey, m_scale);

            APP_MEMORY_INFORMATION info = {};
            if (GetProcessInformation(GetCurrentProcess(), ProcessAppMemoryInfo, &info, sizeof(info)))
            {
                auto ac = static_cast<uint32_t>(info.AvailableCommit / (1024 * 1024));
                auto pc = static_cast<uint32_t>(info.PrivateCommitUsage / (1024 * 1024));
                auto ppc = static_cast<uint32_t>(info.PeakPrivateCommitUsage / (1024 * 1024));
                auto tc = static_cast<uint32_t>(info.TotalCommitUsage / (1024 * 1024));

                wchar_t buff[128] = {};
                swprintf_s(buff, L"%u (MiB)", ac);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"AvailableCommit", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u (MiB)", pc);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"PrivateCommitUsage", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u (MiB)", ppc);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"PeakPrivateCommitUsage", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u (MiB)", tc);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"TotalCommitUsage", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
            }
        }
        break;

    case InfoPage::GLOBALMEMORYSTATUS:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"GlobalMemoryStatusEx", mid, y, ATG::Colors::LightGrey, m_scale);

            MEMORYSTATUSEX info = {};
            info.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&info))
            {
                auto tphys = static_cast<uint32_t>(info.ullTotalPhys / (1024 * 1024));
                auto aphys = static_cast<uint32_t>(info.ullAvailPhys / (1024 * 1024));
                auto tpage = static_cast<uint32_t>(info.ullTotalPageFile / (1024 * 1024));
                auto apage = static_cast<uint32_t>(info.ullAvailPageFile / (1024 * 1024));
                auto tvirt = static_cast<uint32_t>(info.ullTotalVirtual / (1024 * 1024));
                auto avirt = static_cast<uint32_t>(info.ullAvailVirtual / (1024 * 1024));

                wchar_t buff[128] = {};
                swprintf_s(buff, L"%u / %u (MB)", aphys, tphys);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Physical Memory", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u / %u (MB)", apage, tpage);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Page File", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u (MB)", tvirt);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Total Virtual Memory", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u (MB)", avirt);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Available VM", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                if (info.ullAvailExtendedVirtual > 0)
                {
                    auto axvirt = static_cast<uint32_t>(info.ullAvailExtendedVirtual / (1024 * 1024));

                    swprintf_s(buff, L"%u (MB)", axvirt);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Available Extended VM", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
                }
            }
        }
        break;

    case InfoPage::ANALYTICSINFO:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"AnalyticsInfo", mid, y, ATG::Colors::LightGrey, m_scale);

            auto deviceForm = Windows::System::Profile::AnalyticsInfo::DeviceForm;

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DeviceForm", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), deviceForm->Data(), right, y, m_scale);

            auto versionInfo = Windows::System::Profile::AnalyticsInfo::VersionInfo;

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DeviceFamily", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), versionInfo->DeviceFamily->Data(), right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DeviceFamilyVersion", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), versionInfo->DeviceFamilyVersion->Data(), right, y, m_scale);

            // For real-world use just log it as an opaque string, and do the decode in the reader instead
            LARGE_INTEGER li;
            li.QuadPart = _wtoi64(versionInfo->DeviceFamilyVersion->Data());

            wchar_t buff[16] = {};
            swprintf_s(buff, L"%u.%u.%u.%u", HIWORD(li.HighPart), LOWORD(li.HighPart), HIWORD(li.LowPart), LOWORD(li.LowPart));
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
        }
        break;

    case InfoPage::EASCLIENTINFO:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"EasClientDeviceInformation", mid, y, ATG::Colors::LightGrey, m_scale);

            using namespace Windows::Security::ExchangeActiveSyncProvisioning;

            auto easinfo = ref new EasClientDeviceInformation;

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Id", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), easinfo->Id.ToString()->Data(), right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"FriendlyName", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), easinfo->FriendlyName->Data(), right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"OperatingSystem", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), easinfo->OperatingSystem->Data(), right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"SystemManufacturer", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), easinfo->SystemManufacturer->Data(), right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"SystemProductName", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), easinfo->SystemProductName->Data(), right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"SystemSku", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), easinfo->SystemSku->Data(), right, y, m_scale);

            if (!easinfo->SystemHardwareVersion->IsEmpty())
            {
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"SystemHardwareVersion", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), easinfo->SystemHardwareVersion->Data(), right, y, m_scale);
            }

            if (!easinfo->SystemFirmwareVersion->IsEmpty())
            {
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"SystemFirmwareVersion", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), easinfo->SystemFirmwareVersion->Data(), right, y, m_scale);
            }
        }
        break;

    case InfoPage::GAMINGDEVICEINFO:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"GetGamingDeviceModelInformation", mid, y, ATG::Colors::LightGrey, m_scale);

            #if defined(NTDDI_WIN10_RS3) && (NTDDI_VERSION >= NTDDI_WIN10_RS3)

            // Requires the linker settings to include /DELAYLOAD:api-ms-win-gaming-deviceinformation-l1-1-0.dll
            //
            // Note: You can avoid the need for delay loading if you require 10.0.16299 as your minimum OS version
            //       and/or you restrict your package to the Xbox device family

            if (QueryOptionalDelayLoadedAPI(reinterpret_cast<HMODULE>(&__ImageBase),
                "api-ms-win-gaming-deviceinformation-l1-1-0.dll",
                "GetGamingDeviceModelInformation",
                0))
            {
                GAMING_DEVICE_MODEL_INFORMATION info = {};
                GetGamingDeviceModelInformation(&info);

                wchar_t buff[128] = {};
                swprintf_s(buff, L"%08X", info.vendorId);

                switch (info.vendorId)
                {
                case GAMING_DEVICE_VENDOR_ID_MICROSOFT: wcscat_s(buff, L" (Microsoft)"); break;
                }

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"VendorId", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%08X", info.deviceId);

                if (info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT)
                {
                    switch (info.deviceId)
                    {
                    case GAMING_DEVICE_DEVICE_ID_XBOX_ONE: wcscat_s(buff, L" (Xbox One)"); break;
                    case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_S: wcscat_s(buff, L" (Xbox One S)"); break;
                    case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X: wcscat_s(buff, L" (Xbox One X)"); break;
                    case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X_DEVKIT: wcscat_s(buff, L" (Xbox One X Dev Kit)"); break;
                    }
                }

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DeviceId", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
            }
            else
            #endif
            {
                y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"This API requires Windows 10 (16299) or later", mid, y, ATG::Colors::Orange, m_scale);
            }
        }
        break;

    case InfoPage::APICONTRACT_PAGE:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"IsApiContractPresent", mid, y, ATG::Colors::LightGrey, m_scale);

            using namespace Windows::Foundation::Metadata;

            // https://docs.microsoft.com/en-us/uwp/extension-sdks/windows-universal-sdk

            bool isfoundation2 = ApiInformation::IsApiContractPresent("Windows.Foundation.FoundationContract", 2, 0);
            bool isfoundation3 = ApiInformation::IsApiContractPresent("Windows.Foundation.FoundationContract", 3, 0);
            bool isuniversal2 = ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 2, 0);
            bool isuniversal3 = ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 3, 0);
            bool isuniversal4 = ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 4, 0);
            bool isuniversal5 = ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 5, 0);
            bool isuniversal6 = ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 6, 0);
            bool isuniversal7 = ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 7, 0);
            bool isphone = ApiInformation::IsApiContractPresent("Windows.Phone.PhoneContract", 1, 0);
            bool isstore2 = ApiInformation::IsApiContractPresent("Windows.Services.Store.StoreContract", 2, 0);
            bool isstore3 = ApiInformation::IsApiContractPresent("Windows.Services.Store.StoreContract", 3, 0);
            bool isstore4 = ApiInformation::IsApiContractPresent("Windows.Services.Store.StoreContract", 4, 0);
            bool xliveStorage = ApiInformation::IsApiContractPresent("Windows.Gaming.XboxLive.StorageApiContract", 1, 0);
            bool xliveSecure = ApiInformation::IsApiContractPresent("Windows.Networking.XboxLive.XboxLiveSecureSocketsContract", 1, 0);

            assert(ApiInformation::IsApiContractPresent("Windows.Foundation.FoundationContract", 1, 0));
            wchar_t contracts[256] = L"1.0";
            if (isfoundation2) { wcscat_s(contracts, L", 2.0"); }
            if (isfoundation3) { wcscat_s(contracts, L", 3.0"); }

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"FoundationContract", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), contracts, right, y, m_scale);

            assert(ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 1, 0));
            wcscpy_s(contracts, L"1.0");
            if (isuniversal2) { wcscat_s(contracts, L", 2.0"); }
            if (isuniversal3) { wcscat_s(contracts, L", 3.0"); }
            if (isuniversal4) { wcscat_s(contracts, L", 4.0"); }
            if (isuniversal5) { wcscat_s(contracts, L", 5.0"); }
            if (isuniversal6) { wcscat_s(contracts, L", 6.0"); }
            if (isuniversal7) { wcscat_s(contracts, L", 7.0"); }

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"UniversalApiContract", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), contracts, right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"PhoneContract", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), isphone ? L"1.0" : L"", right, y, m_scale);

            assert(ApiInformation::IsApiContractPresent("Windows.Services.Store.StoreContract", 1, 0));
            wcscpy_s(contracts, L"1.0");
            if (isstore2) { wcscat_s(contracts, L", 2.0"); }
            if (isstore3) { wcscat_s(contracts, L", 3.0"); }
            if (isstore4) { wcscat_s(contracts, L", 4.0"); }

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"StoreContract", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), contracts, right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"XboxLive StorageApiContract", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), xliveStorage ? L"1.0" : L"", right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"XboxLive SecureSocketsContract", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), xliveSecure ? L"1.0" : L"", right, y, m_scale);
        }
        break;

    case InfoPage::CPUSETS:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"GetSystemCpuSetInformation", mid, y, ATG::Colors::LightGrey, m_scale);

            ULONG retsize = 0;
            (void)GetSystemCpuSetInformation(nullptr, 0, &retsize, GetCurrentProcess(), 0);

            std::unique_ptr<uint8_t[]> data(new uint8_t[retsize]);
            if (GetSystemCpuSetInformation(
                reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(data.get()),
                retsize, &retsize, GetCurrentProcess(), 0))
            {
                size_t logicalProcessors = 0;
                size_t parkedProcessors = 0;
                size_t allocatedProcessors = 0;
                size_t allocatedElsewhere = 0;
                size_t availableProcessors = 0;
                std::set<DWORD> cores;
                bool moreThanOneGroup = false;

                uint8_t const * ptr = data.get();
                for (DWORD size = 0; size < retsize; )
                {
                    auto info = reinterpret_cast<const SYSTEM_CPU_SET_INFORMATION*>(ptr);
                    if (info->Type == CpuSetInformation)
                    {
                        if (info->CpuSet.Group > 0)
                        {
                            moreThanOneGroup = true;
                        }
                        else
                        {
                            ++logicalProcessors;

                            if (info->CpuSet.Parked)
                            {
                                ++parkedProcessors;
                            }
                            else
                            {
                                if (info->CpuSet.Allocated)
                                {
                                    if (info->CpuSet.AllocatedToTargetProcess)
                                    {
                                        ++allocatedProcessors;
                                        ++availableProcessors;
                                        cores.insert(info->CpuSet.CoreIndex);
                                    }
                                    else
                                    {
                                        ++allocatedElsewhere;
                                    }
                                }
                                else
                                {
                                    ++availableProcessors;
                                    cores.insert(info->CpuSet.CoreIndex);
                                }
                            }
                        }
                    }
                    ptr += info->Size;
                    size += info->Size;
                }

                wchar_t buff[128] = {};
                swprintf_s(buff, L"%zu", logicalProcessors);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Total logical processors", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                if (parkedProcessors > 0)
                {
                    swprintf_s(buff, L"%zu", parkedProcessors);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Parked processors", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
                }

                if (allocatedElsewhere > 0)
                {
                    swprintf_s(buff, L"%zu", allocatedElsewhere);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Allocated to other processes", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
                }

                swprintf_s(buff, L"%zu", availableProcessors);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Available logical processors", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                if (allocatedProcessors > 0)
                {
                    swprintf_s(buff, L"%zu", allocatedProcessors);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Allocated logical processors", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
                }

                swprintf_s(buff, L"%zu", cores.size());
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Available physical cores", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                if (moreThanOneGroup)
                {
                    y += m_smallFont->GetLineSpacing() * m_scale;
                    y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"Note more than one group found; ignored extra groups!", mid, y, ATG::Colors::Orange, m_scale);
                }
            }
        }
        break;

    case InfoPage::DISPLAYINFO:
        {
            using namespace Windows::Graphics::Display;

            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"DisplayInformation", mid, y, ATG::Colors::LightGrey, m_scale);

            auto displayInformation = DisplayInformation::GetForCurrentView();

            wchar_t buff[128] = {};
            swprintf_s(buff, L"%u %%", displayInformation->ResolutionScale);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Resolution Scale", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            swprintf_s(buff, L"%.f (X:%.f  Y:%.f)", displayInformation->LogicalDpi, displayInformation->RawDpiX, displayInformation->RawDpiY);
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Dots Per Inch (DPI)", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

            try
            {
                swprintf_s(buff, L"%u x %u (pixels)", displayInformation->ScreenWidthInRawPixels, displayInformation->ScreenHeightInRawPixels);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Screen Size", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
            }
            catch (...)
            {
                y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"Screen size requires Windows 10 (14393) or later", mid, y, ATG::Colors::Orange, m_scale);
            }

            const wchar_t* orientation = nullptr;
            switch (displayInformation->CurrentOrientation)
            {
            case DisplayOrientations::Landscape: orientation = L"Landscape"; break;
            case DisplayOrientations::LandscapeFlipped: orientation = L"Landscape (flipped)"; break;
            case DisplayOrientations::Portrait: orientation = L"Portrait"; break;
            case DisplayOrientations::PortraitFlipped: orientation = L"Portrait (flipped)"; break;
            default: orientation = L"None"; break;
            }
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Current Orientation", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), orientation, right, y, m_scale);

            switch (displayInformation->NativeOrientation)
            {
            case DisplayOrientations::Landscape: orientation = L"Landscape"; break;
            case DisplayOrientations::LandscapeFlipped: orientation = L"Landscape (flipped)"; break;
            case DisplayOrientations::Portrait: orientation = L"Portrait"; break;
            case DisplayOrientations::PortraitFlipped: orientation = L"Portrait (flipped)"; break;
            default: orientation = L"None"; break;
            }
            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Native Orientation", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), orientation, right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Stereoscopic 3D", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), displayInformation->StereoEnabled ? L"Enabled" : L"Disabled", right, y, m_scale);
        }
        break;

    case InfoPage::DXGI:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"DXGI", mid, y, ATG::Colors::LightGrey, m_scale);

            y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"DXGI_OUTPUT_DESC", mid, y, ATG::Colors::OffWhite, m_scale);

            ComPtr<IDXGIOutput> output;
            if (SUCCEEDED(m_deviceResources->GetSwapChain()->GetContainingOutput(output.GetAddressOf())))
            {
                DXGI_OUTPUT_DESC outputDesc = {};
                DX::ThrowIfFailed(output->GetDesc(&outputDesc));

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DeviceName", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), outputDesc.DeviceName, right, y, m_scale);

                wchar_t buff[128] = {};
                swprintf_s(buff, L"%u,%u,%u,%u", outputDesc.DesktopCoordinates.left, outputDesc.DesktopCoordinates.top, outputDesc.DesktopCoordinates.right, outputDesc.DesktopCoordinates.bottom );
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DesktopCoordinates", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                const wchar_t* rotation = L"UNSPECIFIED";
                switch (outputDesc.Rotation)
                {
                case DXGI_MODE_ROTATION_IDENTITY: rotation = L"IDENTITY"; break;
                case DXGI_MODE_ROTATION_ROTATE90: rotation = L"ROTATE90"; break;
                case DXGI_MODE_ROTATION_ROTATE180: rotation = L"ROTATE180"; break;
                case DXGI_MODE_ROTATION_ROTATE270: rotation = L"ROTATION270"; break;
                }

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Rotation", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), rotation, right, y, m_scale) * 1.25f;


                #if defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
                ComPtr<IDXGIOutput6> output6;
                if (SUCCEEDED(output.As(&output6)))
                {
                    DXGI_OUTPUT_DESC1 outputDesc6;
                    DX::ThrowIfFailed(output6->GetDesc1(&outputDesc6));

                    const wchar_t* colorSpace = L"sRGB";

                    switch (outputDesc6.ColorSpace)
                    {
                    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020: colorSpace = L"HDR10"; break;
                    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709: colorSpace = L"Linear"; break;
                    }

                    y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"DXGI_OUTPUT_DESC1", mid, y, ATG::Colors::OffWhite, m_scale);

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ColorSpace", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), colorSpace, right, y, m_scale);
                }
                #endif

                y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"DXGI_ADAPTER_DESC", mid, y, ATG::Colors::OffWhite, m_scale);

                ComPtr<IDXGIAdapter> adapter;
                if (SUCCEEDED(output->GetParent(IID_PPV_ARGS(adapter.GetAddressOf()))))
                {
                    DXGI_ADAPTER_DESC adapterDesc = {};
                    DX::ThrowIfFailed(adapter->GetDesc(&adapterDesc));

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Description", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), adapterDesc.Description, right, y, m_scale);

                    swprintf_s(buff, L"%04X / %04X", adapterDesc.VendorId, adapterDesc.DeviceId);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"VendorId / DeviceId", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                    swprintf_s(buff, L"%08X / %u", adapterDesc.SubSysId, adapterDesc.Revision);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"SubSysId / Revision", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                    auto dvm = static_cast<uint32_t>(adapterDesc.DedicatedVideoMemory / (1024 * 1024));
                    auto dsm = static_cast<uint32_t>(adapterDesc.DedicatedSystemMemory / (1024 * 1024));
                    auto ssm = static_cast<uint32_t>(adapterDesc.SharedSystemMemory / (1024 * 1024));

                    swprintf_s(buff, L"%u (MiB)", dvm);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DedicatedVideoMemory", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                    swprintf_s(buff, L"%u (MiB) / %u (MiB)", dsm, ssm);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Dedicated / Shared SystemMemory", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
                }
            }

            ComPtr<IDXGIDevice3> dxgiDevice;
            if (SUCCEEDED(m_deviceResources->GetD3DDevice()->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()))))
            {
                ComPtr<IDXGIAdapter> dxgiAdapter;
                if (SUCCEEDED(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf())))
                {
                    ComPtr<IDXGIFactory5> dxgiFactory;
                    if (SUCCEEDED(dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf()))))
                    {
                        BOOL allowTearing = FALSE;
                        if (SUCCEEDED(dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(BOOL))))
                        {
                            y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"DXGI 1.5", mid, y, ATG::Colors::OffWhite, m_scale);

                            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Allow Tearing", left, y, m_scale);
                            y += DrawStringRight(m_batch.get(), m_smallFont.get(), allowTearing ? L"true" : L"false", right, y, m_scale);
                        }
                    }
                }
            }
        }
        break;

    case InfoPage::DIRECT3D11_1:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D 11.1", mid, y, ATG::Colors::LightGrey, m_scale);

            const wchar_t* featLevel = L"Unknown";
            switch (m_deviceResources->GetDeviceFeatureLevel())
            {
            case D3D_FEATURE_LEVEL_9_1: featLevel = L"9.1"; break;
            case D3D_FEATURE_LEVEL_9_2: featLevel = L"9.2"; break;
            case D3D_FEATURE_LEVEL_9_3: featLevel = L"9.3"; break;
            case D3D_FEATURE_LEVEL_10_0: featLevel = L"10.0"; break;
            case D3D_FEATURE_LEVEL_10_1: featLevel = L"10.1"; break;
            case D3D_FEATURE_LEVEL_11_0: featLevel = L"11.0"; break;
            case D3D_FEATURE_LEVEL_11_1: featLevel = L"11.1"; break;
            case D3D_FEATURE_LEVEL_12_0: featLevel = L"12.0"; break;
            case D3D_FEATURE_LEVEL_12_1: featLevel = L"12.1"; break;
            }

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Hardware Feature Level", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), featLevel, right, y, m_scale);

            auto device = m_deviceResources->GetD3DDevice();

            D3D11_FEATURE_DATA_DOUBLES doubles = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_DOUBLES, &doubles, sizeof(doubles))))
            {
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DoublePrecisionFloatShaderOps", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), doubles.DoublePrecisionFloatShaderOps ? L"true" : L"false", right, y, m_scale);
            }

            D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS d3d10compute = {};
            if (FAILED(device->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &d3d10compute, sizeof(d3d10compute))))
            {
                memset(&d3d10compute, 0, sizeof(d3d10compute));
            }

            const wchar_t* directcompute = L"No";
            if (m_deviceResources->GetDeviceFeatureLevel() >= D3D_FEATURE_LEVEL_11_0)
            {
                directcompute = L"5.0";
            }
            else if (d3d10compute.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
            {
                directcompute = (m_deviceResources->GetDeviceFeatureLevel() >= D3D_FEATURE_LEVEL_10_1) ? L"4.1" : L"4.0";
            }

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DirectCompute", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), directcompute, right, y, m_scale);

            D3D11_FEATURE_DATA_D3D11_OPTIONS d3d11opts = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &d3d11opts, sizeof(d3d11opts))))
            {
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"OutputMergerLogicOp", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts.OutputMergerLogicOp ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ClearView", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts.ClearView ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"CBPartialUpdate", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts.ConstantBufferPartialUpdate ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"CBOffsetting", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts.ConstantBufferOffsetting ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"MapNoOverwriteOnDynamicCB", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts.MapNoOverwriteOnDynamicConstantBuffer ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"MapNoOverwriteOnDynamicSRV", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts.MapNoOverwriteOnDynamicBufferSRV ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"SAD4ShaderInstructions", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts.SAD4ShaderInstructions ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ExtendedDoublesShaderInstructions", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts.ExtendedDoublesShaderInstructions ? L"true" : L"false", right, y, m_scale);
            }

            D3D11_FEATURE_DATA_ARCHITECTURE_INFO arch = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_ARCHITECTURE_INFO, &arch, sizeof(arch))))
            {
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"TileBasedDeferredRenderer", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), arch.TileBasedDeferredRenderer ? L"true" : L"false", right, y, m_scale);
            }
        }
        break;

    case InfoPage::DIRECT3D11_2:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D 11.2", mid, y, ATG::Colors::LightGrey, m_scale);

            auto device = m_deviceResources->GetD3DDevice();

            D3D11_FEATURE_DATA_D3D11_OPTIONS1 d3d11opts1 = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS1, &d3d11opts1, sizeof(d3d11opts1))))
            {
                const wchar_t* tiledTier = L"Unknown";
                switch (d3d11opts1.TiledResourcesTier)
                {
                case D3D11_TILED_RESOURCES_NOT_SUPPORTED: tiledTier = L"Not supported"; break;
                case D3D11_TILED_RESOURCES_TIER_1: tiledTier = L"Tier 1"; break;
                case D3D11_TILED_RESOURCES_TIER_2: tiledTier = L"Tier 2"; break;
                case D3D11_TILED_RESOURCES_TIER_3: tiledTier = L"Tier 3"; break;
                }

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"TiledResourcesTier", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), tiledTier, right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"MinMaxFiltering", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts1.MinMaxFiltering ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ClearView(...)DepthOnlyFormats", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts1.ClearViewAlsoSupportsDepthOnlyFormats ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"MapOnDefaultBuffers", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts1.MapOnDefaultBuffers ? L"true" : L"false", right, y, m_scale);
            }
        }
        break;

    case InfoPage::DIRECT3D11_3:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D 11.3", mid, y, ATG::Colors::LightGrey, m_scale);

            auto device = m_deviceResources->GetD3DDevice();

            D3D11_FEATURE_DATA_D3D11_OPTIONS2 d3d11opts2 = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &d3d11opts2, sizeof(d3d11opts2))))
            {
                const wchar_t* cRastTier = L"Unknown";
                switch (d3d11opts2.ConservativeRasterizationTier)
                {
                case D3D11_CONSERVATIVE_RASTERIZATION_NOT_SUPPORTED: cRastTier = L"Not supported"; break;
                case D3D11_CONSERVATIVE_RASTERIZATION_TIER_1: cRastTier = L"Tier 1"; break;
                case D3D11_CONSERVATIVE_RASTERIZATION_TIER_2: cRastTier = L"Tier 2"; break;
                case D3D11_CONSERVATIVE_RASTERIZATION_TIER_3: cRastTier = L"Tier 3"; break;
                }

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ConservativeRasterizationTier", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), cRastTier, right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"PSSpecifiedStencilRefSupported", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts2.PSSpecifiedStencilRefSupported ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"TypedUAVLoadAdditionalFormats", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts2.TypedUAVLoadAdditionalFormats ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ROVsSupported", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts2.ROVsSupported ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"MapOnDefaultTextures", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts2.MapOnDefaultTextures ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"StandardSwizzle", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts2.StandardSwizzle ? L"true" : L"false", right, y, m_scale);

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"UnifiedMemoryArchitecture", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts2.UnifiedMemoryArchitecture ? L"true" : L"false", right, y, m_scale);
            }

            D3D11_FEATURE_DATA_D3D11_OPTIONS3 d3d11opts3 = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &d3d11opts3, sizeof(d3d11opts3))))
            {
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"VPAndRT(...)Rasterizer", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts3.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer ? L"true" : L"false", right, y, m_scale);
            }
        }
        break;

    case InfoPage::DIRECT3D11_4:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D 11.4", mid, y, ATG::Colors::LightGrey, m_scale);

            auto device = m_deviceResources->GetD3DDevice();

            ComPtr<ID3D11Device4> device4;
            if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(device4.GetAddressOf()))))
            {
                // Optional Direct3D 11.4 features for Windows 10 Anniversary Update
                D3D11_FEATURE_DATA_D3D11_OPTIONS4 d3d11opts4 = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS4, &d3d11opts4, sizeof(d3d11opts4))))
                {
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Extended NV12 Shared", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d11opts4.ExtendedNV12SharedTextureSupported ? L"true" : L"false", right, y, m_scale);
                }
                else
                {
                    y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"Partial support with Windows 10 Version 1511", mid, y, ATG::Colors::OffWhite, m_scale);
                }
            }
            else
            {
                y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"Not supported with Windows 10 RTM", mid, y, ATG::Colors::Orange, m_scale);
            }
        }
        break;

    case InfoPage::DIRECT3D12:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D 12", mid, y, ATG::Colors::LightGrey, m_scale);

            auto device = m_deviceResources->GetD3DDevice12();
            
            if (!device)
            {
                y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"Not supported", mid, y, ATG::Colors::Orange, m_scale);
            }
            else
            {
                // Determine highest feature level
                static const D3D_FEATURE_LEVEL s_featureLevels[] =
                {
                    D3D_FEATURE_LEVEL_12_1,
                    D3D_FEATURE_LEVEL_12_0,
                    D3D_FEATURE_LEVEL_11_1,
                    D3D_FEATURE_LEVEL_11_0,
                };

                D3D12_FEATURE_DATA_FEATURE_LEVELS featLevels =
                {
                    _countof(s_featureLevels), s_featureLevels, D3D_FEATURE_LEVEL_11_0
                };

                if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featLevels, sizeof(featLevels))))
                {
                    featLevels.MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_9_1;
                }

                const wchar_t* featLevel = L"Unknown";
                switch (featLevels.MaxSupportedFeatureLevel)
                {
                case D3D_FEATURE_LEVEL_11_0: featLevel = L"11.0"; break;
                case D3D_FEATURE_LEVEL_11_1: featLevel = L"11.1"; break;
                case D3D_FEATURE_LEVEL_12_0: featLevel = L"12.0"; break;
                case D3D_FEATURE_LEVEL_12_1: featLevel = L"12.1"; break;
                }

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Hardware Feature Level", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), featLevel, right, y, m_scale);

                // Determine maximum shader model / root signature
                D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSig = {};
                rootSig.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
                if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSig, sizeof(rootSig))))
                {
                    rootSig.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
                }

                const wchar_t* rootSigVer = L"Unknown";
                switch (rootSig.HighestVersion)
                {
                case D3D_ROOT_SIGNATURE_VERSION_1_0: rootSigVer = L"1.0"; break;
                case D3D_ROOT_SIGNATURE_VERSION_1_1: rootSigVer = L"1.1"; break;
                }

                D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = {};
                #if defined(NTDDI_WIN10_RS5) && (NTDDI_VERSION >= NTDDI_WIN10_RS5)
                shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_4;
                #elif defined(NTDDI_WIN10_RS4) && (NTDDI_VERSION >= NTDDI_WIN10_RS4)
                shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_2;
                #else
                shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_0;
                #endif
                if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))))
                {
                    shaderModel.HighestShaderModel = D3D_SHADER_MODEL_5_1;
                }

                const wchar_t* shaderModelVer = L"Unknown";
                switch (shaderModel.HighestShaderModel)
                {
                case D3D_SHADER_MODEL_5_1: shaderModelVer = L"5.1"; break;
                case D3D_SHADER_MODEL_6_0: shaderModelVer = L"6.0"; break;

                #if defined(NTDDI_WIN10_RS3) && (NTDDI_VERSION >= NTDDI_WIN10_RS3)
                case D3D_SHADER_MODEL_6_1: shaderModelVer = L"6.1"; break;
                #endif

                #if defined(NTDDI_WIN10_RS4) && (NTDDI_VERSION >= NTDDI_WIN10_RS4)
                case D3D_SHADER_MODEL_6_2: shaderModelVer = L"6.2"; break;
                #endif

                #if defined(NTDDI_WIN10_RS5) && (NTDDI_VERSION >= NTDDI_WIN10_RS5)
                case D3D_SHADER_MODEL_6_3: shaderModelVer = L"6.3"; break;
                case D3D_SHADER_MODEL_6_4: shaderModelVer = L"6.4"; break;
                #endif
                }

                wchar_t buff[64] = {};
                swprintf_s(buff, L"%ls / %ls", shaderModelVer, rootSigVer);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Shader Model / Root Signature", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                // Optional Direct3D 12 features
                D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12opts = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &d3d12opts, sizeof(d3d12opts))))
                {
                    const wchar_t* tiledTier = L"Unknown";
                    switch (d3d12opts.TiledResourcesTier)
                    {
                    case D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED: tiledTier = L"Not supported"; break;
                    case D3D12_TILED_RESOURCES_TIER_1: tiledTier = L"Tier 1"; break;
                    case D3D12_TILED_RESOURCES_TIER_2: tiledTier = L"Tier 2"; break;
                    case D3D12_TILED_RESOURCES_TIER_3: tiledTier = L"Tier 3"; break;

                    #if defined(NTDDI_WIN10_RS4) && (NTDDI_VERSION >= NTDDI_WIN10_RS4)
                    case D3D12_TILED_RESOURCES_TIER_4: tiledTier = L"Tier 4"; break;
                    #endif
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"TiledResourcesTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), tiledTier, right, y, m_scale);

                    const wchar_t* resourceTier = L"Unknown";
                    switch (d3d12opts.ResourceBindingTier)
                    {
                    case D3D12_RESOURCE_BINDING_TIER_1: resourceTier = L"Tier 1"; break;
                    case D3D12_RESOURCE_BINDING_TIER_2: resourceTier = L"Tier 2"; break;
                    case D3D12_RESOURCE_BINDING_TIER_3: resourceTier = L"Tier 3"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ResourceBindingTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), resourceTier, right, y, m_scale);

                    const wchar_t* cRastTier = L"Unknown";
                    switch (d3d12opts.ConservativeRasterizationTier)
                    {
                    case D3D12_CONSERVATIVE_RASTERIZATION_TIER_NOT_SUPPORTED: cRastTier = L"Not supported"; break;
                    case D3D12_CONSERVATIVE_RASTERIZATION_TIER_1: cRastTier = L"Tier 1"; break;
                    case D3D12_CONSERVATIVE_RASTERIZATION_TIER_2: cRastTier = L"Tier 2"; break;
                    case D3D12_CONSERVATIVE_RASTERIZATION_TIER_3: cRastTier = L"Tier 3"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ConservativeRasterizationTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), cRastTier, right, y, m_scale);

                    const wchar_t* heapTier = L"Unknown";
                    switch (d3d12opts.ResourceHeapTier)
                    {
                    case D3D12_RESOURCE_HEAP_TIER_1: heapTier = L"Tier 1"; break;
                    case D3D12_RESOURCE_HEAP_TIER_2: heapTier = L"Tier 2"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ResourceHeapTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), heapTier, right, y, m_scale);

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"StandardSwizzle64KBSupported", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts.StandardSwizzle64KBSupported ? L"true" : L"false", right, y, m_scale);

                    const wchar_t* crossTier = L"Unknown";
                    switch (d3d12opts.CrossNodeSharingTier)
                    {
                    case D3D12_CROSS_NODE_SHARING_TIER_NOT_SUPPORTED: crossTier = L"Not supported"; break;
                    case D3D12_CROSS_NODE_SHARING_TIER_1_EMULATED: crossTier = L"Tier 1 (emulated)"; break;
                    case D3D12_CROSS_NODE_SHARING_TIER_1: crossTier = L"Tier 1"; break;
                    case D3D12_CROSS_NODE_SHARING_TIER_2: crossTier = L"Tier 2"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"CrossNodeSharingTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), crossTier, right, y, m_scale);

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"CrossAdapterRowMajorTextureSupported", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts.CrossAdapterRowMajorTextureSupported ? L"true" : L"false", right, y, m_scale);

                    swprintf_s(buff, L"%u", d3d12opts.MaxGPUVirtualAddressBitsPerResource);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"MaxGPUVirtualAddressBitsPerResource", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
                }
            }
        }
        break;

    case InfoPage::DIRECT3D12_OPT1:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D 12 Optional Features", mid, y, ATG::Colors::LightGrey, m_scale);

            auto device = m_deviceResources->GetD3DDevice12();

            if (!device)
            {
                y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"Not supported", mid, y, ATG::Colors::Orange, m_scale);
            }
            else
            {
                // Optional Direct3D 12 features for Windows 10 Anniversary Update
                D3D12_FEATURE_DATA_D3D12_OPTIONS1 d3d12opts1 = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &d3d12opts1, sizeof(d3d12opts1))))
                {
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"WaveOps", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts1.WaveOps ? L"true" : L"false", right, y, m_scale);

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ExpandedComputeResourceStates", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts1.ExpandedComputeResourceStates ? L"true" : L"false", right, y, m_scale);

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Int64ShaderOps", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts1.Int64ShaderOps ? L"true" : L"false", right, y, m_scale);
                }

                // Optional Direct3D 12 features for Windows 10 Creators Update
                #if defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
                D3D12_FEATURE_DATA_D3D12_OPTIONS2 d3d12opts2 = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &d3d12opts2, sizeof(d3d12opts2))))
                {
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DepthBoundsTestSupported", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts2.DepthBoundsTestSupported ? L"true" : L"false", right, y, m_scale);

                    const wchar_t* psmpTier = L"Unknown";
                    switch (d3d12opts2.ProgrammableSamplePositionsTier)
                    {
                    case D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED: psmpTier = L"Not supported"; break;
                    case D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_1: psmpTier = L"Tier 1"; break;
                    case D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_2: psmpTier = L"Tier 2"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ProgrammableSamplePositionsTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), psmpTier, right, y, m_scale);
                }
                #endif

                // Optional Direct3D 12 features for Windows 10 Fall Creators Update
                #if defined(NTDDI_WIN10_RS3) && (NTDDI_VERSION >= NTDDI_WIN10_RS3)
                D3D12_FEATURE_DATA_D3D12_OPTIONS3 d3d12opts3 = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &d3d12opts3, sizeof(d3d12opts3))))
                {
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"CopyQueueTimestampQueriesSupported", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts3.CopyQueueTimestampQueriesSupported ? L"true" : L"false", right, y, m_scale);

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"CastingFullyTypedFormatSupported", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts3.CastingFullyTypedFormatSupported ? L"true" : L"false", right, y, m_scale);

                    d3d12opts3.WriteBufferImmediateSupportFlags = static_cast<D3D12_COMMAND_LIST_SUPPORT_FLAGS>(0xFFFFFFFF);
                    wchar_t vbSupportFlags[128] = {};
                    if (d3d12opts3.WriteBufferImmediateSupportFlags & D3D12_COMMAND_LIST_SUPPORT_FLAG_DIRECT)
                    {
                        wcscat_s(vbSupportFlags, L"DIRECT ");
                    }
                    if (d3d12opts3.WriteBufferImmediateSupportFlags & D3D12_COMMAND_LIST_SUPPORT_FLAG_BUNDLE)
                    {
                        wcscat_s(vbSupportFlags, L"BUNDLE ");
                    }
                    if (d3d12opts3.WriteBufferImmediateSupportFlags & D3D12_COMMAND_LIST_SUPPORT_FLAG_COMPUTE)
                    {
                        wcscat_s(vbSupportFlags, L"COMPUTE ");
                    }
                    if (d3d12opts3.WriteBufferImmediateSupportFlags & D3D12_COMMAND_LIST_SUPPORT_FLAG_COPY)
                    {
                        wcscat_s(vbSupportFlags, L"COPY ");
                    }
                    if (d3d12opts3.WriteBufferImmediateSupportFlags & D3D12_COMMAND_LIST_SUPPORT_FLAG_VIDEO_DECODE)
                    {
                        wcscat_s(vbSupportFlags, L"VDECODE ");
                    }
                    if (d3d12opts3.WriteBufferImmediateSupportFlags & D3D12_COMMAND_LIST_SUPPORT_FLAG_VIDEO_PROCESS)
                    {
                        wcscat_s(vbSupportFlags, L"VPROCESS");
                    }
                    if (!*vbSupportFlags)
                    {
                        wcscat_s(vbSupportFlags, L"None");
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"WriteBufferImmediateSupportFlags", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), vbSupportFlags, right, y, m_scale);

                    const wchar_t* vinstTier  = L"Unknown";
                    switch (d3d12opts3.ViewInstancingTier)
                    {
                    case D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED: vinstTier = L"Not supported"; break;
                    case D3D12_VIEW_INSTANCING_TIER_1: vinstTier = L"Tier 1"; break;
                    case D3D12_VIEW_INSTANCING_TIER_2: vinstTier = L"Tier 2"; break;
                    case D3D12_VIEW_INSTANCING_TIER_3: vinstTier = L"Tier 3"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"ViewInstancingTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), vinstTier, right, y, m_scale);

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"BarycentricsSupported", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts3.BarycentricsSupported ? L"true" : L"false", right, y, m_scale);
                }

                D3D12_FEATURE_DATA_EXISTING_HEAPS d3d12heaps = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_EXISTING_HEAPS, &d3d12heaps, sizeof(d3d12heaps))))
                {
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Existing Heaps", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12heaps.Supported ? L"true" : L"false", right, y, m_scale);
                }
                #endif
            }
        }
        break;

    case InfoPage::DIRECT3D12_OPT2:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D 12 Optional Features (continued)", mid, y, ATG::Colors::LightGrey, m_scale);

            auto device = m_deviceResources->GetD3DDevice12();

            if (!device)
            {
                y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"Not supported", mid, y, ATG::Colors::Orange, m_scale);
            }
            else
            {
                // Optional Direct3D 12 features for Windows 10 April 2018 Update
                #if defined(NTDDI_WIN10_RS4) && (NTDDI_VERSION >= NTDDI_WIN10_RS4)
                D3D12_FEATURE_DATA_D3D12_OPTIONS4 d3d12opts4 = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &d3d12opts4, sizeof(d3d12opts4))))
                {
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"MSAA64KBAlignedTextureSupported", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts4.MSAA64KBAlignedTextureSupported ? L"true" : L"false", right, y, m_scale);

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Native16BitShaderOpsSupported", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts4.Native16BitShaderOpsSupported ? L"true" : L"false", right, y, m_scale);

                    const wchar_t* srcompatTier = L"Unknown";
                    switch (d3d12opts4.SharedResourceCompatibilityTier)
                    {
                        case D3D12_SHARED_RESOURCE_COMPATIBILITY_TIER_0: srcompatTier = L"Tier 0"; break;
                        case D3D12_SHARED_RESOURCE_COMPATIBILITY_TIER_1: srcompatTier = L"Tier 1"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"SharedResourceCompatibilityTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), srcompatTier, right, y, m_scale);
                }

                D3D12_FEATURE_DATA_SERIALIZATION d3d12serial = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_SERIALIZATION, &d3d12serial, sizeof(d3d12serial))))
                {
                    const wchar_t* serialTier = L"Unknown";
                    switch (d3d12serial.HeapSerializationTier)
                    {
                        case D3D12_HEAP_SERIALIZATION_TIER_0: serialTier = L"Tier 0"; break;
                        case D3D12_HEAP_SERIALIZATION_TIER_10: serialTier = L"Tier 10"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"HeapSerializationTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), serialTier, right, y, m_scale);

                    wchar_t buff[64] = {};
                    swprintf_s(buff, L"%u", d3d12serial.NodeIndex);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Serialization NodeIndex", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
                }

                D3D12_FEATURE_DATA_CROSS_NODE d3d12xnode = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_CROSS_NODE, &d3d12xnode, sizeof(d3d12xnode))))
                {
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Cross node AtomicShaderInstructions", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12xnode.AtomicShaderInstructions ? L"true" : L"false", right, y, m_scale);

                    const wchar_t* shareTier = L"Unknown";
                    switch (d3d12xnode.SharingTier)
                    {
                        case D3D12_CROSS_NODE_SHARING_TIER_NOT_SUPPORTED: shareTier = L"Not supported"; break;
                        case D3D12_CROSS_NODE_SHARING_TIER_1_EMULATED: shareTier = L"Tier 1 (Emulated)"; break;
                        case D3D12_CROSS_NODE_SHARING_TIER_1: shareTier = L"Tier 1"; break;
                        case D3D12_CROSS_NODE_SHARING_TIER_2: shareTier = L"Tier 2"; break;
                        case D3D12_CROSS_NODE_SHARING_TIER_3: shareTier = L"Tier 3"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"Cross node SharingTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), shareTier, right, y, m_scale);
                }
                #endif

                // Optional Direct3D 12 features for Windows 10 October 2018 Update
                #if defined(NTDDI_WIN10_RS5) && (NTDDI_VERSION >= NTDDI_WIN10_RS5)
                D3D12_FEATURE_DATA_D3D12_OPTIONS5 d3d12opts5 = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &d3d12opts5, sizeof(d3d12opts5))))
                {
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"SRVOnlyTiledResourceTier3", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), d3d12opts5.SRVOnlyTiledResourceTier3 ? L"true" : L"false", right, y, m_scale);

                    const wchar_t* passTier = L"Unknown";
                    switch (d3d12opts5.RenderPassesTier)
                    {
                    case D3D12_RENDER_PASS_TIER_0: passTier = L"Tier 0"; break;
                    case D3D12_RENDER_PASS_TIER_1: passTier = L"Tier 1"; break;
                    case D3D12_RENDER_PASS_TIER_2: passTier = L"Tier 2"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"RenderPassesTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), passTier, right, y, m_scale);

                    const wchar_t* rtTier = L"Unknown";
                    switch (d3d12opts5.RaytracingTier)
                    {
                    case D3D12_RAYTRACING_TIER_NOT_SUPPORTED: rtTier = L"Not Supported"; break;
                    case D3D12_RAYTRACING_TIER_1_0: rtTier = L"Tier 1"; break;
                    }

                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"RaytracingTier", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), rtTier, right, y, m_scale);
                }
                #endif
            }
        }
        break;
    }

    m_batch->End();

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

    // Clear the views
    auto renderTarget = m_deviceResources->GetRenderTargetView();

    // Don't need to clear render target since we drawing image to fill screen

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

    m_batch = std::make_unique<SpriteBatch>(context);

    m_smallFont = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_largeFont = std::make_unique<SpriteFont>(device, L"SegoeUI_36.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"ATGSampleBackground.DDS", nullptr, m_background.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    auto vp = m_deviceResources->GetScreenViewport();
    m_batch->SetViewport(vp);

    m_batch->SetRotation(m_deviceResources->GetRotation());

    auto size = m_deviceResources->GetOutputSize();

    if (size.bottom <= 200)
        m_scale = 0.25f;
    else if (size.bottom <= 480)
        m_scale = 0.5f;
    else if (size.bottom <= 600)
        m_scale = 0.75f;
    else if (size.bottom >= 1080)
        m_scale = 1.5f;
    else if (size.bottom >= 720)
        m_scale = 1.25f;
    else
        m_scale = 1.f;
}

void Sample::OnDeviceLost()
{
    m_batch.reset();
    m_smallFont.reset();
    m_largeFont.reset();
    m_ctrlFont.reset();
    m_background.Reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
