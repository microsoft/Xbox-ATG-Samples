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
#include <vector>
#include <mutex>

// A basic sample implementation that creates a D3D11 device and
// provides a render loop.
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

class Sample
{
#define MAX_CHANNELS 12 //up to 7.1.4 channels

	struct Audiochannel
	{
		char * wavBuffer;
		UINT32 buffersize;
		float  volume;
		UINT32 curBufferLoc;
		float  PosX;
		float  PosY;
		float  PosZ;

		Microsoft::WRL::ComPtr<ISpatialAudioObject> object;
		AudioObjectType		objType;

	};

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
	void Render();

	// Rendering helpers
	void Clear();

	// Messages
	void OnSuspending();
	void OnResuming();

	Microsoft::WRL::ComPtr<ISACRenderer>      m_Renderer;

	bool	m_bThreadActive;
	bool	m_bPlayingSound;

	BedChannel                              m_bedChannels[MAX_CHANNELS];
	std::vector<PointSound>                 m_pointSounds;
	int                                     m_numChannels;
	std::mutex                              MutexLock;
	int		                                m_availableObjects;
	int		                                m_usedObjects;

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

	DirectX::BoundingBox                    m_boundingBox;

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
    std::unique_ptr<DirectX::SpriteFont>		m_ctrlFont;

    std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_batch;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_batchInputLayout;
    std::unique_ptr<DirectX::CommonStates>          m_states;
    std::unique_ptr<DirectX::BasicEffect>           m_batchEffect;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_circle;

	// DirectXTK objects.
	std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;

	bool	m_fileLoaded;

	//worker thread for spatial system
	PTP_WORK m_workThread;
};
