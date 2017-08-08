//--------------------------------------------------------------------------------------
// CPUSets.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "CPUSets.h"

#include "ATGColors.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

// This is the name of the mutex used for cross thread communication.
const wchar_t* Sample::g_graphicsMutexName = L"GRAPHICS_LOCK";

// Used to simulate work on a thread.
volatile double g_workerThreadStorage;

Sample::Sample() : m_graphicsMutex(INVALID_HANDLE_VALUE), m_hyperThreading(HyperThreadedState::Unknown)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

// Initialize the Direct3D resources required to run.
void Sample::Initialize(IUnknown* window, int width, int height, DXGI_MODE_ROTATION rotation)
{
    m_gamePad = std::make_unique<GamePad>();

    m_keyboard = std::make_unique<Keyboard>();
    m_keyboard->SetWindow(reinterpret_cast<ABI::Windows::UI::Core::ICoreWindow*>(window));

    m_deviceResources->SetWindow(window, width, height, rotation);

    m_deviceResources->CreateDeviceResources();  	
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // Initialize graphics and audio
    m_graphicsMutex = CreateMutex(nullptr, false, g_graphicsMutexName);
    m_audioEngine = std::make_unique<AudioEngine>(AudioEngine_Default);

    // Query CPU Sets information
    ULONG size;
    HANDLE curProc = GetCurrentProcess();
    (void)GetSystemCpuSetInformation(nullptr, 0, &size, curProc, 0);

    std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
    
    PSYSTEM_CPU_SET_INFORMATION cpuSets = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.get());
    
    if (!GetSystemCpuSetInformation(cpuSets, size, &size, curProc, 0))
    {
        DWORD err = GetLastError();
        if (err == ERROR_INSUFFICIENT_BUFFER)
            throw std::exception("Insufficient buffer size for querying CPU set information");
        else
            throw std::exception("An unexpected error occured attempting to query CPU set information");
    }

    size_t count = 0;
    while (size > 0)
    {
        size -= cpuSets[count].Size;
        ++count;
    }

    ReportCPUInformation(cpuSets, count);

    // Set up the threads
    OrganizeCPUSets(cpuSets, count);
    SortThreads();
}

#pragma region Frame Update
// Executes basic render loop.
void Sample::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Sample::Update(DX::StepTimer const& /*timer*/)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        m_gamePadButtons.Update(pad);

        if (pad.IsViewPressed())
        {
            Windows::ApplicationModel::Core::CoreApplication::Exit();
        }
    }
    else
    {
        m_gamePadButtons.Reset();
    }

    auto kb = m_keyboard->GetState();
    m_keyboardButtons.Update(kb);

    if (kb.A)
    {
        ULONG size;
        HANDLE curProc = GetCurrentProcess();
        (void)GetSystemCpuSetInformation(nullptr, 0, &size, curProc, 0);

        std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);

        PSYSTEM_CPU_SET_INFORMATION cpuSets = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.get());
        GetSystemCpuSetInformation(cpuSets, size, &size, GetCurrentProcess(), 0);
        
        size_t count = 0;
        while (size > 0)
        {
            size -= cpuSets[count].Size;
            ++count;
        }

        ReportCPUInformation(cpuSets, count);
    }

    if (kb.Escape)
    {
        Windows::ApplicationModel::Core::CoreApplication::Exit();
    }

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Sample::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Render");

    static XMMATRIX local = SimpleMath::Matrix::CreateTranslation(0.f, 0.f, -10.f); ;
    DWORD waitResult = WaitForSingleObject(m_graphicsMutex, 0);
    if (waitResult == WAIT_OBJECT_0)
    {
        local = m_world * SimpleMath::Matrix::CreateTranslation(0.f, 0.f, -10.f);
        ReleaseMutex(m_graphicsMutex);
    }
    m_model->Draw(context, *m_states, local, m_view, m_projection);

    PIXEndEvent(context);
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Sample::Clear()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    PIXBeginEvent(context, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views
    auto renderTarget = m_deviceResources->GetBackBufferRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, ATG::Colors::Background);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    PIXEndEvent(context);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Sample::OnSuspending()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    context->ClearState();

    m_deviceResources->Trim();
    m_audioEngine->Suspend();
}

void Sample::OnResuming()
{
    m_timer.ResetElapsedTime();
    m_gamePadButtons.Reset();
    m_keyboardButtons.Reset();
    m_audioEngine->Resume();
}

void Sample::OnWindowSizeChanged(int width, int height, DXGI_MODE_ROTATION rotation)
{
    if (!m_deviceResources->WindowSizeChanged(width, height, rotation))
        return;

    CreateWindowSizeDependentResources();
}

void Sample::ValidateDevice()
{
    m_deviceResources->ValidateDevice();
}

// Properties
void Sample::GetDefaultSize(int& width, int& height) const
{
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Sample::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    m_states = std::make_unique<CommonStates>(device);
    m_fxFactory = std::make_unique<EffectFactory>(device);

    m_model = Model::CreateFromSDKMESH(device, L"horse1054.sdkmesh", *m_fxFactory);

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, L"HorseSwirl.dds", nullptr, m_texture.ReleaseAndGetAddressOf())
        );
}

// Allocate all memory resources that change on a window SizeChanged event.
void Sample::CreateWindowSizeDependentResources()
{
    m_at = SimpleMath::Vector3(0.f, 0.f, 0.f);
    m_eye = SimpleMath::Vector3(0.0f, 300.f, -100.f);
    m_view = SimpleMath::Matrix::CreateLookAt(m_eye, m_at, -SimpleMath::Vector3::UnitZ);

    auto size = m_deviceResources->GetOutputSize();
    float aspectRatio = float(size.right) / float(size.bottom);
    float fovAngleY = 70.0f * XM_PI / 180.0f;

    // This is a simple example of change that can be made when the app is in
    // portrait or snapped view.
    if (aspectRatio < 1.0f)
    {
        fovAngleY *= 2.0f;
    }

    m_projection = SimpleMath::Matrix::CreatePerspectiveFieldOfView(
        fovAngleY,
        aspectRatio,
        0.01f,
        1000.0f
        );
}

void Sample::OnDeviceLost()
{
    m_fxFactory.reset();
    m_model.reset();
    m_states.reset();
    m_texture.Reset();
}

void Sample::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}

void Sample::ReportCPUInformation(SYSTEM_CPU_SET_INFORMATION* cpuSetInfo, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        wchar_t buffer[1024] = { L'\0' };
        swprintf_s(buffer, 1024, L"CPU ID: %i\n\tGroup: %i"
            "\n\tLogical index: %i\n\tCore index: %i\n\tCache ID: %i"
            "\n\tNUMA ID: %i\n\tEfficiency class: %i\n\tAll flags: %i"
            "\n\tAllocated: %i\n\tAllocated to target: %i"
            "\n\tParked: %i\n\tRealtime: %i\n\tReserved flags: %i\n\tReserved: %i\n",
            cpuSetInfo[i].CpuSet.Id,                    // Unique ID for every CPU core. This is the value to use with SetProcessDefaultCpuSets
            cpuSetInfo[i].CpuSet.Group,                 // Some PCs (mostly servers) have groups of CPU cores
            cpuSetInfo[i].CpuSet.LogicalProcessorIndex, // Index of the logical core of the CPU, relative to this CPU group
            cpuSetInfo[i].CpuSet.CoreIndex,             // Index of the home core any logical core is associated with, relative to this CPU group 
            cpuSetInfo[i].CpuSet.LastLevelCacheIndex,   // ID of the memory cache this core uses, relative to this CPU group
            cpuSetInfo[i].CpuSet.NumaNodeIndex,         // ID of the NUMA group for this core, relative to this CPU group
            cpuSetInfo[i].CpuSet.EfficiencyClass,
            cpuSetInfo[i].CpuSet.AllFlags,
            cpuSetInfo[i].CpuSet.Allocated,
            cpuSetInfo[i].CpuSet.AllocatedToTargetProcess,
            cpuSetInfo[i].CpuSet.Parked,
            cpuSetInfo[i].CpuSet.RealTime,
            cpuSetInfo[i].CpuSet.ReservedFlags,
            cpuSetInfo[i].CpuSet.Reserved);

        OutputDebugString(buffer);
    }

    return;
}

void Sample::OrganizeCPUSets(SYSTEM_CPU_SET_INFORMATION* cpuSetInfo, size_t count)
{
    // There are a number of useful ways to organize threads based on the information provided by
    //  the API. Fore example:
    //  1 - Time critical threads should have their own logical cores (LogicalProcessorCore), 
    //      preferably on their own physical core (CoreIndex)
    //  2 - Threads that share data should share the same cache if possible
    //  3 - NUMA and groups can also be taken into account for applications are meant to be run
    //      on servers where these hardware features are expected
    //  4 - If multiple caches are present, threads that communicate frequently can be kept on
    //      core with the same cache. Memory intensive threads that don't share data can be kept
    //      on cores with different caches to prevent cache misses.
    //
    // This demonstrates organizing threads based on the position of logical cores with respect
    //  to physical cores. Heavy, time critical threads should have their own physical cores to
    //  prevent contention for processing time with other threads.

    m_hyperThreading = HyperThreadedState::NotHyperThreaded;

    for (size_t i = 0; i < count; ++i)
    {
        if (cpuSetInfo[i].Type == CPU_SET_INFORMATION_TYPE::CpuSetInformation)
        {
            auto cpus = m_cpuSets.find(cpuSetInfo[i].CpuSet.CoreIndex);
            if (cpus == m_cpuSets.end())
            {
                std::pair<BYTE, std::vector<SYSTEM_CPU_SET_INFORMATION>> cpuSetInfoPair;
                cpuSetInfoPair.first = cpuSetInfo[i].CpuSet.CoreIndex;
                cpuSetInfoPair.second.push_back(cpuSetInfo[i]);
                m_cpuSets.insert(cpuSetInfoPair);
            }
            else
            {
                // There are multiple logical cores on one physical core so the CPU is using hyperthreading.
                m_hyperThreading = HyperThreadedState::HyperThreaded;
                cpus->second.push_back(cpuSetInfo[i]);
            }
        }
    }

    {
        unsigned long retsize = 0;
        (void)GetSystemCpuSetInformation(nullptr, 0, &retsize,
            GetCurrentProcess(), 0);

        std::unique_ptr<uint8_t[]> data(new uint8_t[retsize]);
        if (!GetSystemCpuSetInformation(
            reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(data.get()),
            retsize, &retsize, GetCurrentProcess(), 0))
        {
            // Error!
        }

        unsigned long count2 = retsize / sizeof(SYSTEM_CPU_SET_INFORMATION);
        bool sharedcache = false;

        std::map<unsigned char, std::vector<SYSTEM_CPU_SET_INFORMATION>> cachemap;
        for (size_t i = 0; i < count2; ++i)
        {
            auto cpuset = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(data.get())[i];
            if (cpuset.Type == CPU_SET_INFORMATION_TYPE::CpuSetInformation)
            {
                if (cachemap.find(cpuset.CpuSet.LastLevelCacheIndex) == cachemap.end())
                {
                    std::pair<unsigned char, std::vector<SYSTEM_CPU_SET_INFORMATION>> newvalue;
                    newvalue.first = cpuset.CpuSet.LastLevelCacheIndex;
                    newvalue.second.push_back(cpuset);
                    cachemap.insert(newvalue);
                }
                else
                {
                    sharedcache = true;
                    cachemap[cpuset.CpuSet.LastLevelCacheIndex].push_back(cpuset);
                }
            }
        }
    }
}

unsigned long __stdcall Sample::GeneratorThread(void* params)
{
    Sample* args = (Sample*)params;
    for (;;)
    {
        unsigned long waitResult = WaitForSingleObject(args->GetGraphicsMutex(), 10);
        if (waitResult == WAIT_OBJECT_0)
        {
            float time = GetTickCount64() / 1000.f;
            auto world = SimpleMath::Matrix::CreateFromYawPitchRoll(0, 0, time);
            args->SetWorldMatrix(world);
            ReleaseMutex(args->GetGraphicsMutex());
            Sleep(1);
        }
    }
}

unsigned long __stdcall Sample::AudioThread(void* params)
{
    Sample* args = (Sample*)params;
    const wchar_t* strFilePath = L"MusicMono_adpcm.wav";
    auto musicEffect = std::make_unique<SoundEffect>(args->GetAudioEngine(), strFilePath);
    auto effectInstance = musicEffect->CreateInstance();

    effectInstance->Play(true);

    for (;;)
    {
        Sleep(1);
        (*args->GetAudioEngine()).Update();
    }

    return 0;
}

unsigned long __stdcall Sample::WorkerThread(void* /*params*/)
{
    // Simulate work being done on the worker thread.
    for (;;)
    {
        double cosAccumulation = 0.0, sinAccumulation = 0.0, tanAccumulation = 0.0;
        for (double input = -3.0; input < 3.0; input += 0.01)
        {
            cosAccumulation += cos(input);
            sinAccumulation += sin(input);
            tanAccumulation += tan(input);
        }

        g_workerThreadStorage = cosAccumulation + sinAccumulation + tanAccumulation;
    }
}

void Sample::SortThreads()
{
    if (!CreateThread(nullptr, 0, Sample::WorkerThread, this, 0, nullptr))
    {
        throw std::exception("Something went wrong creating the WorkerThread");
    }

    // Keep the handle for other threads so their CPU sets can be explicitly set.
    HANDLE generatorHandle = CreateThread(nullptr, 0, Sample::GeneratorThread, this, 0, nullptr);
    HANDLE audioHandle = CreateThread(nullptr, 0, Sample::AudioThread, this, 0, nullptr);

    if (!generatorHandle || !audioHandle)
    {
        throw std::exception("Something went wrong creating threads");
    }

    // Handle the case where there are multiple physical cores.
    if (m_cpuSets.size() > 1)
    {
        // If this isn't a hyperthreaded system, there is only one logical core per physical core.
        if (!CpuIsUsingHyperthreading())
        {
            // Use an iterator to find the ID of the CPU set on the second core.
            auto coreIterator = m_cpuSets.begin();
            unsigned long core0 = coreIterator->second.front().CpuSet.Id;
            ++coreIterator;
            unsigned long core1 = coreIterator->second.front().CpuSet.Id;

            (void)SetThreadSelectedCpuSets(GetCurrentThread(), &core0, 1);
            (void)SetThreadSelectedCpuSets(audioHandle, &core1, 1);

            // If there are more than two cores, the audio thread and generator thread can be put on separate cores.
            ++coreIterator;
            if (coreIterator != m_cpuSets.end())
            {
                unsigned long core2 = coreIterator->second.front().CpuSet.Id;
                (void)SetThreadSelectedCpuSets(generatorHandle, &core2, 1);
            }
            
            // If there are only two cores, the audio and generator threads will be grouped to give the graphics thread its own core.
            else
            {
                (void)SetThreadSelectedCpuSets(generatorHandle, &core1, 1);
            }

            // Set the default CPU set to the remaining cores
            unsigned long remainingIds[10];
            unsigned coreCount = 0;
            while (coreIterator != m_cpuSets.end() && coreCount < 10)
            {
                remainingIds[coreCount] = coreIterator->second.front().CpuSet.Id;
                ++coreCount;
                ++coreIterator;
            }

            if(coreCount > 0)
                (void)SetProcessDefaultCpuSets(GetCurrentProcess(), remainingIds, coreCount);
        }

        // If there are multiple logical cores per physical core, just make sure the rendering thread has its own physical core.
        else
        {
            auto coreIterator = m_cpuSets.begin();
            (void)SetThreadSelectedCpuSets(GetCurrentThread(), &coreIterator->second.front().CpuSet.Id, 1);
            ++coreIterator;

            // Spread out remaining threads on physical core 1 and let the render thread have physical core 0 to itself.
            auto core1SetsIter = coreIterator->second.begin();
            (void)SetThreadSelectedCpuSets(audioHandle, &core1SetsIter->CpuSet.Id, 1);

            if (++core1SetsIter == coreIterator->second.end())
                --core1SetsIter;
            (void)SetThreadSelectedCpuSets(generatorHandle, &core1SetsIter->CpuSet.Id, 1);

            if (++core1SetsIter == coreIterator->second.end())
                --core1SetsIter;
            (void)SetProcessDefaultCpuSets(GetCurrentProcess(), &core1SetsIter->CpuSet.Id, 1);
        }
    }

    // Handle the case where there's only one physical core.
    else
    {
        auto cpuSets = m_cpuSets.begin()->second;

        // Only assign threads to cores if there are multiple cores to work with.
        if (cpuSets.size() > 1)
        {
            auto setsIterator = cpuSets.begin();
            (void)SetThreadSelectedCpuSets(GetCurrentThread(), &setsIterator->CpuSet.Id, 1);
            ++setsIterator;

            // Spread out the threads anywhere other than the first CPU set which will be reserved for the rendering thread.
            (void)SetThreadSelectedCpuSets(audioHandle, &setsIterator->CpuSet.Id, 1);

            if (++setsIterator == cpuSets.end())
                --setsIterator;
            (void)SetThreadSelectedCpuSets(generatorHandle, &setsIterator->CpuSet.Id, 1);

            if (++setsIterator == cpuSets.end())
                --setsIterator;
            (void)SetProcessDefaultCpuSets(GetCurrentProcess(), &setsIterator->CpuSet.Id, 1);
        }
    }
}

#pragma endregion
