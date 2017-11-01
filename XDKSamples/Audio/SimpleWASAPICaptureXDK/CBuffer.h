//--------------------------------------------------------------------------------------
// CBuffer.h
//
// Circular buffer
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <Mmreg.h>
#include "Common.h"

class CBuffer
{
public:
    CBuffer(uint32_t size);
    virtual ~CBuffer();

    void GetCaptureBuffer(uint32_t numBytesRequested, byte *data);
    void SetCaptureBuffer(uint32_t numBytesGiven, byte *data);

    void SetSourceFormat(WAVEFORMATEX* sourceWfx);
    void SetRenderFormat(WAVEFORMATEX* renderWfx);

    uint32_t GetCurrentUsage(){ return m_size - m_free;};

private:
    void AllocateSize(uint32_t inSize);
    void SetFormatCalculations();

    WAVEFORMATEX        m_sourceFormat;
    WAVEFORMATEX        m_renderFormat;
    byte*				m_buffer;
    uint32_t	    	m_size;
    uint32_t			m_front;
    uint32_t			m_back;
    uint32_t			m_free;
    CRITICAL_SECTION    m_critSec;
    int					m_sourceSampleSize;
    int					m_renderSampleSize;
};
