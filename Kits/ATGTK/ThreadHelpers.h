//--------------------------------------------------------------------------------------
// File: ThreadHelpers.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//-------------------------------------------------------------------------------------

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

namespace DX
{
    class ThreadSuspender
    {
    private:
        HANDLE m_hThread;

    public:
        ThreadSuspender(HANDLE hThread)
            : m_hThread(hThread)
        {
            SuspendThread(m_hThread);
        }

        ~ThreadSuspender()
        {
            ResumeThread(m_hThread);
        }
    };

    // Sets a thread name for debugging and profiling
    template<UINT TNameLength>
    inline void SetThreadName(HANDLE hThread, _In_z_ const char(&name)[TNameLength])
    {
        // See https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code
#if defined(_XBOX_ONE) && defined(_TITLE)
        wchar_t wname[MAX_PATH];
        int result = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, name, TNameLength, wname, MAX_PATH);
        if (result > 0)
        {
            ::SetThreadName(hThread, wname);
        }
#elif (_WIN32_WINNT >= _WIN32_WINNT_WIN10)
        wchar_t wname[MAX_PATH] = {};
        const int result = MultiByteToWideChar(CP_UTF8, 0, name, TNameLength, wname, MAX_PATH);
        if (result > 0)
        {
            ::SetThreadDescription(hThread, wname);
        }
#else
#pragma pack(push,8)
        typedef struct tagTHREADNAME_INFO
        {
            uint32_t    dwType;
            const char* szName;
            uint32_t    dwThreadID;
            uint32_t    dwFlags;
        } THREADNAME_INFO;
#pragma pack(pop)

        DWORD threadId = GetThreadId(hThread);

        THREADNAME_INFO info = { 0x1000, name, !threadId ? uint32_t(-1) : threadId, 0 };

#pragma warning(push)
#pragma warning(disable: 6320 6322)
        __try
        {
            RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (const ULONG_PTR*)&info);
        }
        __except (GetExceptionCode() == 0x406D1388 ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_EXECUTE_HANDLER)
        {
            __noop;
        }
#pragma warning(pop)
#endif
    }

    class ThreadHelpers
    {
        ThreadHelpers()
        {
#if defined(_XBOX_ONE) && defined(_TITLE)

#elif defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_APP
            ULONG retsize = 0;
            std::ignore = GetSystemCpuSetInformation(nullptr, 0, &retsize, GetCurrentProcess(), 0);

            m_cpuSetsInformation.reset(new uint8_t[retsize]);

            PSYSTEM_CPU_SET_INFORMATION cpuSets = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(m_cpuSetsInformation.get());
            if (!GetSystemCpuSetInformation(cpuSets, retsize, &retsize, GetCurrentProcess(), 0))
            {
                throw std::exception("GetSystemCpuSetInformation");
            }

            size_t count = retsize / cpuSets[0].Size;

            for (unsigned long i = 0; i < count; ++i)
            {
                if (cpuSets[i].Type == CpuSetInformation)
                {
                    if (m_coresCollection.find(cpuSets[i].CpuSet.CoreIndex) == m_coresCollection.end())
                    {
                        m_coresCollection.insert(std::pair<unsigned char, PSYSTEM_CPU_SET_INFORMATION>(cpuSets[i].CpuSet.CoreIndex, &cpuSets[i]));
                    }
                }
            }
#else

            unsigned long length = 0;
            std::ignore = GetLogicalProcessorInformation(nullptr, &length);

            std::unique_ptr<uint8_t> buffer(new uint8_t[length]);

            SYSTEM_LOGICAL_PROCESSOR_INFORMATION *procInfo = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION *>(buffer.get());
            if (!GetLogicalProcessorInformation(procInfo, &length))
            {
                throw std::exception("GetLogicalProcessorInformation");
            }

            const size_t count = length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
            for (size_t i = 0; i < count; ++i)
            {
                if (procInfo[i].Relationship == RelationProcessorCore)
                {
                    m_physicalCoreMaskLookup.push_back(procInfo[i].ProcessorMask);
                }
            }

#endif
        }

    public:

        ThreadHelpers(ThreadHelpers&&) = default;
        ThreadHelpers& operator=(ThreadHelpers&&) = default;

        ThreadHelpers(const ThreadHelpers&) = delete;
        ThreadHelpers& operator=(const ThreadHelpers&) = delete;

        void SetThreadPhysicalProcessor(HANDLE hThread, unsigned coreIndex) const
        {
#if defined(_XBOX_ONE) && defined(_TITLE)
            assert(coreIndex < 8);
            SetThreadAffinityMask(hThread, uintptr_t(0x1) << coreIndex);
#elif defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_APP
            if (coreIndex < GetCoreCount())
            {
                auto iter = m_coresCollection.begin();
                while (coreIndex-- != 0)
                    ++iter;

                unsigned long physicalCore = m_coresCollection.at(iter->first)->CpuSet.Id;

                std::ignore = SetThreadSelectedCpuSets(hThread, &physicalCore, 1);
            }
#else
            if (coreIndex < GetCoreCount())
            {
                SetThreadAffinityMask(hThread, m_physicalCoreMaskLookup[coreIndex]);
            }
#endif
        }

        unsigned GetCoreCount() const
        {
#if defined(_XBOX_ONE) && defined(_TITLE)
            return 8;
#elif defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_APP
            return static_cast<unsigned>(m_coresCollection.size());
#else
            return static_cast<unsigned>(m_physicalCoreMaskLookup.size());
#endif
        }

        static ThreadHelpers* Instance()
        {
            static ThreadHelpers s_instance;
            return &s_instance;
        }

    private:

#if defined(_XBOX_ONE) && defined(_TITLE)

#elif defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_APP

        std::unique_ptr<uint8_t[]> m_cpuSetsInformation;
        std::map<unsigned char, PSYSTEM_CPU_SET_INFORMATION> m_coresCollection;

#else

        std::vector<uintptr_t> m_physicalCoreMaskLookup;

#endif
    };
}
