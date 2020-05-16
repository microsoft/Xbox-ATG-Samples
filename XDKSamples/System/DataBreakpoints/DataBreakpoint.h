//--------------------------------------------------------------------------------------
// DataBreakpoint.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "DataBreakpointTest.h"

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample() noexcept(false);
    ~Sample() = default;

    Sample(Sample&&) = default;
    Sample& operator= (Sample&&) = default;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic game loop
    void Tick();

    // Messages
    void OnSuspending();
    void OnResuming();

private:

    void Update(DX::StepTimer const& timer);
    void Render();
    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // Sample specific objects
    enum class TestStatus
    {
        TEST_NOT_RUN,
        TEST_SUCCESS,
        TEST_FAILURE,
    };
    DataBreakpointTest m_dataTest;
    TestStatus m_executionTestResult;
    TestStatus m_readTestResult;
    TestStatus m_readWriteTestResult;

    void DrawStatusString(const std::wstring& button, const std::wstring& testName, TestStatus status, DirectX::XMFLOAT2& pos);
    void DrawHelpText(DirectX::XMFLOAT2& pos, DataBreakpointTest::WhichTest);

    // DirectXTK objects.
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_background;
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>        m_font;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;
};
