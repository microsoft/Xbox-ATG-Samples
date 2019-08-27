//--------------------------------------------------------------------------------------
// WaveSampleGenerator.h
//
// Demonstrates how to play an in-memory WAV file via WASAPI.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef WAVE_SAMPLE_GENERATOR_H_INCLUDED
#define WAVE_SAMPLE_GENERATOR_H_INCLUDED

#include "pch.h"
#include "Common.h"

class WaveSampleGenerator
{
public:
    WaveSampleGenerator();
    ~WaveSampleGenerator();

    Platform::Boolean IsEOF() const { return (m_SampleQueue == nullptr); }
    UINT32 GetBufferLength() const { return ( m_SampleQueue != nullptr ? m_SampleQueue->BufferSize : 0 ); }
    void Flush();

    HRESULT GenerateSampleBuffer( BYTE* pWaveData, DWORD waveSize, WAVEFORMATEXTENSIBLE* pSourceWfx, UINT32 framesPerPeriod, WAVEFORMATEX *pWfx );

    HRESULT FillSampleBuffer( UINT32 bytesToRead, BYTE *pData );

private:

    RenderBuffer*   m_SampleQueue;
    RenderBuffer**  m_SampleQueueTail;
};

#endif