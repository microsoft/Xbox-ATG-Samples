//--------------------------------------------------------------------------------------
// File: PerformanceTimersXbox.h
//
// Helpers for doing CPU & GPU performance timing and statitics
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once


namespace DX
{
    //----------------------------------------------------------------------------------
    // CPU performance timer
    class CPUTimer
    {
    public:
        static constexpr size_t c_maxTimers = 8;

        CPUTimer();

        CPUTimer(const CPUTimer&) = delete;
        CPUTimer& operator=(const CPUTimer&) = delete;

        CPUTimer(CPUTimer&&) = default;
        CPUTimer& operator=(CPUTimer&&) = default;

        // Start/stop a particular performance timer (don't start same index more than once in a single frame)
        void Start(uint32_t timerid = 0);
        void Stop(uint32_t timerid = 0);

        // Should Update once per frame to compute timer results
        void Update();

        // Reset running average
        void Reset();

        // Returns delta time in milliseconds
        double GetElapsedMS(uint32_t timerid = 0) const;

        // Returns running average in milliseconds
        float GetAverageMS(uint32_t timerid = 0) const
        {
            return (timerid < c_maxTimers) ? m_avg[timerid] : 0.f;
        }

    private:
        double          m_qpfFreqInv;
        LARGE_INTEGER   m_start[c_maxTimers];
        LARGE_INTEGER   m_end[c_maxTimers];
        float           m_avg[c_maxTimers];
    };


#if defined(__d3d12_x_h__) || defined(__XBOX_D3D12_X__)
    //----------------------------------------------------------------------------------
    // DirectX 12.X implementation of GPU timer
    template<typename t_CommandList = ID3D12CommandList>
    class GPUCommandListTimer
    {
        typedef uint64_t Timestamp;

        static constexpr Timestamp c_invalidTimestamp = 0;

    public:
        static constexpr size_t c_maxTimers = 8;

        GPUCommandListTimer() :
            m_currentFrame(0),
            m_frame{}
        {}

        GPUCommandListTimer(ID3D12Device* device, ID3D12CommandQueue* commandQueue) :
            m_currentFrame(0),
            m_frame{}
        {
            RestoreDevice(device, commandQueue);
        }

        GPUCommandListTimer(const GPUCommandListTimer&) = delete;
        GPUCommandListTimer& operator=(const GPUCommandListTimer&) = delete;

        GPUCommandListTimer(GPUCommandListTimer&&) = default;
        GPUCommandListTimer& operator=(GPUCommandListTimer&&) = default;

        ~GPUCommandListTimer() { ReleaseDevice(); }

        // Indicate beginning & end of frame
        void BeginFrame(_In_ t_CommandList* commandList);

        void EndFrame(_In_ t_CommandList* commandList);

        // Allows GetElapsedMS to retrieve the most recently submitted timings, instead of the oldest ones
        // Requires the caller to block until the data is available
        void Flush(_In_ t_CommandList* commandList);

        // Start/stop a particular performance timer (don't start same index more than once in a single frame)
        void Start(_In_ t_CommandList* commandList, uint32_t timerid = 0);
        void Stop(_In_ t_CommandList* commandList, uint32_t timerid = 0);

        // Reset running average
        void Reset();

        // Returns delta time in milliseconds
        double GetElapsedMS(uint32_t timerid = 0) const;

        // Returns running average in milliseconds
        float GetAverageMS(uint32_t timerid = 0) const;

        // Device management
        void ReleaseDevice();
        void RestoreDevice(_In_ ID3D12Device* device, _In_ ID3D12CommandQueue* commandQueue);

    private:
        static constexpr size_t c_bufferCount = 3;

        unsigned m_currentFrame;
        struct Frame
        {
            bool                                m_pending;
            Timestamp*                          m_start[c_maxTimers];
            Timestamp*                          m_end[c_maxTimers];
            float                               m_avg[c_maxTimers];
            float                               m_timing[c_maxTimers];
            bool                                m_used[c_maxTimers];

            Frame() :
                m_pending(false),
                m_avg{},
                m_timing{},
                m_used{}
            {}

            void BeginFrame(_In_ t_CommandList* commandList);
            void EndFrame(_In_ t_CommandList* commandList);
            void ComputeFrame(_In_ t_CommandList* commandList);

            void Start(_In_ t_CommandList* commandList, uint32_t timerid = 0);
            void Stop(_In_ t_CommandList* commandList, uint32_t timerid = 0);
        } m_frame[c_bufferCount];
    };

    typedef GPUCommandListTimer<ID3D12GraphicsCommandList> GPUTimer;
    typedef GPUCommandListTimer<ID3D12XboxDmaCommandList> GPUDmaTimer;
    typedef GPUCommandListTimer<ID3D12GraphicsCommandList> GPUComputeTimer;

#elif defined(__d3d11_x_h__)
    //----------------------------------------------------------------------------------
    // DirectX 11.X implementation of GPU timer
    template<typename t_Context = ID3D11DeviceContextX>
    class GPUContextTimer
    {
        typedef uint64_t Timestamp;

        static constexpr Timestamp c_invalidTimestamp = 0;

    public:
        static constexpr size_t c_maxTimers = 8;

        GPUContextTimer() :
            m_currentFrame(0),
            m_frame{}
        {}

        GPUContextTimer(ID3D11DeviceX* device) :
            m_currentFrame(0),
            m_frame{}
        {
            RestoreDevice(device);
        }

        GPUContextTimer(const GPUContextTimer&) = delete;
        GPUContextTimer& operator=(const GPUContextTimer&) = delete;

        GPUContextTimer(GPUContextTimer&&) = default;
        GPUContextTimer& operator=(GPUContextTimer&&) = default;

        ~GPUContextTimer() { ReleaseDevice(); }

        // Indicate beginning & end of frame
        void BeginFrame(_In_ t_Context* context);

        void EndFrame(_In_ t_Context* context);

        // Allows GetElapsedMS to retrieve the most recently submitted timings, instead of the oldest ones
        // Requires the caller to block until the data is available
        void Flush(_In_ t_Context* context);

        // Start/stop a particular performance timer (don't start same index more than once in a single frame)
        void Start(_In_ t_Context* context, uint32_t timerid = 0);
        void Stop(_In_ t_Context* context, uint32_t timerid = 0);

        // Reset running average
        void Reset();

        // Returns delta time in milliseconds
        double GetElapsedMS(uint32_t timerid = 0) const;

        // Returns running average in milliseconds
        float GetAverageMS(uint32_t timerid = 0) const;

        // Device management
        void ReleaseDevice();
        void RestoreDevice(_In_ ID3D11DeviceX* device);

    private:
        static constexpr size_t c_bufferCount = 3;

        unsigned m_currentFrame;
        struct Frame
        {
            bool                                m_pending;
            Timestamp*                          m_start[c_maxTimers];
            Timestamp*                          m_end[c_maxTimers];
            float                               m_avg[c_maxTimers];
            float                               m_timing[c_maxTimers];
            bool                                m_used[c_maxTimers];

            Frame() :
                m_pending(false),
                m_avg{},
                m_timing{},
                m_used{}
            {}

            void BeginFrame(_In_ t_Context* context);
            void EndFrame(_In_ t_Context* context);
            void ComputeFrame(_In_ t_Context* context);

            void Start(_In_ t_Context* context, uint32_t timerid = 0);
            void Stop(_In_ t_Context* context, uint32_t timerid = 0);

            void Flush(_In_ t_Context* context) { context->Flush(); }
        } m_frame[c_bufferCount];
    };

    typedef GPUContextTimer<ID3D11DeviceContextX> GPUTimer;
    typedef GPUContextTimer<ID3D11DmaEngineContextX> GPUDmaTimer;
    typedef GPUContextTimer<ID3D11ComputeContextX> GPUComputeTimer;

    // Naming inconsistency --- graphics and async compute have Flush() while dma has Submit()
    template<>
    void GPUContextTimer<ID3D11DmaEngineContextX>::Frame::Flush(_In_ ID3D11DmaEngineContextX* context) { context->Submit();  }
#else
#error Must include d3d11*.h or d3d12*.h before PerformanceTimersXbox.h
#endif
}
