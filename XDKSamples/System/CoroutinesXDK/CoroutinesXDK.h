//--------------------------------------------------------------------------------------
// CoroutinesXDK.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

template <typename T>
struct awaitable_future
{
    struct promise_type;
    using handle_t = std::experimental::coroutine_handle<promise_type>;

    struct promise_type
    {
        T value;

        // This is called to create the future object when memory allocation fails. There isn't much
        //  that can be done in this case so return an object with a null coroutine handle.
        static auto get_return_object_on_allocation_failure()
        {
            return awaitable_future{ nullptr };
        }

        // This is called to create the future object when the coroutine is invoked.
        auto get_return_object()
        {
            return awaitable_future{ handle_t::from_promise(*this) };
        }

        // When the coroutine is first invoked this is called. If it returns true the coroutine will
        //  start executing immediately. If it returns false the coroutine will suspend immediately
        //  and return execution to the caller. The caller would then have to resume the coroutine
        //  to execute the body of the coroutine.
        // In this case execution occurs immediately after the coroutine is called.
        auto initial_suspend()
        {
            return std::experimental::suspend_never{};
        }

        // When execution reaches the end of the coroutine, or a co_return statement is encountered,
        //  this will be called to determine whether the coroutine should suspend before cleaning up
        //  the coroutine context. If this returns true, then the coroutine context will be freed.
        //  If this returns false, then the coroutine context will remain in memory and must be
        //  released later by client code.
        // In this case, the coroutine is suspended and remains in memory. This allows the caller
        //  to access the data co_returned from the coroutine.
        auto final_suspend()
        {
            return std::experimental::suspend_always{};
        }

        // This function is required for the use of co_return. The value that is used with the unary
        //  co_return operator is passed to this function. This implementation simply stores the
        //  value to be accessed later by the caller through the future object.
        void return_value(T val)
        {
            value = val;
        }

        // Overloading the new operator in the promise type redirects allocations for the coroutine
        //  context here instead of using ::operator new(). Elision or preallocated heaps may be
        //  used for better runtime performance.
        void *operator new(size_t size, std::nothrow_t, void*)
        {
            return malloc(size);
        }

        // Overloading operator delete here allows client code to handle freeing memory. This is
        //  needed if operator new is overloaded so you can use elision or a preallocated heap.
        void operator delete(void *p, size_t)
        {
            free(p);
        }
    };

    // If the coroutine is not done, this function will block until it is completed. If it is
    //  completed, then it will return the value immediately.
    // This specific function is not required for coroutines, but client code should include some
    //  way of accessing data once a coroutine is completed.
    T get()
    {
        return coro_handle.promise().value;
    }

    // Resume the coroutine if it has been suspended.
    // This specific function is not necessary for implementing future types that can be returned
    //  from coroutines. A future type should provide a way to resume a suspended coroutine.
    void resume()
    {
        coro_handle.resume();
    }

    // This function will clean up the coroutine context. It is meant to be called after a coroutine
    //  is finished, but it may also be called on a suspended coroutine if it is determined by
    //  client code that the coroutine is no longer needed.
    // This specific function is not needed for the coroutine implementation. However, if there
    //  is a chance final_suspend will return suspend_always, then there should be some way the
    //  client code can clean up the coroutine context manually.
    void release()
    {
        coro_handle.destroy();
    }

    // The inclusion of the three await_* functions allow this type to be used with co_await

    // Always suspend upon co_await
    bool await_ready()
    {
        return false;
    }


    // Return the value held by the promise object when resumed
    auto await_resume()
    {
        return coro_handle.promise().value;
    }

    // No special action needed when suspending
    void await_suspend(handle_t)
    {

    }

    handle_t coro_handle;
};

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

    // Properties
    bool RequestHDRMode() const { return m_deviceResources ? (m_deviceResources->GetDeviceOptions() & DX::DeviceResources::c_EnableHDR) != 0 : false; }

    awaitable_future<Platform::String^> GetNewDisplayString();

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void EmitThreadIdDebug();
    std::future<void> UpdateUsersOffThread();
    std::future<Windows::Xbox::UI::AccountPickerResult^> GetNewUser();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>        m_deviceResources;

    // Rendering loop timer.
    uint64_t                                    m_frame;
    DX::StepTimer                               m_timer;

    // Input devices.
    std::unique_ptr<DirectX::GamePad>           m_gamePad;

    DirectX::GamePad::ButtonStateTracker        m_gamePadButtons;

    // DirectXTK objects.
    std::unique_ptr<DirectX::GraphicsMemory>    m_graphicsMemory;
    std::unique_ptr<DirectX::SpriteFont>        m_font;
    std::unique_ptr<DirectX::SpriteFont>        m_ctrlFont;
    std::unique_ptr<DirectX::SpriteBatch>       m_spriteBatch;

    Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^    m_users;
    Windows::Xbox::System::IUser^                                                   m_currentUser;
    bool                                                                            m_acquiringNewUser;

    Platform::String^                                                               m_displayString;
    Windows::Foundation::IAsyncOperation<Platform::String^>^                        m_stringAsync;
    awaitable_future<Platform::String^>                                             m_future;

    std::mutex                                                                      m_usersMutex, m_userMutex;
};



