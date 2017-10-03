//--------------------------------------------------------------------------------------
// OSLockable.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

// It's required to include the proper OS header based on target platform before including this header
#include <chrono>
#include <cmath>
#include <cstdint>

// Provides wrappers around the standard Windows locking primitives to make them act like a C++ timedLockable
// This allowed them to all be used in any C++ thread interfaces

namespace ATG
{
    //////////////////////////////////////////////////////////////////////////
    /// \brief EventLockable
    /// \details ATG::EventLockable<manualReset, initialState>
    /// \details Wraps a Windows Event object and implements the C++ concept TimedLockable
    ///          Can be used in all C++ thread interfaces
    //////////////////////////////////////////////////////////////////////////
    template <bool manualReset = false, bool initialState = true>
    class EventLockable
    {
    private:
        HANDLE m_event;

    public:
        EventLockable(const EventLockable&) = delete;
        EventLockable& operator=(const EventLockable&) = delete;

        EventLockable() { m_event = CreateEvent(nullptr, manualReset, initialState, nullptr); if (!m_event) throw std::exception("CreateEvent failed"); }
        EventLockable(EventLockable&& rhs) { m_event = rhs.m_event; rhs.m_event = INVALID_HANDLE_VALUE; }
        ~EventLockable() { CloseHandle(m_event); }

        void wait() { lock(); }
        void signal() { unlock(); }

        void lock()
        {
            WaitForSingleObject(m_event, INFINITE);
        }

        bool try_lock()
        {
            return WaitForSingleObject(m_event, 0) == WAIT_OBJECT_0;
        }

        void unlock()
        {
            SetEvent(m_event);
        }

        void reset()
        {
            ResetEvent(m_event);
        }

        template<class _Rep, class _Period>
        bool try_lock_for(const std::chrono::duration<_Rep, _Period>& relTime)
        {
            int64_t msWait = std::chrono::duration_cast<std::chrono::nanoseconds>(relTime).count();
            if (msWait < 0)
                msWait = 0;
            msWait = static_cast<uint64_t> (std::ceil(static_cast<double> (msWait) / 1000000.0));

            return WaitForSingleObject(m_event, static_cast<DWORD> (msWait)) == WAIT_OBJECT_0;
        }

        template<class _Clock, class _Duration>
        bool try_lock_until(const std::chrono::time_point<_Clock, _Duration>& absTime)
        {
            return try_lock_for(absTime - std::chrono::steady_clock::now());
        }
    };

    //////////////////////////////////////////////////////////////////////////
    /// \brief SemaphoreLockable
    /// \details ATG::SemaphoreLockable<initialCount, maximumCount>
    /// \details Wraps a Windows Semaphore object and implements the C++ concept TimedLockable
    ///          Can be used in all C++ thread interfaces
    //////////////////////////////////////////////////////////////////////////
    template<uint32_t initialCount, uint32_t maximumCount>
    class SemaphoreLockable
    {
    private:
        HANDLE m_semaphore;

    public:
        SemaphoreLockable(const SemaphoreLockable&) = delete;
        SemaphoreLockable& operator=(const SemaphoreLockable&) = delete;

        SemaphoreLockable()
        {
            m_semaphore = CreateSemaphoreEx(nullptr, initialCount, maximumCount, nullptr, 0, SYNCHRONIZE | SEMAPHORE_MODIFY_STATE);
            if (!m_semaphore)
                throw std::exception("CreateSemaphoreEx failed");
        }
        SemaphoreLockable(SemaphoreLockable&& rhs) { m_semaphore = rhs.m_semaphore; rhs.m_semaphore = INVALID_HANDLE_VALUE; }
        ~SemaphoreLockable() { CloseHandle(m_semaphore); }

        void wait() { lock(); }
        void signal(uint32_t postCount = 1) { unlock(postCount); }
        void post(uint32_t postCount = 1) { unlock(postCount); }

        void lock()
        {
            WaitForSingleObject(m_semaphore, INFINITE);
        }

        bool try_lock()
        {
            return WaitForSingleObject(m_semaphore, 0) == WAIT_OBJECT_0;
        }

        void unlock(uint32_t count = 1)
        {
            ReleaseSemaphore(m_semaphore, count, nullptr);
        }

        template<class _Rep, class _Period>
        bool try_lock_for(const std::chrono::duration<_Rep, _Period>& relTime)
        {
            int64_t msWait = std::chrono::duration_cast<std::chrono::milliseconds>(relTime).count();
            if (msWait < 0)
                msWait = 0;
            return WaitForSingleObject(m_semaphore, static_cast<DWORD> (msWait)) == WAIT_OBJECT_0;
        }

        template<class _Clock, class _Duration>
        bool try_lock_until(const std::chrono::time_point<_Clock, _Duration>& absTime)
        {
            return try_lock_for(absTime - std::chrono::steady_clock::now());
        }
    };

    //////////////////////////////////////////////////////////////////////////
    /// \brief MutexLockable
    /// \details ATG::MutexLockable
    /// \details Wraps a Windows Mutex object and implements the C++ concept TimedLockable
    ///          Can be used in all C++ thread interfaces
    //////////////////////////////////////////////////////////////////////////
    class MutexLockable
    {
    private:
        HANDLE m_mutex;

    public:
        MutexLockable(const MutexLockable&) = delete;
        MutexLockable& operator=(const MutexLockable&) = delete;

        MutexLockable()
        {
            m_mutex = CreateMutex(nullptr, false, nullptr);
            if (!m_mutex)
                throw std::exception("CreateMutex failed");
        }
        MutexLockable(MutexLockable&& rhs) { m_mutex = rhs.m_mutex; rhs.m_mutex = INVALID_HANDLE_VALUE; }
        ~MutexLockable() { CloseHandle(m_mutex); }

        void lock()
        {
            WaitForSingleObject(m_mutex, INFINITE);
        }

        bool try_lock()
        {
            return WaitForSingleObject(m_mutex, 0) == WAIT_OBJECT_0;
        }

        void unlock()
        {
            ReleaseMutex(m_mutex);
        }

        template<class _Rep, class _Period>
        bool try_lock_for(const std::chrono::duration<_Rep, _Period>& relTime)
        {
            int64_t msWait = std::chrono::duration_cast<std::chrono::milliseconds>(relTime).count();
            if (msWait < 0)
                msWait = 0;
            return WaitForSingleObject(m_mutex, static_cast<DWORD> (msWait)) == WAIT_OBJECT_0;
        }

        template<class _Clock, class _Duration>
        bool try_lock_until(const std::chrono::time_point<_Clock, _Duration>& absTime)
        {
            return try_lock_for(absTime - std::chrono::steady_clock::now());
        }
    };

    //////////////////////////////////////////////////////////////////////////
    /// \brief CriticalSectionLockable
    /// \details ATG::CriticalSectionLockable
    /// \details Wraps a Windows CRITICAL_SECTION object and implements the C++ concept Lockable
    ///          TimedLockable is not supported because CRITICAL_SECTION does not support
    ///          Can be used in all C++ thread interfaces
    //////////////////////////////////////////////////////////////////////////
    class CriticalSectionLockable
    {
    private:
        CRITICAL_SECTION m_critSection;

    public:
        CriticalSectionLockable(const CriticalSectionLockable&) = delete;
        CriticalSectionLockable& operator=(const CriticalSectionLockable&) = delete;

        CriticalSectionLockable(uint32_t spinCount = 0) { (void)InitializeCriticalSectionAndSpinCount(&m_critSection, spinCount); }
        CriticalSectionLockable(CriticalSectionLockable&& rhs) = delete;
        ~CriticalSectionLockable() { DeleteCriticalSection(&m_critSection); }

        void lock()
        {
            EnterCriticalSection(&m_critSection);
        }

        bool try_lock()
        {
            return TryEnterCriticalSection(&m_critSection) == TRUE;
        }

        void unlock()
        {
            LeaveCriticalSection(&m_critSection);
        }
    };
}
