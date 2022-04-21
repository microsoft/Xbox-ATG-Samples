//--------------------------------------------------------------------------------------
// File: PerformanceTimers.cpp
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "pch.h"
#include "PerformanceTimersXbox.h"

#include "DirectXHelpers.h"

#include <atomic>
#include <exception>
#include <stdexcept>
#include <tuple>

using namespace DirectX;
using namespace DX;

using Microsoft::WRL::ComPtr;

namespace
{
    inline float lerp(float a, float b, float f)
    {
        return (1.f - f) * a + f * b;
    }

    inline float UpdateRunningAverage(float avg, float value)
    {
        return lerp(value, avg, 0.95f);
    }

    inline void DebugWarnings(uint32_t timerid, uint64_t start, uint64_t end)
    {
#if defined(_DEBUG)
        if (!start && end > 0)
        {
            char buff[128] = {};
            sprintf_s(buff, "ERROR: Timer %u stopped but not started\n", timerid);
            OutputDebugStringA(buff);
        }
        else if (start > 0 && !end)
        {
            char buff[128] = {};
            sprintf_s(buff, "ERROR: Timer %u started but not stopped\n", timerid);
            OutputDebugStringA(buff);
        }
#else
        UNREFERENCED_PARAMETER(timerid);
        UNREFERENCED_PARAMETER(start);
        UNREFERENCED_PARAMETER(end);
#endif
    }
};

//======================================================================================
// CPUTimer
//======================================================================================

CPUTimer::CPUTimer() :
    m_qpfFreqInv(1.f),
    m_start{},
    m_end{},
    m_avg{}
{
    LARGE_INTEGER qpfFreq;
    if (!QueryPerformanceFrequency(&qpfFreq))
    {
        throw std::exception("QueryPerformanceFrequency");
    }

    m_qpfFreqInv = 1000.0 / double(qpfFreq.QuadPart);
}

void CPUTimer::Start(uint32_t timerid)
{
    if (timerid >= c_maxTimers)
        throw std::out_of_range("Timer ID out of range");

    if (!QueryPerformanceCounter(&m_start[timerid]))
    {
        throw std::exception("QueryPerformanceCounter");
    }
}

void CPUTimer::Stop(uint32_t timerid)
{
    if (timerid >= c_maxTimers)
        throw std::out_of_range("Timer ID out of range");

    if (!QueryPerformanceCounter(&m_end[timerid]))
    {
        throw std::exception("QueryPerformanceCounter");
    }
}

void CPUTimer::Update()
{
    for (uint32_t j = 0; j < c_maxTimers; ++j)
    {
        auto start = static_cast<uint64_t>(m_start[j].QuadPart);
        auto end = static_cast<uint64_t>(m_end[j].QuadPart);

        DebugWarnings(j, start, end);

        float value = float(double(end - start) * m_qpfFreqInv);
        m_avg[j] = UpdateRunningAverage(m_avg[j], value);
    }
}

void CPUTimer::Reset()
{
    memset(m_avg, 0, sizeof(m_avg));
}

double CPUTimer::GetElapsedMS(uint32_t timerid) const
{
    if (timerid >= c_maxTimers)
        return 0.0;

    auto start = static_cast<uint64_t>(m_start[timerid].QuadPart);
    auto end = static_cast<uint64_t>(m_end[timerid].QuadPart);

    return double(end - start) * m_qpfFreqInv;
}


//======================================================================================
// A thread-safe block allocator (no free) for timestamps
//======================================================================================
template<typename t_Type>
class TimestampAllocator
{
public:
    TimestampAllocator(SIZE_T size) :
        m_size(size), 
        m_index(0)
    {
        m_memory = static_cast<t_Type*>(VirtualAlloc(nullptr,
            size,
            MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES | MEM_GRAPHICS,
            PAGE_READWRITE | PAGE_WRITECOMBINE));

        if (!m_memory)
        {
            DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        memset(m_memory, 0, size);
    }

    virtual ~TimestampAllocator() noexcept
    {
        if (m_memory)
        {
            std::ignore = VirtualFree(m_memory, m_size, MEM_RELEASE);
            m_memory = nullptr;
        }
    }

    t_Type* GetNext()
    {
        auto next = m_memory + m_index++;

        auto count = m_size / sizeof(t_Type);
        if (next >= m_memory + count)
        {
            throw std::out_of_range("Too many timers requested");
        }
        else
        {
            return next;
        }
    }

private:
    t_Type* m_memory;
    const size_t m_size;
    std::atomic<uint32_t> m_index;
};


//======================================================================================
// GPUTimer (DirectX 12.X)
//======================================================================================
#if defined(__d3d12_x_h__) || defined(__XBOX_D3D12_X__)

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::BeginFrame(_In_ t_CommandList* commandList)
{
    m_frame[m_currentFrame].BeginFrame(commandList);
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::EndFrame(_In_ t_CommandList* commandList)
{
    m_frame[m_currentFrame].EndFrame(commandList);
    m_frame[m_currentFrame].m_pending = true;
    m_currentFrame = (m_currentFrame + 1) % c_bufferCount;

    if (m_frame[m_currentFrame].m_pending)
    {
        m_frame[m_currentFrame].ComputeFrame(commandList);
        m_frame[m_currentFrame].m_pending = false;
    }
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::Flush(_In_ t_CommandList* commandList)
{
    for (size_t frame = 0; frame < 2 * (c_bufferCount - 1); ++frame)
    {
        BeginFrame(commandList);
        EndFrame(commandList);
    }
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::Start(_In_ t_CommandList* commandList, uint32_t timerid)
{
    m_frame[m_currentFrame].Start(commandList, timerid);
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::Stop(_In_ t_CommandList* commandList, uint32_t timerid)
{
    m_frame[m_currentFrame].Stop(commandList, timerid);
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::Reset()
{
    for (size_t i = 0; i < c_bufferCount; ++i)
    {
        memset(m_frame[i].m_avg, 0, sizeof(Frame::m_avg));
    }
}

template<typename t_CommandList>
double GPUCommandListTimer<t_CommandList>::GetElapsedMS(uint32_t timerid) const
{
    if (timerid >= c_maxTimers)
        return 0.0;

    uint32_t oldestBuffer = (m_currentFrame + 1) % c_bufferCount;
    return m_frame[oldestBuffer].m_timing[timerid];
}

template<typename t_CommandList>
float GPUCommandListTimer<t_CommandList>::GetAverageMS(uint32_t timerid) const
{
    if (timerid >= c_maxTimers)
        return 0.f;

    uint32_t oldestBuffer = (m_currentFrame + 1) % c_bufferCount;
    return m_frame[oldestBuffer].m_avg[timerid];
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::ReleaseDevice()
{
    for (size_t i = 0; i < c_bufferCount; ++i)
    {
        for (size_t j = 0; j < c_maxTimers; ++j)
        {
            *m_frame[i].m_start[j] = c_invalidTimestamp;
            *m_frame[i].m_end[j] = c_invalidTimestamp;
        }
    }
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::RestoreDevice(_In_ ID3D12Device* /*device*/, _In_ ID3D12CommandQueue* /*commandQueue*/)
{
    // Dma context requires 32-byte alignment for writebacks
    constexpr uint32_t dmaWritebackAlignment = 32;
    constexpr uint32_t alignmentPadding = dmaWritebackAlignment / sizeof(Timestamp);
    typedef uint64_t PaddedTimestamp[alignmentPadding];

    // One large page supports a total of 2048 timestamps, which is enough for 85 timers
    const uint32_t largePageSize = 64 * 1024;

    // Allocate Timestamp memory --- a single page shared among all timers of this type
    static __declspec(align(dmaWritebackAlignment)) TimestampAllocator<PaddedTimestamp> Allocator(largePageSize);

    for (size_t i = 0; i < c_bufferCount; ++i)
    {
        for (size_t j = 0; j < c_maxTimers; ++j)
        {
            m_frame[i].m_start[j] = *Allocator.GetNext();
            m_frame[i].m_end[j] = *Allocator.GetNext();
        }
    }
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::Frame::BeginFrame(_In_ t_CommandList* /*commandList*/)
{
    for (size_t j = 0; j < c_maxTimers; ++j)
    {
        if (m_used[j])
        {
            throw std::overflow_error("Timer reset while still in use");
        }

        *m_start[j] = c_invalidTimestamp;
        *m_end[j] = c_invalidTimestamp;
    }
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::Frame::EndFrame(_In_ t_CommandList* /*commandList*/)
{
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::Frame::ComputeFrame(_In_ t_CommandList* commandList)
{
#if defined(_DEBUG) || defined(PROFILE)
    bool blocked = false;
#endif

    for (uint32_t j = 0; j < c_maxTimers; ++j)
    {
        if (!m_used[j])
        {
            m_timing[j] = 0.f;
            continue;
        }

        while (c_invalidTimestamp == *m_start[j])
        {
#if defined(_DEBUG) || defined(PROFILE)
            if (!blocked)
            {
                OutputDebugStringA("WARNING: Blocked performance queries\n");
                blocked = true;
            }
#endif
            commandList->KickoffX();
            Sleep(1);
        }

        while (c_invalidTimestamp == *m_end[j])
        {
#if defined(_DEBUG) || defined(PROFILE)
            if (!blocked)
            {
                OutputDebugStringA("WARNING: Blocked performance queries\n");
                blocked = true;
            }
#endif
            commandList->KickoffX();
            Sleep(1);
        }

        uint64_t start = *m_start[j];
        uint64_t end = *m_end[j];

        DebugWarnings(j, start, end);

        constexpr double c_FrequencyInvMS = 1000.0 / double(D3D11X_XBOX_GPU_TIMESTAMP_FREQUENCY);
        float value = float(double(end - start) * c_FrequencyInvMS);
        m_timing[j] = value;
        m_avg[j] = UpdateRunningAverage(m_avg[j], value);
        m_used[j] = false;
    }
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::Frame::Start(_In_ t_CommandList* commandList, uint32_t timerid)
{
    if (timerid >= c_maxTimers)
        throw std::out_of_range("Timer ID out of range");

    m_used[timerid] = true;

    commandList->Write64BitValueBottomOfPipeX((D3D12_GPU_VIRTUAL_ADDRESS)m_start[timerid], 0, D3D12XBOX_FLUSH_NONE, D3D12XBOX_WRITE_VALUE_BOP_FLAG_GPU_TIMESTAMP);
}

template<typename t_CommandList>
void GPUCommandListTimer<t_CommandList>::Frame::Stop(_In_ t_CommandList* commandList, uint32_t timerid)
{
    if (timerid >= c_maxTimers)
        throw std::out_of_range("Timer ID out of range");

    commandList->Write64BitValueBottomOfPipeX((D3D12_GPU_VIRTUAL_ADDRESS)m_end[timerid], 0, D3D12XBOX_FLUSH_NONE, D3D12XBOX_WRITE_VALUE_BOP_FLAG_GPU_TIMESTAMP);
}

// Force compilation of these template instantiations
namespace DX
{
    template class GPUCommandListTimer<ID3D12GraphicsCommandList>;
    template class GPUCommandListTimer<ID3D12XboxDmaCommandList>;
}


//======================================================================================
// GPUTimer (DirectX 11.X)
//======================================================================================
#elif defined(__d3d11_x_h__)

template<typename t_Context>
void GPUContextTimer<t_Context>::BeginFrame(_In_ t_Context* context)
{
    m_frame[m_currentFrame].BeginFrame(context);
}

template<typename t_Context>
void GPUContextTimer<t_Context>::EndFrame(_In_ t_Context* context)
{
    m_frame[m_currentFrame].EndFrame(context);
    m_frame[m_currentFrame].m_pending = true;
    m_currentFrame = (m_currentFrame + 1) % c_bufferCount;

    if (m_frame[m_currentFrame].m_pending)
    {
        m_frame[m_currentFrame].ComputeFrame(context);
        m_frame[m_currentFrame].m_pending = false;
    }
}

template<typename t_Context>
void GPUContextTimer<t_Context>::Flush(_In_ t_Context* context)
{
    for (size_t frame = 0; frame < 2 * (c_bufferCount - 1); ++frame)
    {
        BeginFrame(context);
        EndFrame(context);
    }
}

template<typename t_Context>
void GPUContextTimer<t_Context>::Start(_In_ t_Context* context, uint32_t timerid)
{
    m_frame[m_currentFrame].Start(context, timerid);
}

template<typename t_Context>
void GPUContextTimer<t_Context>::Stop(_In_ t_Context* context, uint32_t timerid)
{
    m_frame[m_currentFrame].Stop(context, timerid);
}

template<typename t_Context>
void GPUContextTimer<t_Context>::Reset()
{
    for (size_t i = 0; i < c_bufferCount; ++i)
    {
        memset(m_frame[i].m_avg, 0, sizeof(Frame::m_avg));
    }
}

template<typename t_Context>
double GPUContextTimer<t_Context>::GetElapsedMS(uint32_t timerid) const
{
    if (timerid >= c_maxTimers)
        return 0.0;

    uint32_t oldestBuffer = (m_currentFrame + 1) % c_bufferCount;
    return m_frame[oldestBuffer].m_timing[timerid];
}

template<typename t_Context>
float GPUContextTimer<t_Context>::GetAverageMS(uint32_t timerid) const
{
    if (timerid >= c_maxTimers)
        return 0.f;

    uint32_t oldestBuffer = (m_currentFrame + 1) % c_bufferCount;
    return m_frame[oldestBuffer].m_avg[timerid];
}

template<typename t_Context>
void GPUContextTimer<t_Context>::ReleaseDevice()
{
    for (size_t i = 0; i < c_bufferCount; ++i)
    {
        for (size_t j = 0; j < c_maxTimers; ++j)
        {
            *m_frame[i].m_start[j] = c_invalidTimestamp;
            *m_frame[i].m_end[j] = c_invalidTimestamp;
        }
    }
}

template<typename t_Context>
void GPUContextTimer<t_Context>::RestoreDevice(_In_ ID3D11DeviceX* /*device*/)
{
    // Dma context requires 32-byte alignment for writebacks
    constexpr uint32_t dmaWritebackAlignment = 32;
    constexpr uint32_t alignmentPadding = dmaWritebackAlignment / sizeof(Timestamp);
    typedef __declspec(align(dmaWritebackAlignment)) uint64_t PaddedTimestamp[alignmentPadding];

    // One large page supports a total of 2048 timestamps, which is enough for 85 timers
    const uint32_t largePageSize = 64 * 1024;

    // Allocate Timestamp memory --- a single page shared among all timers of this type
    static TimestampAllocator<PaddedTimestamp> Allocator(largePageSize);

    for (size_t i = 0; i < c_bufferCount; ++i)
    {
        for (size_t j = 0; j < c_maxTimers; ++j)
        {
            m_frame[i].m_start[j] = *Allocator.GetNext();
            m_frame[i].m_end[j] = *Allocator.GetNext();
        }
    }
}

template<typename t_Context>
void GPUContextTimer<t_Context>::Frame::BeginFrame(_In_ t_Context* /*context*/)
{
    for (size_t j = 0; j < c_maxTimers; ++j)
    {
        if (m_used[j])
        {
            throw std::overflow_error("Timer reset while still in use");
        }

        *m_start[j] = c_invalidTimestamp;
        *m_end[j] = c_invalidTimestamp;
    }
}

template<typename t_Context>
void GPUContextTimer<t_Context>::Frame::EndFrame(_In_ t_Context* /*context*/)
{
}

template<typename t_Context>
void GPUContextTimer<t_Context>::Frame::ComputeFrame(_In_ t_Context* context)
{
#if defined(_DEBUG) || defined(PROFILE)
    bool blocked = false;
#endif

    for (uint32_t j = 0; j < c_maxTimers; ++j)
    {
        if (!m_used[j])
        {
            m_timing[j] = 0.f;
            continue;
        }

        while (c_invalidTimestamp == *m_start[j])
        {
#if defined(_DEBUG) || defined(PROFILE)
            if (!blocked)
            {
                OutputDebugStringA("WARNING: Blocked performance queries\n");
                blocked = true;
            }
#endif
            Flush(context);
            Sleep(1);
        }

        while (c_invalidTimestamp == *m_end[j])
        {
#if defined(_DEBUG) || defined(PROFILE)
            if (!blocked)
            {
                OutputDebugStringA("WARNING: Blocked performance queries\n");
                blocked = true;
            }
#endif
            Flush(context);
            Sleep(1);
        }

        uint64_t start = *m_start[j];
        uint64_t end = *m_end[j];

        DebugWarnings(j, start, end);

        constexpr double c_FrequencyInvMS = 1000.0 / double(D3D11X_XBOX_GPU_TIMESTAMP_FREQUENCY);
        float value = float(double(end - start) * c_FrequencyInvMS);
        m_timing[j] = value;
        m_avg[j] = UpdateRunningAverage(m_avg[j], value);
        m_used[j] = false;
    }
}

template<typename t_Context>
void GPUContextTimer<t_Context>::Frame::Start(_In_ t_Context* context, uint32_t timerid)
{
    if (timerid >= c_maxTimers)
        throw std::out_of_range("Timer ID out of range");

    m_used[timerid] = true;

    context->WriteTimestampToMemory(m_start[timerid]);
}

template<typename t_Context>
void GPUContextTimer<t_Context>::Frame::Stop(_In_ t_Context* context, uint32_t timerid)
{
    if (timerid >= c_maxTimers)
        throw std::out_of_range("Timer ID out of range");

    context->WriteTimestampToMemory(m_end[timerid]);
}

// Force compilation of these template instantiations
namespace DX
{
    template class GPUContextTimer<ID3D11DeviceContextX>;
    template class GPUContextTimer<ID3D11DmaEngineContextX>;
    template class GPUContextTimer<ID3D11ComputeContextX>;
}

#else
#error Must include d3d11*.h or d3d12*.h in pch.h
#endif
