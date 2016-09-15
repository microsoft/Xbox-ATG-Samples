//--------------------------------------------------------------------------------------
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//
// ToneSampleGenerator.h
//

#include "pch.h"
#include "ToneSampleGenerator.h"

const int TONE_DURATION_SEC = 30;
const double TONE_AMPLITUDE = 0.5;     // Scalar value, should be between 0.0 - 1.0

//
//  Convert from double to float, byte, short or int32.
//
template<>
float Convert<float>(double Value)
{
    return static_cast<float>(Value);
};

template<>
short Convert<short>(double Value)
{
    return static_cast<short>(Value * _I16_MAX);
};

//
//  ToneSampleGenerator()
//
ToneSampleGenerator::ToneSampleGenerator()
{
    m_SampleQueue = nullptr;
    m_SampleQueueTail = &m_SampleQueue;
}

//
//  ~ToneSampleGenerator()
//
ToneSampleGenerator::~ToneSampleGenerator()
{
    // Flush unused samples
    Flush();
}

//
//  GenerateSampleBuffer()
//
//  Create a linked list of sample buffers
//
HRESULT ToneSampleGenerator::GenerateSampleBuffer(unsigned long Frequency, unsigned int FramesPerPeriod, WAVEFORMATEX *wfx )
{
    HRESULT hr = S_OK;

    uint32_t renderBufferSizeInBytes = FramesPerPeriod * wfx->nBlockAlign;
    uint64_t renderDataLength = ( wfx->nSamplesPerSec * TONE_DURATION_SEC * wfx->nBlockAlign ) + ( renderBufferSizeInBytes - 1 );
	uint64_t renderBufferCount = renderDataLength / renderBufferSizeInBytes;

    double theta = 0;

    for( UINT64 i = 0; i < renderBufferCount; i++ )
    {
        RenderBuffer *SampleBuffer = new (std::nothrow) RenderBuffer();
        if (nullptr == SampleBuffer)
        {
            return E_OUTOFMEMORY;
        }

        SampleBuffer->BufferSize = renderBufferSizeInBytes;
        SampleBuffer->BytesFilled = renderBufferSizeInBytes;
        SampleBuffer->Buffer = new (std::nothrow) unsigned char[ renderBufferSizeInBytes ];
        if (nullptr == SampleBuffer->Buffer)
        {
            return E_OUTOFMEMORY;
        }

        switch( CalculateMixFormatType( wfx ) )
        {
        case RenderSampleType::SampleType16BitPCM:
            GenerateSineSamples<short>( SampleBuffer->Buffer, SampleBuffer->BufferSize, Frequency, wfx->nChannels, wfx->nSamplesPerSec, TONE_AMPLITUDE, &theta);
            break;

        case RenderSampleType::SampleTypeFloat:
            GenerateSineSamples<float>( SampleBuffer->Buffer, SampleBuffer->BufferSize, Frequency, wfx->nChannels, wfx->nSamplesPerSec, TONE_AMPLITUDE, &theta);
            break;

        default:
            return E_UNEXPECTED;
            break;
        }

        *m_SampleQueueTail = SampleBuffer;
        m_SampleQueueTail = &SampleBuffer->Next;
    }
    return hr;
}

//
// GenerateSineSamples()
//
//  Generate samples which represent a sine wave that fits into the specified buffer.
//
//  T:  Type of data holding the sample (short, int, byte, float)
//  Buffer - Buffer to hold the samples
//  BufferLength - Length of the buffer.
//  ChannelCount - Number of channels per audio frame.
//  SamplesPerSecond - Samples/Second for the output data.
//  InitialTheta - Initial theta value - start at 0, modified in this function.
//
template <typename T>
void ToneSampleGenerator::GenerateSineSamples(unsigned char *Buffer, size_t BufferLength, unsigned long Frequency, unsigned short ChannelCount, unsigned long SamplesPerSecond, double Amplitude, double *InitialTheta)
{
    double sampleIncrement = (Frequency * (M_PI*2)) / (double)SamplesPerSecond;
    T *dataBuffer = reinterpret_cast<T *>(Buffer);
    double theta = (InitialTheta != NULL ? *InitialTheta : 0);

    for (size_t i = 0 ; i < BufferLength / sizeof(T) ; i += ChannelCount)
    {
        double sinValue = Amplitude * sin( theta );
        for(size_t j = 0 ;j < ChannelCount; j++)
        {
            dataBuffer[i+j] = Convert<T>(sinValue);
        }
        theta += sampleIncrement;
    }

    if (InitialTheta != NULL)
    {
        *InitialTheta = theta;
    }
}

//
//  FillSampleBuffer()
//
//  File the Data buffer of size BytesToRead with the first item in the queue.  Caller is responsible for allocating and freeing buffer
//
HRESULT ToneSampleGenerator::FillSampleBuffer( unsigned int BytesToRead, unsigned char *Data )
{
    if (nullptr == Data)
    {
        return E_POINTER;
    }

    RenderBuffer *SampleBuffer = m_SampleQueue;

    if (BytesToRead > SampleBuffer->BufferSize)
    {
        return E_INVALIDARG;
    }

    CopyMemory( Data, SampleBuffer->Buffer, BytesToRead );

    m_SampleQueue = m_SampleQueue->Next;

    return S_OK;
}

//
//  Flush()
//
//  Remove and free unused samples from the queue
//
void ToneSampleGenerator::Flush()
{
    while( m_SampleQueue != nullptr )
    {
        RenderBuffer *SampleBuffer = m_SampleQueue;
        m_SampleQueue = SampleBuffer->Next;
        SAFE_DELETE( SampleBuffer );
    }
}

