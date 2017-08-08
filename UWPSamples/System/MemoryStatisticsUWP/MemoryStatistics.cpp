//--------------------------------------------------------------------------------------
// MemoryStatistics.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "MemoryStatistics.h"

#include "ATGColors.h"
#include "ControllerFont.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

using Microsoft::WRL::ComPtr;

Sample::Sample() :
    m_frameMemoryUsage{0},
    m_preRunMemoryUsage{0},
    m_temporaryTextBuffer{0},
    m_temporaryTextTime(0.f),
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

    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();  	
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_teapots.reserve(c_MaxTeapots);
    CreateNewTeapot();

    // Grab a snapshot of the memory usage post-initialization so it can be compared to memory usage in any given frame
    (void)GetProcessMemoryInfo(GetCurrentProcess(), &m_preRunMemoryUsage, sizeof(m_preRunMemoryUsage));
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
void Sample::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());

    if (m_temporaryTextTime > 0.f)
    {
        m_temporaryTextTime -= elapsedTime;
        if (m_temporaryTextTime <= 0.f)
        {
            m_temporaryTextTime = 0.f;
            *m_temporaryTextBuffer = 0;
        }
    }

    // Query information about the memory usage
    // NOTE: This API requires linking to kernel32.lib so it cannot be used in release builds
    //  of a game. It can only be used in debug and prerelease builds.
    GetProcessMemoryInfo(GetCurrentProcess(), &m_frameMemoryUsage, sizeof(m_frameMemoryUsage));

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamepadPresent = true;

        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            Windows::ApplicationModel::Core::CoreApplication::Exit();
        }

        if (m_gamePadButtons.dpadRight == GamePad::ButtonStateTracker::PRESSED)
        {
            if (m_teapots.size() < c_MaxTeapots)
                CreateNewTeapot();
        }

        if (m_gamePadButtons.dpadLeft == GamePad::ButtonStateTracker::PRESSED)
        {
            if (m_teapots.size() > 1)
                DestroyTeapot();
        }

        if (m_gamePadButtons.y == GamePad::ButtonStateTracker::PRESSED)
        {
            PercentageStats();
        }
    }
    else
    {
        m_gamepadPresent = false;
        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.Escape)
    {
        Windows::ApplicationModel::Core::CoreApplication::Exit();
    }

    if (kb.Right)
    {
        if(m_teapots.size() < c_MaxTeapots)
            CreateNewTeapot();
    }
    else if (kb.Left)
    {
        if (m_teapots.size() > 1)
            DestroyTeapot();
    }
    else if (kb.P)
    {
        PercentageStats();
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

    for (auto & teapot : m_teapots)
    {
        auto world = Matrix::CreateRotationY(teapot.m_lifeFrameCount++ / 100.f);
        teapot.m_teapot->Draw(world * teapot.m_location, m_view, m_projection);
    }

    auto rect = m_deviceResources->GetOutputSize();
    auto safeRect = Viewport::ComputeTitleSafeArea(rect.right, rect.bottom);

    XMFLOAT2 pos(float(safeRect.left), float(safeRect.top));

    m_batch->Begin();

    if (*m_temporaryTextBuffer)
    {
        m_font->DrawString(m_batch.get(), m_temporaryTextBuffer, pos, ATG::Colors::Blue);
    }

    // The data from GetProcessMemoryInfo returns information in bytes. We're translating to MiB for
    //  easier onscreen readability.
    float workingSetSize = m_frameMemoryUsage.WorkingSetSize / (1024.f * 1024.f);
    float quotaPeakPagedPoolUsage = m_frameMemoryUsage.QuotaPeakPagedPoolUsage / (1024.f * 1024.f);
    float quotaPagedPoolUsage = m_frameMemoryUsage.QuotaPagedPoolUsage / (1024.f * 1024.f);
    float quotaPeakNonPagedPoolUsage = m_frameMemoryUsage.QuotaPeakNonPagedPoolUsage / (1024.f * 1024.f);
    float quotaNonPagedPoolUsage = m_frameMemoryUsage.QuotaNonPagedPoolUsage / (1024.f * 1024.f);
    float pagefileUsage = m_frameMemoryUsage.PagefileUsage / (1024.f * 1024.f);
    float peakPagefileUsage = m_frameMemoryUsage.PeakPagefileUsage / (1024.f * 1024.f);

    wchar_t buffer[2048] = { L'\0' };
    swprintf_s(buffer, L"PageFaultCount: %u\n"
        L"WorkingSetSize: %.3f (MiB)\n"
        L"QuotaPeakPagedPoolUsage: %.3f (MiB)\n"
        L"QuotaPagePoolUsage: %.3f (MiB)\n"
        L"QuotaPeakNonPagedPoolUsage: %.3f (MiB)\n"
        L"QuotaNonPagedPoolUsage: %.3f (MiB)\n"
        L"PagefileUsage: %.3f (MiB)\n"
        L"PeakPagefileUsage: %.3f (MiB)\n",
        m_frameMemoryUsage.PageFaultCount,
        workingSetSize,
        quotaPeakPagedPoolUsage,
        quotaPagedPoolUsage,
        quotaPeakNonPagedPoolUsage,
        quotaNonPagedPoolUsage,
        pagefileUsage,
        peakPagefileUsage);

    pos.y = float(safeRect.bottom) - XMVectorGetY(m_font->MeasureString(buffer)) - m_font->GetLineSpacing() * 1.5f;

    m_font->DrawString(m_batch.get(), buffer, pos, ATG::Colors::Green);

    pos.y = float(safeRect.bottom) - m_font->GetLineSpacing();

    if (m_gamepadPresent)
    {
        DX::DrawControllerString(m_batch.get(), m_font.get(), m_ctrlFont.get(), L"Use [DPad] to add/remove teapots, and the [Y] button for percentages", pos, ATG::Colors::OffWhite);
    }
    else
    {
        m_font->DrawString(m_batch.get(), L"Use Right key to add teapots, Left key to remove teapots, and the P key for percentages", pos, ATG::Colors::OffWhite);
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

    for (auto & teapot : m_teapots)
    {
        teapot.m_teapot = DirectX::GeometricPrimitive::CreateTeapot(context);
    }

    m_batch = std::make_unique<SpriteBatch>(context);
    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
    m_ctrlFont = std::make_unique<SpriteFont>(device, L"XboxOneControllerLegendSmall.spritefont");
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    m_batch->SetRotation(m_deviceResources->GetRotation());

    m_at = Vector3(0.f, 0.f, -0.1f);
    m_eye = Vector3(0.0f, 0.0f, 0.0f);
    m_view = Matrix::CreateLookAt(m_eye, m_at, Vector3::UnitY);

    auto size = m_deviceResources->GetOutputSize();
    float aspectRatio = float(size.right) / float(size.bottom);
    float fovAngleY = 70.0f * XM_PI / 180.0f;

    // This is a simple example of change that can be made when the app is in
    // portrait or snapped view.
    if (aspectRatio < 1.0f)
    {
        fovAngleY *= 2.0f;
    }

    Matrix projection = Matrix::CreatePerspectiveFieldOfView(
        fovAngleY,
        aspectRatio,
        0.01f,
        100.0f
        );

    m_projection = projection * m_deviceResources->GetOrientationTransform3D();
}

void Sample::OnDeviceLost()
{
    for (auto & teapot : m_teapots)
    {
        teapot.m_teapot.reset();
    }
    m_batch.reset();
    m_font.reset();
    m_ctrlFont.reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}

void Sample::CreateNewTeapot()
{
    // Grab a snapshot of memory usage before a teapot is allocated
    PROCESS_MEMORY_COUNTERS before, after;
    GetProcessMemoryInfo(GetCurrentProcess(), &before, sizeof(before));

    auto context = m_deviceResources->GetD3DDeviceContext();

    TeapotData teapot;
    teapot.m_lifeFrameCount = 0;
    teapot.m_location = Matrix::CreateTranslation(FloatRand(-4.f, 4.f), FloatRand(-4.f, 4.f), FloatRand(-8.f, -4.f));

    m_teapots.push_back(teapot);
    m_teapots.back().m_teapot = DirectX::GeometricPrimitive::CreateTeapot(context);

    // Now that a teapot has been created, grab another capture of resource usage
    GetProcessMemoryInfo(GetCurrentProcess(), &after, sizeof(after));

    // Comparing resource usage before and after a teapot was created will allow
    //  us to see exactly how much was needed for this particular action. Display
    //  this data to the screen since it can be interesting.
    size_t pfuDiff = after.PagefileUsage - before.PagefileUsage;
    size_t wssDiff = after.WorkingSetSize - before.WorkingSetSize;
    size_t qppDiff = after.QuotaPagedPoolUsage - before.QuotaPagedPoolUsage;
    size_t qnpDiff = after.QuotaNonPagedPoolUsage - before.QuotaNonPagedPoolUsage;

    swprintf_s(m_temporaryTextBuffer, L"Memory used by teapot creation:\n"
        L"PageFileUsage: %zu bytes\n"
        L"WorkingSetSize: %zu bytes\n"
        L"QuotaPagedPoolUsage: %zu bytes\n"
        L"QuotaNonPagedPoolUsage: %zu bytes\n\n",
        pfuDiff,
        wssDiff,
        qppDiff,
        qnpDiff);

    m_temporaryTextTime = 4.f;
}

void Sample::DestroyTeapot()
{
    m_teapots.pop_back();
}

float Sample::FloatRand(float lowerBound, float upperBound)
{
    if (lowerBound == upperBound)
        return lowerBound;

    std::uniform_real_distribution<float> dist(lowerBound, upperBound);

    return dist(m_randomEngine);
}

void Sample::PercentageStats()
{
    //  Calculate current frame's memory usage as a percentage of
    //  the memory that was needed for initialization. If this is greater than 100% it means
    //  more resources are being allocated at runtime. This is a good way to see if too many
    //  resources are being allocated during gameplay. If any of those allocations can be
    //  anticipated and made during initialization, this could improve performance during any
    //  portion of a game that the user is actively engaged in.
    float pfuComp = 100 * (float)m_frameMemoryUsage.PagefileUsage / (float)m_preRunMemoryUsage.PagefileUsage;
    float qnpComp = 100 * (float)m_frameMemoryUsage.QuotaNonPagedPoolUsage / (float)m_preRunMemoryUsage.QuotaNonPagedPoolUsage;
    float qppComp = 100 * (float)m_frameMemoryUsage.QuotaPagedPoolUsage / (float)m_preRunMemoryUsage.QuotaPagedPoolUsage;
    float wssComp = 100 * (float)m_frameMemoryUsage.WorkingSetSize / (float)m_preRunMemoryUsage.WorkingSetSize;

    swprintf_s(m_temporaryTextBuffer, L"Percentage of initial memory in use\n"
        L"PagefileUsage: %.2f%%\n"
        L"QuotaNonPagedPoolUsage: %.2f%%\n"
        L"QuotaPagedPoolUsage: %.2f%%\n"
        L"WorkingSetSize: %.2f%%\n\n",
        pfuComp,
        qnpComp,
        qppComp,
        wssComp);

    m_temporaryTextTime = 4.f;
}

#pragma endregion
