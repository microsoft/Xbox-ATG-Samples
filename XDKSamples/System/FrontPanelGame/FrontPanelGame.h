//--------------------------------------------------------------------------------------
// FrontPanelGame.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

#include "Game.h"

#include "FrontPanel/FrontPanelInput.h"
#include "FrontPanel/RasterFont.h"

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
public:

    Sample();

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

    void InitializeGame(bool alive);
    void RenderToFrontPanel() const;
    void UpdateGame();
    void RespondToInput();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>           m_deviceResources;
                                                   
    // Rendering loop timer.				       
    uint64_t                                       m_frame;
    DX::StepTimer                                  m_timer;
                                                   
    // DirectXTK objects.					       
    std::unique_ptr<DirectX::GraphicsMemory>            m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteBatch>               m_batch;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_background;
    // FrontPanel objects.
    Microsoft::WRL::ComPtr<IXboxFrontPanelControl> m_frontPanelControl;
    std::unique_ptr<ATG::FrontPanelDisplay>        m_frontPanelDisplay;
    std::unique_ptr<ATG::FrontPanelInput>          m_frontPanelInput;
    ATG::FrontPanelInput::ButtonStateTracker       m_frontPanelInputButtons;
    ATG::RasterFont                                m_font;

    // Game objects
    unsigned int                              m_score;
    bool                                      m_alive;
    std::shared_ptr<GameBoard>                m_gameBoard;
    std::shared_ptr<Snake>                    m_snake;

};
