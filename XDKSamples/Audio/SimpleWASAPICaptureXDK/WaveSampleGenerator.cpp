//--------------------------------------------------------------------------------------
// WaveSampleGenerator.cpp
//
// Demonstrates how to play an in-memory WAV file via WASAPI.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "WaveSampleGenerator.h"

//--------------------------------------------------------------------------------------
//  Name:   WaveSampleReader
//  Desc:   Class that can read from WAVE data and convert it to floating point
//--------------------------------------------------------------------------------------

class WaveSampleReader
{
public:
    WaveSampleReader( const BYTE* pWaveData, DWORD waveSize, WAVEFORMATEXTENSIBLE* pWfx );

    ~WaveSampleReader()
    {
    }

    void ReadBlock( FLOAT* pOutput, UINT32 nMaxChannels );

    bool IsEOF( void ) const 
    {
        return ( ( m_pCurrent - m_pBase ) >= m_nSize );
    }

private:

    const BYTE* m_pBase;
    const BYTE* m_pCurrent;
    UINT32      m_nSize;
    UINT32      m_nBitsPerSample;
    UINT32      m_nChannels;
    UINT32      m_nBlockAlign;
    bool        m_bIsFloat;
};

//--------------------------------------------------------------------------------------
//  Name:   WaveSampleReader
//--------------------------------------------------------------------------------------

WaveSampleReader::WaveSampleReader( const BYTE* pWaveData, DWORD waveSize, WAVEFORMATEXTENSIBLE* pWfx ) :
    m_pBase( pWaveData ),
    m_pCurrent( pWaveData ),
    m_nSize( waveSize )
{
    if ( ( pWfx->Format.wFormatTag == WAVE_FORMAT_PCM ) ||
            ( pWfx->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && pWfx->SubFormat == KSDATAFORMAT_SUBTYPE_PCM ) )
    {
        m_bIsFloat = false;
        m_nBitsPerSample = pWfx->Format.wBitsPerSample;

        m_nChannels = pWfx->Format.nChannels;
        m_nBlockAlign = pWfx->Format.nBlockAlign;
    }
    else if ( ( pWfx->Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT ) ||
        ( pWfx->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && pWfx->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT ) ) 
    {
        m_bIsFloat = true;
        m_nBitsPerSample = pWfx->Format.wBitsPerSample;

        m_nChannels = pWfx->Format.nChannels;
        m_nBlockAlign = pWfx->Format.nBlockAlign;
    }
}


//--------------------------------------------------------------------------------------
//  Name:   ReadBlock
//  Desc:   Read one block's worth of sample data, convert it to floating point and
//          store as many channels as is possible in the array provided.
//--------------------------------------------------------------------------------------

void WaveSampleReader::ReadBlock( FLOAT* pOutput, UINT32 nMaxChannels )
{
    if ( !IsEOF() )
    {
        const UINT32  nCopyChannels = std::min( nMaxChannels, m_nChannels );
        const UINT32  nSampleIncrement = m_nBitsPerSample / 8;

        const BYTE* pSample = m_pCurrent;
        UINT32  nChannel = 0;

        //  Copy as many channels as we can
        for ( ; nChannel < nCopyChannels; ++nChannel )
        {
            FLOAT   fValue = 0;

            if ( m_bIsFloat )
            {
                fValue = *(FLOAT *)pSample;
            }
            else
            {
                switch ( m_nBitsPerSample )
                {
                case 16:
                    fValue = *(INT16 *)( pSample ) / ( (FLOAT)INT16_MAX + 1.0f );
                    break;

                case 24:
                    {
                        //  Convert from 24 bit to 32 bit, replicating top byte into
                        //  low byte.
                        const UINT32 unpacked = ( ( (UINT32)pSample[2] & 0xFFU ) << 24 ) |
                            ( ( (UINT32)pSample[1] & 0xFFU ) << 16 ) |
                            ( ( (UINT32)pSample[0] & 0xFFU ) << 8 ) |
                            ( ( (UINT32)pSample[2] & 0xFFU ) );
                        fValue = ( (INT32)unpacked ) / ( (FLOAT)INT32_MAX + 1.0f );
                    }
                    break;

                case 32:
                    fValue = *(INT32 *)( pSample ) / ( (FLOAT)INT32_MAX + 1.0f );
                    break;
                }
            }
            pOutput[ nChannel ] = fValue;
            pSample += nSampleIncrement;
        }

        //  Fill remaining channels with zero
        for ( ; nChannel < nMaxChannels; ++nChannel )
        {
            pOutput[ nChannel ] = 0.0f;
        }

        //  Advance current pointer by one block
        m_pCurrent += m_nBlockAlign;
    }
    else
    {
        //  If we're at EOF then just clear the output buffer
        ZeroMemory( pOutput, nMaxChannels * sizeof( FLOAT ) );
    }
}


//--------------------------------------------------------------------------------------
//  Name:   WaveSampleWriter
//  Desc:   Class that accepts floating point sample data, and can store it in an
//          alternative representation.
//--------------------------------------------------------------------------------------

class WaveSampleWriter
{
public:
    WaveSampleWriter( BYTE* pBuffer, DWORD bufferSize, WAVEFORMATEX* pWfx );

    ~WaveSampleWriter()
    {
    }

    void WriteBlock( const FLOAT* pInput, UINT32 nMaxChannels );

    bool IsEOF( void ) const 
    {
        return ( ( m_pCurrent - m_pBase ) >= m_nSize );
    }

private:

    BYTE*       m_pBase;
    BYTE*       m_pCurrent;
    UINT32      m_nSize;
    UINT32      m_nBitsPerSample;
    UINT32      m_nChannels;
    UINT32      m_nBlockAlign;
    bool        m_bIsFloat;
};


//--------------------------------------------------------------------------------------
//  Name: WaveSampleWriter
//--------------------------------------------------------------------------------------

WaveSampleWriter::WaveSampleWriter( BYTE* pBuffer, DWORD bufferSize, WAVEFORMATEX* pWfx ) :
    m_pBase( pBuffer ),
    m_pCurrent( pBuffer ),
    m_nSize( bufferSize )
{
    if ( ( pWfx->wFormatTag == WAVE_FORMAT_PCM ) ||
        ( pWfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && ((WAVEFORMATEXTENSIBLE*)pWfx)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM ) )
    {
        m_bIsFloat = false;
        m_nBitsPerSample = pWfx->wBitsPerSample;

        m_nChannels = pWfx->nChannels;
        m_nBlockAlign = pWfx->nBlockAlign;
    }
    else if ( ( pWfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ) ||
        ( pWfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && ((WAVEFORMATEXTENSIBLE*)pWfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT ) ) 
    {
        m_bIsFloat = true;
        m_nBitsPerSample = pWfx->wBitsPerSample;

        m_nChannels = pWfx->nChannels;
        m_nBlockAlign = pWfx->nBlockAlign;
    }
}


//--------------------------------------------------------------------------------------
//  Name: WriteBlock
//  Desc: Given a number of floating point samples, write as many as we can
//        into our output buffer.
//--------------------------------------------------------------------------------------

void WaveSampleWriter::WriteBlock( const FLOAT* pInput, UINT32 nMaxChannels )
{
    if ( !IsEOF() )
    {
        const UINT32  nCopyChannels = std::min( nMaxChannels, m_nChannels );
        const UINT32  nSampleIncrement = m_nBitsPerSample / 8;

        BYTE*   pSample = m_pCurrent;
        UINT32  nChannel = 0;

        //  Copy as many channels as we can
        for ( ; nChannel < nCopyChannels; ++nChannel )
        {
            FLOAT   fValue = pInput[ nChannel ];

            if ( m_bIsFloat )
            {
                *(FLOAT *)( pSample ) = fValue;
            }
            else
            {
                switch ( m_nBitsPerSample )
                {
                case 16:
                    *(INT16 *)( pSample ) = (INT16)( fValue * ( (FLOAT)INT16_MAX ) );
                    break;

                case 32:
                    *(INT32 *)( pSample ) = (INT32)( fValue * ( (FLOAT)INT32_MAX ) );
                    break;
                }
            }

            pSample += nSampleIncrement;
        }

        //  Fill remaining channels with zero
        ZeroMemory( pSample, ( m_nChannels - nChannel ) * nSampleIncrement );

        //  Advance current pointer by one block
        m_pCurrent += m_nBlockAlign;
    }
}

//--------------------------------------------------------------------------------------
//  Name: WaveSampleGenerator()
//--------------------------------------------------------------------------------------
WaveSampleGenerator::WaveSampleGenerator()
{
    m_SampleQueue = nullptr;
    m_SampleQueueTail = &m_SampleQueue;
}


//--------------------------------------------------------------------------------------
//  Name: ~WaveSampleGenerator()
//--------------------------------------------------------------------------------------

WaveSampleGenerator::~WaveSampleGenerator()
{
    // Flush unused samples
    Flush();
}


//--------------------------------------------------------------------------------------
//  Name: GenerateSampleBuffer()
//  Desc: Create a linked list of sample buffers
//--------------------------------------------------------------------------------------

HRESULT WaveSampleGenerator::GenerateSampleBuffer( BYTE* pWaveData, DWORD waveSize, WAVEFORMATEXTENSIBLE* pSourceWfx, UINT32 framesPerPeriod, WAVEFORMATEX *pWfx )
{
    HRESULT hr = S_OK;

    int sampleRatio = pWfx->nSamplesPerSec / pSourceWfx->Format.nSamplesPerSec;

    UINT32 renderBufferSizeInBytes = framesPerPeriod * pWfx->nBlockAlign;
    UINT32 renderDataLength = ( waveSize / pSourceWfx->Format.nBlockAlign * pWfx->nBlockAlign ) + ( renderBufferSizeInBytes - 1 );
    UINT32 renderBufferCount = renderDataLength * sampleRatio / renderBufferSizeInBytes;

    WaveSampleReader    reader( pWaveData, waveSize, pSourceWfx );

    for( UINT32 i = 0; i < renderBufferCount; i++ )
    {
        RenderBuffer*   sampleBuffer = new RenderBuffer();

        sampleBuffer->BufferSize = renderBufferSizeInBytes;
        sampleBuffer->BytesFilled = renderBufferSizeInBytes;
        sampleBuffer->Buffer = new BYTE[ renderBufferSizeInBytes ];

        WaveSampleWriter    writer( sampleBuffer->Buffer, sampleBuffer->BufferSize, pWfx );

        while ( !writer.IsEOF() )
        {
            static const UINT32 nMaxChannels = 8;
            FLOAT               channelData[ nMaxChannels ];

            //  Get a block of channel data (as much as we're interested in).
            //  If the reader is at end of file then this returns a block
            //  of zeroed samples.
            reader.ReadBlock( channelData, nMaxChannels );

            for(int j=0; j<sampleRatio; j++)
            {
                writer.WriteBlock( channelData, nMaxChannels );
            }
        }

        *m_SampleQueueTail = sampleBuffer;
        m_SampleQueueTail = &sampleBuffer->Next;
    }
    return hr;
}


//--------------------------------------------------------------------------------------
//  Name: FillSampleBuffer()
//  Desc: File the Data buffer of size BytesToRead with the first item in the queue.  
//  Caller is responsible for allocating and freeing buffer
//--------------------------------------------------------------------------------------

HRESULT WaveSampleGenerator::FillSampleBuffer( UINT32 bytesToRead, BYTE *pData )
{
    if (nullptr == pData)
    {
        return E_POINTER;
    }

    RenderBuffer *sampleBuffer = m_SampleQueue;

    if (bytesToRead > sampleBuffer->BufferSize)
    {
        return E_INVALIDARG;
    }

    CopyMemory( pData, sampleBuffer->Buffer, bytesToRead );

    m_SampleQueue = m_SampleQueue->Next;

    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Name: Flush()
//  Desc: Remove and free unused samples from the queue
//--------------------------------------------------------------------------------------

void WaveSampleGenerator::Flush()
{
    while( m_SampleQueue != nullptr )
    {
        RenderBuffer *sampleBuffer = m_SampleQueue;
        m_SampleQueue = sampleBuffer->Next;
    }
}
