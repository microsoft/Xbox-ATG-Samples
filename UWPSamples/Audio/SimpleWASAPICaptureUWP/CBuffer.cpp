//--------------------------------------------------------------------------------------
// CBuffer.cpp
//
// Circular buffer
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "CBuffer.h"

//
//  CBuffer()
//
CBuffer::CBuffer(uint32_t size)
{
    ZeroMemory( &m_sourceFormat, sizeof( WAVEFORMATEX ) );
    ZeroMemory( &m_renderFormat, sizeof( WAVEFORMATEX ) );
    AllocateSize(size);
    m_front = 0;
    m_back = 0;
    m_sourceSampleSize = 0;
    m_renderSampleSize = 0;

    if (!InitializeCriticalSectionEx(&m_critSec, 0, 0))
    {
        DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}


//
//  ~CBuffer()
//
CBuffer::~CBuffer()
{
    ZeroMemory( &m_buffer, sizeof(m_size ) );
    ZeroMemory( &m_sourceFormat, sizeof( WAVEFORMATEX ) );
    ZeroMemory( &m_renderFormat, sizeof( WAVEFORMATEX ) );
    DeleteCriticalSection( &m_critSec );
}


//--------------------------------------------------------------------------------------
//  Name: AllocateSize
//  Desc: Allocates buffer
//--------------------------------------------------------------------------------------
void CBuffer::AllocateSize(uint32_t inSize)
{
    m_buffer = new BYTE[inSize];
	ZeroMemory(m_buffer, inSize);
    m_size = inSize;
    m_free = inSize;
}


//--------------------------------------------------------------------------------------
//  Name: SetSourceFormat
//  Desc: Sets the format of the data entering the buffering
//--------------------------------------------------------------------------------------
void CBuffer::SetSourceFormat(WAVEFORMATEX* sourceWfx)
{
    CopyMemory(&m_sourceFormat, sourceWfx, sizeof( WAVEFORMATEX));
    SetFormatCalculations();
}

                    
//--------------------------------------------------------------------------------------
//  Name: SetRenderFormat
//  Desc: Sets the format of the data stored in the buffer
//--------------------------------------------------------------------------------------
void CBuffer::SetRenderFormat(WAVEFORMATEX* renderWfx)
{
    CopyMemory(&m_renderFormat, renderWfx, sizeof( WAVEFORMATEX));
    SetFormatCalculations();

    //We can't trust the format in the buffer, so empty it
    m_front = 0;
    m_back = 0;
}


//--------------------------------------------------------------------------------------
//  Name: SetFormatCalculations
//  Desc: Sets the format of the data stored in the buffer
//--------------------------------------------------------------------------------------
void CBuffer::SetFormatCalculations()
{
	if(m_sourceFormat.cbSize != 0 && m_renderFormat.cbSize != 0)
    {
        if(m_sourceFormat.wBitsPerSample != 32 || m_renderFormat.wBitsPerSample != 32)
        {
            //We only support 32bit
            m_sourceSampleSize = 0;
            m_renderSampleSize = 0;
        }
        else
        {
            m_sourceSampleSize = m_sourceFormat.nChannels * m_sourceFormat.nBlockAlign;
            m_renderSampleSize = m_renderFormat.nChannels * m_renderFormat.nBlockAlign;
        }
    }
}


//--------------------------------------------------------------------------------------
//  Name: GetCaptureBuffer
//  Desc: Gets data from the buffer
//--------------------------------------------------------------------------------------
void CBuffer::GetCaptureBuffer(uint32_t numBytesRequested, byte *data)
{
    //Fills the data buffer with data from circular buffer and silent the rest
    uint32_t numBytesWritten = 0;
            
    if(numBytesRequested > GetCurrentUsage() || m_sourceSampleSize == 0 || m_renderSampleSize == 0)
    {
        return;
    }
            
    EnterCriticalSection( &m_critSec );

    if(numBytesRequested > m_size - m_front)
    {
        //We need to circle
        CopyMemory(data, m_buffer + m_front, m_size - m_front);
        numBytesWritten = m_size - m_front;
        m_front = 0;
    }

    CopyMemory(data + numBytesWritten, m_buffer + m_front, numBytesRequested - numBytesWritten);
    m_front += numBytesRequested - numBytesWritten;
    m_free += numBytesRequested;

    if(m_front >= m_size)
    {
        m_front -= m_size;
    }

    LeaveCriticalSection( &m_critSec );
}


//--------------------------------------------------------------------------------------
//  Name: SetCaptureBuffer
//  Desc: Add data to buffer
//--------------------------------------------------------------------------------------
void CBuffer::SetCaptureBuffer(uint32_t numBytesGiven, byte *data)
{
    if(m_sourceSampleSize == 0 || m_renderSampleSize == 0)
    {
        return;
    }
            
    EnterCriticalSection( &m_critSec );
	uint32_t numSamplesGiven = numBytesGiven/m_sourceSampleSize;

    for( uint32_t sample = 0; sample < numSamplesGiven; sample++ )
    {
		//Add mono sample
		CopyMemory(m_buffer + m_back, data + (sample*m_sourceSampleSize), m_sourceSampleSize);
        m_back += m_sourceSampleSize;

		if(m_back >= m_size)
		{
            m_back -= m_size;
		}

		//Add blank space for any additional channels
		for(DWORD channels = 1; channels < m_renderFormat.nChannels; channels++)
		{
			ZeroMemory(m_buffer + m_back, m_sourceSampleSize);
            m_back += m_sourceSampleSize;

			if(m_back >= m_size)
			{
                m_back -= m_size;
			}
		}
    }

	if(m_renderSampleSize/m_sourceSampleSize*numBytesGiven > m_free)
	{
        m_front = m_back;
        m_free = 0;
	}
	else
	{
        m_free -= numBytesGiven * m_renderSampleSize/m_sourceSampleSize;
	}

    LeaveCriticalSection( &m_critSec );
}
