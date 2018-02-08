//--------------------------------------------------------------------------------------
// SimpleSpatialPlaySoundUWP.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "ISACRenderer.h"
#include "StepTimer.h"
#include "SpriteFont.h"

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
class Sample : public DX::IDeviceNotify
{
public:

#define MAX_CHANNELS 12 //up to 7.1.4 channels

	struct Audiochannel
	{
		char *										wavBuffer;
		UINT32										buffersize;
		UINT32										curBufferLoc;
		float										volume;

		Microsoft::WRL::ComPtr<ISpatialAudioObject> object;
		AudioObjectType								objType;
	};

	Sample();

	// Initialization and management
	void Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

	// Basic render loop
	void Tick();
	void Render();

	// Rendering helpers
	void Clear();

	// IDeviceNotify
	virtual void OnDeviceLost() override;
	virtual void OnDeviceRestored() override;

	// Messages
	void OnActivated();
	void OnDeactivated();
	void OnSuspending();
	void OnResuming();
	void OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation);
	void ValidateDevice();

	// Properties
	void GetDefaultSize(int& width, int& height) const;


	Microsoft::WRL::ComPtr<ISACRenderer>		m_Renderer;
	int											m_numChannels;
	Audiochannel								m_WavChannels[MAX_CHANNELS];  
	bool										m_bThreadActive;
	bool										m_bPlayingSound;

private:

	void Update(DX::StepTimer const& timer);

	void CreateDeviceDependentResources();
	void CreateWindowSizeDependentResources();

	bool LoadFile(LPCWSTR inFile);
	void SetChannelTypesVolumes(void);

	HRESULT InitializeSpatialStream(void);

	// Device resources.
	std::unique_ptr<DX::DeviceResources>    m_deviceResources;

	// Rendering loop timer.
	DX::StepTimer                           m_timer;

	std::unique_ptr<DirectX::SpriteBatch>   m_spriteBatch;
	std::unique_ptr<DirectX::SpriteFont>    m_font;
    std::unique_ptr<DirectX::SpriteFont>    m_ctrlFont;

	// Input devices.
	std::unique_ptr<DirectX::Keyboard>      m_keyboard;
    std::unique_ptr<DirectX::GamePad>       m_gamePad;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
	DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;

    bool                                    m_ctrlConnected;

	uint32_t m_bufferSize;

	bool	m_fileLoaded;
	int		m_curFile;

	//worker thread for spatial system
	PTP_WORK m_workThread;
};