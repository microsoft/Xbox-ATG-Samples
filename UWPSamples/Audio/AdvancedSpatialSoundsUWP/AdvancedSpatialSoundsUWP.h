//--------------------------------------------------------------------------------------
// AdvancedSpatialSoundsUWP.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "ISACRenderer.h"
#include "StepTimer.h"
#include "SpriteFont.h"
#include <vector>

#define MAX_CHANNELS 12 //up to 7.1.4 channels

enum TravelType {
    Linear = 0,
    Bounce,
    Round
};

struct TravelData
{
    TravelType travelType;
    float radius;
    float vel;
    DirectX::XMFLOAT3 direction;
    DirectX::BoundingBox boundingBox;
};

struct BedChannel
{
    char * wavBuffer;
    UINT32 bufferSize;
    float  volume;
    UINT32 curBufferLoc;
    Microsoft::WRL::ComPtr<ISpatialAudioObject> object;
    AudioObjectType objType;
};

struct PointSound
{
    char *  wavBuffer;
    UINT32  bufferSize;
    float   volume;
    UINT32  curBufferLoc;
    float   posX;
    float   posY;
    float   posZ;
    int soundIndex;
    Microsoft::WRL::ComPtr<ISpatialAudioObject> object;
    TravelData travelData;
    bool    isPlaying;
};

class Sample final : public DX::IDeviceNotify
{
public:
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

	Microsoft::WRL::ComPtr<ISACRenderer>    m_renderer;
	int                                     m_numChannels;
    BedChannel                              m_bedChannels[MAX_CHANNELS];
    std::vector<PointSound>                 m_pointSounds;
    bool	                                m_threadActive;
	bool	                                m_playingSound;
	int		                                m_availableObjects;
    int		                                m_usedObjects;
    SRWLOCK                                 SRWLock;

private:

	void Update(DX::StepTimer const& timer);

	void CreateDeviceDependentResources();
	void CreateWindowSizeDependentResources();

    void XM_CALLCONV DrawRoom(DirectX::FXMVECTOR color);
    void XM_CALLCONV DrawListener(DirectX::FXMVECTOR color);
    void XM_CALLCONV DrawSound(float x, float y, float z, DirectX::FXMVECTOR color);

    bool LoadBed();
    bool LoadPointFile(LPCWSTR inFile, PointSound* outChannel);

    void Sample::LinearTravel(PointSound* inSound);
    void Sample::BounceTravel(PointSound* inSound);
    void Sample::RoundTravel(PointSound* inSound);

	// Device resources.
	std::unique_ptr<DX::DeviceResources>    m_deviceResources;

	// Rendering loop timer.
	DX::StepTimer                           m_timer;
    
    bool                                    m_fileLoaded;
    DirectX::BoundingBox                    m_boundingBox;

    //worker thread for spatial system
    PTP_WORK m_workThread;

    std::unique_ptr<DirectX::SpriteBatch>   m_spriteBatch;
	std::unique_ptr<DirectX::SpriteFont>    m_font;
    std::unique_ptr<DirectX::SpriteFont>    m_ctrlFont;
    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_batch;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_batchInputLayout;
    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::BasicEffect>           m_batchEffect;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_circle;

	// Input devices.
    std::unique_ptr<DirectX::GamePad>       m_gamePad;
	std::unique_ptr<DirectX::Keyboard>      m_keyboard;

    DirectX::GamePad::ButtonStateTracker    m_gamePadButtons;
	DirectX::Keyboard::KeyboardStateTracker m_keyboardButtons;

    bool                                    m_ctrlConnected;
};