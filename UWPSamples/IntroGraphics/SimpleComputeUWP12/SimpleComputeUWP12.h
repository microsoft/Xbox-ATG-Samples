//--------------------------------------------------------------------------------------
// SimpleComputeUWP12.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "ControllerHelp.h"

class SmoothedFPS
{
public:
    SmoothedFPS(uint32_t frameInterval = 100)
    {
        Initialize(frameInterval);
    }

    void Initialize(uint32_t frameInterval = 100)
    {
        m_frameInterval = frameInterval;
        m_timeAccumulator = 0.0f;
        m_frameAccumulator = 0;
        m_smoothedFPS = 0.0f;
    }

    void Tick(float DeltaTime)
    {
        m_timeAccumulator += DeltaTime;
        ++m_frameAccumulator;

        if (m_frameAccumulator >= m_frameInterval)
        {
            m_smoothedFPS = (float)m_frameInterval / m_timeAccumulator;
            m_timeAccumulator = 0.0f;
            m_frameAccumulator = 0;
        }
    }

    float GetFPS() const { return m_smoothedFPS; }

private:
    float m_smoothedFPS;
    float m_timeAccumulator;
    uint32_t m_frameAccumulator;
    uint32_t m_frameInterval;
};

// A basic sample implementation that creates a D3D12 device and
// provides a render loop.
class Sample final : public DX::IDeviceNotify
{
public:

    Sample() noexcept(false);
    ~Sample() = default;

    Sample(Sample&&) = delete;
    Sample& operator= (Sample&&) = delete;

    Sample(Sample const&) = delete;
    Sample& operator= (Sample const&) = delete;

    // Initialization and management
    void Initialize(::IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation);

    // Basic render loop
    void Tick();

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
    void GetDefaultSize( int& width, int& height ) const;

private:
    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void ResetWindow();

    void UpdateFractalData();
        // called by compute side of work, updates the window parameters

    bool EnsureResourceState(_In_range_(0, 1) uint32_t index, D3D12_RESOURCE_STATES afterState);

    void AsyncComputeThreadProc();

    uint32_t ComputeIndex()  const { return 1 - m_renderIndex; }
    uint32_t RenderIndex() const { return m_renderIndex; }
    void SwapRenderComputeIndex() { m_renderIndex = 1 - m_renderIndex; }

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                               m_timer;

    // on-screen help
    std::unique_ptr<ATG::Help>                  m_help;
    bool                                        m_showHelp;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;
    std::unique_ptr<DirectX::Keyboard>          m_keyboard;
    std::unique_ptr<DirectX::Mouse>             m_mouse;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;
    DirectX::Keyboard::KeyboardStateTracker     m_keyboardButtons;
    bool                                        m_gamepadPresent;

    // Compute data
    uint32_t                                    m_ThreadGroupX;
    uint32_t                                    m_ThreadGroupY;

    SmoothedFPS                                 m_renderFPS;
    SmoothedFPS                                 m_computeFPS;

    std::atomic<bool>                           m_terminateThread;
    std::atomic<bool>                           m_suspendThread;
    std::thread *                               m_computeThread;

    std::atomic<uint32_t>                       m_renderIndex;          // which bank of fractal data the renderer is using, the value is either 0 or 1, compute is using the inverse

    DirectX::XMFLOAT4                           m_window;               // the bounds for the Mandelbrot set being calculated on the CPU
    uint32_t                                    m_fractalMaxIterations; // number of iterations when calculating the Mandelbrot set on the CPU
    std::atomic<bool>							m_windowUpdated;

    // shared data
    std::unique_ptr<DirectX::DescriptorHeap>    m_SRVDescriptorHeap;    // shader resource views for the fractal texture and data
    std::unique_ptr<DirectX::DescriptorHeap>    m_samplerDescriptorHeap;// shader resource views for the samplers used by the compute shader
    DirectX::SharedGraphicsResource             m_renderHeap;           // renderer version of the fractal constant data
    DirectX::SharedGraphicsResource             m_computeHeap;          // async compute version of the fractal constant data

    // fractal texture data
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_fractalColorMap[2];   // the textures used by the sampler for coloring the computed fractal, one for sync, one for async
    Microsoft::WRL::ComPtr<ID3D12Resource>      m_fractalTexture[2];    // the actual texture generated by the compute shader, double buffered, async and render operating on opposite textures
    D3D12_RESOURCE_STATES                       m_resourceStateFractalTexture[2];   // current state of the fractal texture, unordered or texture view
    Microsoft::WRL::ComPtr<ID3D12Fence>         m_renderResourceFence;  // fence used by async compute to start once it's texture has changed to unordered access
    uint64_t                                    m_renderResourceFenceValue;

    // compute data
    std::atomic<bool>                           m_usingAsyncCompute;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_computePSO;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_computeRootSignature;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_computeAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>  m_computeCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;

    Microsoft::WRL::ComPtr<ID3D12Fence>         m_computeFence; // fence used by the async compute shader to stall waiting for task is complete, so it can signal render when it's done
    uint64_t                                    m_computeFenceValue;
    Microsoft::WRL::Wrappers::Event             m_computeFenceEvent;

    Microsoft::WRL::Wrappers::Event             m_computeResumeSignal;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::DescriptorHeap>    m_resourceDescriptors;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>        m_font;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;

    enum Descriptors
    {
        TextFont,
        ControllerFont,
        Count
    };

    enum ResourceBufferState : uint32_t
    {
        ResourceState_ReadyCompute,
        ResourceState_Computing,    // async is currently running on this resource buffer
        ResourceState_Computed,     // async buffer has been updated, no one is using it, moved to this state by async thread, only render will access in this state
        ResourceState_Switching,    // switching buffer from texture to unordered, from render to compute access
        ResourceState_Rendering,    // buffer is currently being used by the render system for the frame
        ResourceState_Rendered      // render frame finished for this resource. possible to switch to computing by render thread if needed
    };

    std::atomic<ResourceBufferState> m_resourceState[2];

    // Indexes for the root parameter table
    enum RootParameters : uint32_t
    {
        e_rootParameterCB = 0,
        e_rootParameterSampler,
        e_rootParameterSRV,
        e_rootParameterUAV,
        e_numRootParameters
    };

    // indexes of resources into the descriptor heap
    enum DescriptorHeapCount : uint32_t
    {
        e_cCB = 10,
        e_cUAV = 2,
        e_cSRV = 4,
    };
    enum DescriptorHeapIndex : uint32_t
    {
        e_iCB = 0,
        e_iUAV = e_iCB + e_cCB,
        e_iSRV = e_iUAV + e_cUAV,
        e_iHeapEnd = e_iSRV + e_cSRV
    };
};
