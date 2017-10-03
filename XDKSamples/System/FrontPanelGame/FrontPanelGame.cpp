//--------------------------------------------------------------------------------------
// FrontPanelGame.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "FrontPanelGame.h"

#include "ATGColors.h"

#include "FrontPanel\CPUShapes.h"

extern void ExitSample();

using namespace DirectX;
using namespace ATG;
using ButtonState = FrontPanelInput::ButtonStateTracker::ButtonState;

using Microsoft::WRL::ComPtr;

Sample::Sample()
    : m_frame(0)
    , m_score(0)
    , m_alive(false)
{
    // Renders only 2D, so no need for a depth buffer.
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN);

    // Check to see whethter there is a front panel
    if (IsXboxFrontPanelAvailable())
    {
        // Get the default front panel
        DX::ThrowIfFailed(GetDefaultXboxFrontPanel(m_frontPanelControl.ReleaseAndGetAddressOf()));

        // Initialize the FrontPanelDisplay object
        m_frontPanelDisplay = std::make_unique<FrontPanelDisplay>(m_frontPanelControl.Get());

        // Initialize the FrontPanelInput object
        m_frontPanelInput = std::make_unique<FrontPanelInput>(m_frontPanelControl.Get());
    }
}

void Sample::InitializeGame(bool alive)
{
    m_alive = alive;

    m_score = 0;

    m_gameBoard = std::make_shared<GameBoard>(*m_frontPanelDisplay.get());

    if (m_alive)
    {
        m_snake = std::make_shared<Snake>(
            m_gameBoard,
            0, 1,
            5, 5);

        m_gameBoard->SpawnFood();
    }
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window)
{
    m_deviceResources->SetWindow(window);

    m_deviceResources->CreateDeviceResources();  
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // Using fixed frame rate with 60fps target frame rate
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60.0);

    // Don't do anything if there is no front panel
    if (m_frontPanelControl)
    {
        m_font = RasterFont(L"assets\\LucidaConsole12.rasterfont");

        InitializeGame(false);
    }
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

    // Don't do anything if there is no front panel
    if (m_frontPanelControl)
    {
        UpdateGame();

        RenderToFrontPanel();
    }

    PIXEndEvent();
}

void Sample::RenderToFrontPanel() const
{
    m_frontPanelDisplay->Clear();
    
    BufferDesc fpDesc = m_frontPanelDisplay->GetBufferDescriptor();
    m_font.DrawStringFmt(fpDesc, 0, 0, L"\nSnake\n\nScore: %u", m_score);

    unsigned int ts = static_cast<unsigned int>(m_timer.GetTotalSeconds());

    CPUShapes shapes = CPUShapes(m_frontPanelDisplay->GetDisplayWidth(), m_frontPanelDisplay->GetDisplayHeight(), m_frontPanelDisplay->GetBuffer());
    shapes.RenderRect(0 + ts % 11, 0, 2, 2);

    m_gameBoard->Render();

    m_frontPanelDisplay->Present();
}

void Sample::UpdateGame()
{
    RespondToInput();

    // Each frame is 1/60th of a second
    static unsigned frames = 0;

    // advancing the simulation every 8 frames seems to the right amount
    // given the size of the dpad control etc.
    if (m_alive && (frames > 8)) 
    {
        switch (m_snake->Move())
        {
        case SnakeMoveResult::Move:
            break;

        case SnakeMoveResult::Eat:
            m_gameBoard->SpawnFood();
            m_score++;
            break;

        case SnakeMoveResult::Fail:
            m_alive = false;
            break;
        }

        frames = 0;
    }
    else if (!m_alive && (frames > 20))
    {
        // Blink the light around the start button
        auto state = m_frontPanelInput->GetState();
        auto lights = state.lights.rawLights;
        
        if (state.lights.light1)
        {
            lights &= ~XBOX_FRONT_PANEL_LIGHTS_LIGHT1;
        }
        else
        {
            lights |= XBOX_FRONT_PANEL_LIGHTS_LIGHT1;
        }
        m_frontPanelInput->SetLightStates(static_cast<XBOX_FRONT_PANEL_LIGHTS>(lights));

        frames = 0;
    }

    ++frames;
}

void Sample::RespondToInput()
{

    auto fpInput = m_frontPanelInput->GetState();
    m_frontPanelInputButtons.Update(fpInput);

    if(m_snake && m_frontPanelInputButtons.dpadUp == ButtonState::PRESSED)
    {
        m_snake->SetDirection(0, -1);
    }
    else if (m_snake && m_frontPanelInputButtons.dpadDown == ButtonState::PRESSED)
    {
        m_snake->SetDirection(0, 1);
    }
    else if(m_snake && m_frontPanelInputButtons.dpadLeft == ButtonState::PRESSED)
    {
        m_snake->SetDirection(-1, 0);
    }
    else if(m_snake && m_frontPanelInputButtons.dpadRight == ButtonState::PRESSED)
    {
        m_snake->SetDirection(1, 0);
    }
    else if(m_frontPanelInputButtons.button1 == ButtonState::PRESSED)
    {
        InitializeGame(true);
    }

    // Use the select button to take a screen capture
    if (m_frontPanelInputButtons.buttonSelect == ButtonState::PRESSED)
    {
        m_frontPanelDisplay->SaveDDSToFile(L"D:\\FrontPanelDisplay.dds");
    }

    if (m_frontPanelInputButtons.buttonsChanged)
    {
        XBOX_FRONT_PANEL_LIGHTS lights = XBOX_FRONT_PANEL_LIGHTS_NONE;

        if(m_frontPanelInputButtons.button1 & ButtonState::HELD)
        {
            lights = static_cast<XBOX_FRONT_PANEL_LIGHTS>(lights | XBOX_FRONT_PANEL_LIGHTS_LIGHT1);
        }
        if (m_frontPanelInputButtons.button2 & ButtonState::HELD)
        {
            lights = static_cast<XBOX_FRONT_PANEL_LIGHTS>(lights | XBOX_FRONT_PANEL_LIGHTS_LIGHT2);
        }
        if (m_frontPanelInputButtons.button3 & ButtonState::HELD)
        {
            lights = static_cast<XBOX_FRONT_PANEL_LIGHTS>(lights | XBOX_FRONT_PANEL_LIGHTS_LIGHT3);
        }
        if (m_frontPanelInputButtons.button4 & ButtonState::HELD)
        {
            lights = static_cast<XBOX_FRONT_PANEL_LIGHTS>(lights | XBOX_FRONT_PANEL_LIGHTS_LIGHT4);
        }
        if (m_frontPanelInputButtons.button5 & ButtonState::HELD)
        {
            lights = static_cast<XBOX_FRONT_PANEL_LIGHTS>(lights | XBOX_FRONT_PANEL_LIGHTS_LIGHT5);
        }

        m_frontPanelInput->SetLightStates(lights);
    }
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

    auto output = m_deviceResources->GetOutputSize();

    auto safe = SimpleMath::Viewport::ComputeTitleSafeArea(output.right, output.bottom);

    m_batch->Begin();
    m_batch->Draw(m_background.Get(), output);
    m_batch->End();

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
    auto device = m_deviceResources->GetD3DDevice();
    m_graphicsMemory = std::make_unique<GraphicsMemory>(device, m_deviceResources->GetBackBufferCount());

    auto context = m_deviceResources->GetD3DDeviceContext();
    m_batch = std::make_unique<SpriteBatch>(context);

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device,
            IsXboxFrontPanelAvailable() ? L"FrontPanelPresent.png" : L"NoFrontPanel.png",
            nullptr, m_background.ReleaseAndGetAddressOf())
    );
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
}
#pragma endregion
