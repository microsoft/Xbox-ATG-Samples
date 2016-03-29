//--------------------------------------------------------------------------------------
// SystemInfo.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "SystemInfo.h"
#include "ControllerFont.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

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
        font->DrawString(batch, text, pos, Colors::White, 0.f, Vector2::Zero, scale);
    }

    inline float DrawStringRight(SpriteBatch* batch, SpriteFont* font, const wchar_t* text, float mid, float y, float scale)
    {
        XMFLOAT2 pos(mid, y);
        font->DrawString(batch, text, pos, Colors::White, 0.f, Vector2::Zero, scale);
        return font->GetLineSpacing()*scale;
    }
}

Sample::Sample() :
    m_scale(1.f),
    m_current(0),
    m_gamepadPresent(false)
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

    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

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
            Windows::ApplicationModel::Core::CoreApplication::Exit();
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
        Windows::ApplicationModel::Core::CoreApplication::Exit();
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

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.bottom));
    if (m_gamepadPresent)
    {
        DX::DrawControllerString(m_batch.get(), m_smallFont.get(), m_ctrlFont.get(), L"Use [A], [B], or [DPad] to cycle pages", pos, Colors::Gray, m_scale);
    }
    else
    {
        m_smallFont->DrawString(m_batch.get(), L"Use Left/Right to cycle pages", pos, Colors::Gray, 0, Vector2::Zero, m_scale);
    }

    float spacer = XMVectorGetX(m_smallFont->MeasureString(L"X")*m_scale);

    float left = mid - spacer;
    float right = mid + spacer;

    switch (static_cast<InfoPage>(m_current))
    {
    case InfoPage::SYSTEMINFO:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"GetNativeSystemInfo", mid, y, Colors::Yellow, m_scale);

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

            wchar_t buff[128] = { 0 };
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
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"GetProcessInformation", mid, y, Colors::Yellow, m_scale);

            APP_MEMORY_INFORMATION info = {};
            if (GetProcessInformation(GetCurrentProcess(), ProcessAppMemoryInfo, &info, sizeof(info)))
            {
                auto ac = static_cast<uint32_t>(info.AvailableCommit / (1024 * 1024));
                auto pc = static_cast<uint32_t>(info.PrivateCommitUsage / (1024 * 1024));
                auto ppc = static_cast<uint32_t>(info.PeakPrivateCommitUsage / (1024 * 1024));
                auto tc = static_cast<uint32_t>(info.TotalCommitUsage / (1024 * 1024));

                wchar_t buff[128] = { 0 };
                swprintf_s(buff, L"%u (MB)", ac);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"AvailableCommit", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u (MB)", pc);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"PrivateCommitUsage", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u (MB)", ppc);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"PeakPrivateCommitUsage", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                swprintf_s(buff, L"%u (MB)", tc);
                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"TotalCommitUsage", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
            }
        }
        break;

    case InfoPage::ANALYTICSINFO:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"AnalyticsInfo", mid, y, Colors::Yellow, m_scale);

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

    case InfoPage::APICONTRACT:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"IsApiContractPresent", mid, y, Colors::Yellow, m_scale);

            using namespace Windows::Foundation::Metadata;

            bool isfoundation = ApiInformation::IsApiContractPresent("Windows.Foundation.FoundationContract", 1, 0);
            bool isuniversal1 = ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 1, 0);
            bool isuniversal2 = ApiInformation::IsApiContractPresent("Windows.Foundation.UniversalApiContract", 2, 0);
            bool isphone = ApiInformation::IsApiContractPresent("Windows.Phone.PhoneContract", 1, 0);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"FoundationContract 1.0", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), isfoundation ? L"true" : L"false", right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"UniversalApiContract 1.0", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), isuniversal1 ? L"true" : L"false", right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"UniversalApiContract 2.0", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), isuniversal2 ? L"true" : L"false", right, y, m_scale);

            DrawStringLeft(m_batch.get(), m_smallFont.get(), L"PhoneContract 1.0", left, y, m_scale);
            y += DrawStringRight(m_batch.get(), m_smallFont.get(), isphone ? L"true" : L"false", right, y, m_scale);
        }
        break;

    case InfoPage::CPUSETS:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"GetSystemCpuSetInformation", mid, y, Colors::Yellow, m_scale);

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

                wchar_t buff[128] = { 0 };
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
                    y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"Note more than one group found; ignored extra groups!", mid, y, Colors::Red, m_scale);
                }
            }
        }
        break;

    case InfoPage::DXGI:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"DXGI", mid, y, Colors::Yellow, m_scale);

            y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"DXGI_OUTPUT_DESC", mid, y, Colors::White, m_scale);

            ComPtr<IDXGIOutput> output;
            if (SUCCEEDED(m_deviceResources->GetSwapChain()->GetContainingOutput(output.GetAddressOf())))
            {
                DXGI_OUTPUT_DESC outputDesc = {};
                DX::ThrowIfFailed(output->GetDesc(&outputDesc));

                DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DeviceName", left, y, m_scale);
                y += DrawStringRight(m_batch.get(), m_smallFont.get(), outputDesc.DeviceName, right, y, m_scale);

                wchar_t buff[128] = { 0 };
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

                y += DrawStringCenter(m_batch.get(), m_smallFont.get(), L"DXGI_ADAPTER_DESC", mid, y, Colors::White, m_scale);

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

                    swprintf_s(buff, L"%u (MB)", dvm);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DedicatedVideoMemory", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                    swprintf_s(buff, L"%u (MB)", dsm);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"DedicatedSystemMemory", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);

                    swprintf_s(buff, L"%u (MB)", ssm);
                    DrawStringLeft(m_batch.get(), m_smallFont.get(), L"SharedSystemMemory", left, y, m_scale);
                    y += DrawStringRight(m_batch.get(), m_smallFont.get(), buff, right, y, m_scale);
                }
            }
        }
        break;

    case InfoPage::DIRECT3D11_1:
        {
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D 11.1", mid, y, Colors::Yellow, m_scale);

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
            y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D 11.2", mid, y, Colors::Yellow, m_scale);

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
        y += DrawStringCenter(m_batch.get(), m_largeFont.get(), L"Direct3D 11.3", mid, y, Colors::Yellow, m_scale);

        auto device = m_deviceResources->GetD3DDevice();

            D3D11_FEATURE_DATA_D3D11_OPTIONS2 d3d11opts2 = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &d3d11opts2, sizeof(d3d11opts2))))
            {
                const wchar_t* cRastTier = L"Unknown";
                switch (d3d11opts2.ConservativeRasterizationTier)
                {
                case D3D11_TILED_RESOURCES_NOT_SUPPORTED: cRastTier = L"Not supported"; break;
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
    }

    m_batch->End();

    PIXEndEvent(context);
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views
    auto renderTarget = m_deviceResources->GetBackBufferRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
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
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_batch = std::make_unique<SpriteBatch>(context);

    m_smallFont = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_largeFont = std::make_unique<SpriteFont>(device, L"SegoeUI_36.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneController.spritefont");

    DX::ThrowIfFailed(CreateDDSTextureFromFile(device, L"ATGSampleBackground.DDS", nullptr, m_background.ReleaseAndGetAddressOf()));
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
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
