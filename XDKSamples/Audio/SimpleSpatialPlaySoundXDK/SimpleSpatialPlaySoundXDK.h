//--------------------------------------------------------------------------------------
// SimpleSpatialPlaySoundXDK.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "SpriteFont.h"
#include "ISACRenderer.h"

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample
{
#define MAX_CHANNELS 12 //up to 7.1.4 channels

	struct Audiochannel
	{
		char * wavBuffer;
		UINT32 buffersize;
		float  volume;
		UINT32 curBufferLoc;

		Microsoft::WRL::ComPtr<ISpatialAudioObject> object;
		AudioObjectType		objType;

	};

public:

    Sample();

    // Initialization and management
    void Initialize(IUnknown* window);

    // Basic game loop
    void Tick();
    void Render();

    // Rendering helpers
    void Clear();

    // Messages
    void OnSuspending();
    void OnResuming();


	Microsoft::WRL::ComPtr<ISACRenderer>      m_Renderer;

	int		m_numChannels;
	Audiochannel WavChannels[MAX_CHANNELS];
	bool	m_bThreadActive;
	bool	m_bPlayingSound;
	int		m_availableObjects;
    
private:

    void Update(DX::StepTimer const& timer);

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

	bool LoadFile(LPCWSTR inFile);

	void SetChannelPosVolumes(void);
	HRESULT InitializeSpatialStream(void);

	// Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;
	std::unique_ptr<DirectX::SpriteBatch>		m_spriteBatch;
	std::unique_ptr<DirectX::SpriteFont>		m_font;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

	bool	m_fileLoaded;
	int		m_curFile;

	//worker thread for spatial system
	PTP_WORK m_workThread;

};
