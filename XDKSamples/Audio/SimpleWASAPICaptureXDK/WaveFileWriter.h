//--------------------------------------------------------------------------------------
// WaveFileWriter.h
//
// Demonstrates how to write a WAV file.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef WAVE_FILE_WRITER_H_INCLUDED
#define WAVE_FILE_WRITER_H_INCLUDED

#include "pch.h"
#include "fileapi.h"
typedef const WAVEFORMATEX *LPCWAVEFORMATEX;

#ifndef HRFROMP
#   define HRFROMP(P) ((P) != nullptr ? S_OK : E_OUTOFMEMORY)
#endif

typedef struct
{
    DWORD           dwChunkId;
    DWORD           dwDataSize;
} RIFFHEADER, *LPRIFFHEADER;

enum
{
    WAVELDR_FOURCC_RIFF         = 'FFIR',
    WAVELDR_FOURCC_WAVE         = 'EVAW',
    WAVELDR_FOURCC_FORMAT       = ' tmf',
    WAVELDR_FOURCC_DATA         = 'atad',
};

class CWaveFileWriter
{
public:
    CWaveFileWriter(void);
    virtual ~CWaveFileWriter(void);

    // Initialization/termination
    HRESULT Open(LPCWSTR	pFileName, LPCWAVEFORMATEX pwfxFormat);
    HRESULT Commit(void);
    HRESULT Close(void);

    // File data
    HRESULT WriteSample(LPVOID pvBuffer, DWORD dwBufferSize, LPDWORD pdwWritten = NULL);

    // File header
    static DWORD GetWaveHeader(LPCWAVEFORMATEX pwfxFormat, DWORD dwLoopSegmentSize, DWORD dwDataSegmentSize, LPVOID pvBuffer, DWORD dwSize);
    static HRESULT CreateWaveHeader(LPCWAVEFORMATEX pwfxFormat, DWORD dwLoopSegmentSize, DWORD dwDataSegmentSize, LPVOID *ppvBuffer, LPDWORD pdwSize);
    static DWORD GetFormatSize(LPCWAVEFORMATEX pwfxFormat);

protected:
    HANDLE					m_hFile;             // File handle
    LPWAVEFORMATEX          m_pwfxFormat;        // File format
    DWORD                   m_dwFormatSize;      // Format size
    LPVOID                  m_pvWaveHeader;      // Wave header
    DWORD                   m_dwWaveHeaderSize;  // Wave header size
    DWORD                   m_dwWritten;         // Total amount of data writen to the data segment
    DWORD                   m_dwLoopSegmentSize; // Size of the loop chunk, if one was written
};

__forceinline DWORD CWaveFileWriter::GetFormatSize(LPCWAVEFORMATEX pwfxFormat)
{
    if(WAVE_FORMAT_PCM == pwfxFormat->wFormatTag)
    {
        return sizeof(*pwfxFormat) - sizeof(pwfxFormat->cbSize);
    }
    else
    {
        return sizeof(*pwfxFormat) + pwfxFormat->cbSize;
    }
}

#endif