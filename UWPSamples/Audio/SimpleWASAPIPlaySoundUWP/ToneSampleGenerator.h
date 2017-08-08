//--------------------------------------------------------------------------------------
// ToneSampleGenerator.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <limits.h>

template<typename T> T Convert(double Value);

class ToneSampleGenerator
{
public:
    ToneSampleGenerator();
    ~ToneSampleGenerator();
    
    bool		 IsEOF(){ return (m_SampleQueue == nullptr); };
    unsigned int GetBufferLength(){ return ( m_SampleQueue != nullptr ? m_SampleQueue->BufferSize : 0 ); };
    void Flush();

    HRESULT GenerateSampleBuffer( unsigned long Frequency, unsigned int FramesPerPeriod, WAVEFORMATEX *wfx );
    HRESULT FillSampleBuffer( unsigned int BytesToRead, unsigned char *Data );

private:
    template <typename T>
        void GenerateSineSamples(unsigned char *Buffer, size_t BufferLength, unsigned long Frequency, unsigned short ChannelCount, unsigned long SamplesPerSecond, double Amplitude, double *InitialTheta);

private:
    RenderBuffer    *m_SampleQueue;
    RenderBuffer    **m_SampleQueueTail;
};
