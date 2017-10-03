//--------------------------------------------------------------------------------------
// ShardOSPrimitiveTool.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "StepTimer.h"
#include <vector>
#include <thread>
#include <atomic>

class Sample;

class SharedSample
{
    friend void PerformTests(SharedSample *data);

private:
    Sample *m_sample;   // The lifetime of this class is entirely within the lifetime of this Sample

    std::vector<std::wstring> BreakCommandLine(const wchar_t *commandLineParams);

private:
    bool m_cmdLineError;

    std::atomic<bool> m_shutdownThread;
    std::atomic<bool> m_finishedTestRun;
    std::atomic<bool> m_noContention;
    std::atomic<uint32_t> m_idleWorkers;
    std::atomic<uint32_t> m_whatRunFinished;
    std::thread *m_workerThread;

public:
    SharedSample(Sample* sample);
    ~SharedSample();

    void Update(DX::StepTimer const& timer);
    void Render();

    void ParseCommandLine(const wchar_t *commandlineParams);
};
