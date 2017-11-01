//--------------------------------------------------------------------------------------
// WaveFileGenerator.cpp
//
// Demonstrates how to write a WAV file.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "WaveFileWriter.h"

//
//  CWaveFileWriter()
//
CWaveFileWriter::CWaveFileWriter()
{
    m_hFile = NULL;
    m_pwfxFormat = NULL;
    m_dwFormatSize = 0;
    m_pvWaveHeader = NULL;
    m_dwWaveHeaderSize = 0;
    m_dwLoopSegmentSize = 0;
    m_dwWritten = 0;
}


//
//  ~CWaveFileWriter()
//
CWaveFileWriter::~CWaveFileWriter()
{
    Close();
}


//--------------------------------------------------------------------------------------
//  Name:   Open
//  Desc:   Creates or overwrites the given file and writes WAV header information
//--------------------------------------------------------------------------------------
HRESULT	CWaveFileWriter::Open(LPCWSTR pFileName, LPCWAVEFORMATEX pwfxFormat)
{
    HRESULT hr = S_OK;

    m_hFile = CreateFile2(pFileName,
                GENERIC_WRITE,
                FILE_SHARE_WRITE,
                CREATE_ALWAYS,
                NULL);

    if(m_hFile == INVALID_HANDLE_VALUE)
    {
        hr = E_FAIL;
    }

    // Make a copy of the format
    if(SUCCEEDED(hr))
    {
        m_dwFormatSize = GetFormatSize(pwfxFormat);
        m_pwfxFormat = (LPWAVEFORMATEX)new BYTE[m_dwFormatSize];
        hr = HRFROMP(m_pwfxFormat);
    }

    if(SUCCEEDED(hr))
    {
        CopyMemory(m_pwfxFormat, pwfxFormat, m_dwFormatSize);
    }

    // Allocate the wave header
    if(SUCCEEDED(hr))
    {
        m_dwWaveHeaderSize = GetWaveHeader(m_pwfxFormat, 0, 0, NULL, 0);
        m_pvWaveHeader = new BYTE[m_dwWaveHeaderSize];
        hr = HRFROMP(m_pvWaveHeader);
    }

    // Write the header
    if(SUCCEEDED(hr))
    {
        hr = Commit();
    }

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   Commit
//  Desc:   Updates the wave header
//--------------------------------------------------------------------------------------
HRESULT	CWaveFileWriter::Commit()
{
    BOOL bResult = false;
    LARGE_INTEGER liZero = {0};

    GetWaveHeader(m_pwfxFormat, m_dwLoopSegmentSize, m_dwWritten, m_pvWaveHeader, m_dwWaveHeaderSize);

    bResult = SetEndOfFile(m_hFile);

    if(bResult)
    {
        //Move pointer to the beginning of the file
        DWORD dwReturn = SetFilePointer(m_hFile, liZero.LowPart, &liZero.HighPart, FILE_BEGIN);

        if(dwReturn == INVALID_SET_FILE_POINTER)
        {
            bResult = false;
        }
    }

    if(bResult) 
    {
        //Write header
        bResult = WriteFile(m_hFile, m_pvWaveHeader, m_dwWaveHeaderSize, NULL, NULL);
    }

    if(bResult)
    {
        //Move pointer to the end of the file
        SetFilePointer(m_hFile, liZero.LowPart, &liZero.HighPart, FILE_END);
    }

    return bResult? S_OK : E_FAIL;
}


//--------------------------------------------------------------------------------------
//  Name:   Close
//  Desc:   Commits and frees the object's resources.
//--------------------------------------------------------------------------------------
HRESULT	CWaveFileWriter::Close()
{
    HRESULT hr  = S_OK;

    // Go back and write the header
    if(m_hFile)
    {
        hr = Commit();
    }

    // Close the file
    if(m_hFile)
    {
        CloseHandle(m_hFile);
        m_hFile = NULL;
    }

    // Free memory and reset the object
    delete[] m_pwfxFormat;
    delete[] m_pvWaveHeader;

    m_dwFormatSize = 0;
    m_dwWaveHeaderSize = 0;
    m_dwWritten = 0;

    return hr;
}


//--------------------------------------------------------------------------------------
//  Name:   WriteSample
//  Desc:   Writes wave data to the file.
//--------------------------------------------------------------------------------------
HRESULT	CWaveFileWriter::WriteSample(LPVOID pvBuffer, DWORD dwBufferSize, LPDWORD pdwWritten)
{
    BOOL bResult;

    bResult = WriteFile(m_hFile, pvBuffer, dwBufferSize, pdwWritten, NULL);

    if(bResult)
    {
        m_dwWritten += pdwWritten ? *pdwWritten : dwBufferSize;
    }

    return bResult? S_OK : E_FAIL;
}

        
//--------------------------------------------------------------------------------------
//  Name:   GetWaveHeader
//  Desc:   Creates a wave file header.
//--------------------------------------------------------------------------------------
DWORD CWaveFileWriter::GetWaveHeader(LPCWAVEFORMATEX pwfxFormat, DWORD dwLoopSegmentSize, DWORD dwDataSegmentSize, LPVOID pvBuffer, DWORD dwBufferSize)
{
    LPRIFFHEADER            pChunk;
    LPDWORD                 pdwFileType;
    LPWAVEFORMATEX          pwfxDstFormat;
    DWORD                   dwRiffChunkSize;
    DWORD                   dwRiffDataSize;
    DWORD                   dwFormatChunkSize;
    DWORD                   dwFormatDataSize;
    DWORD                   dwDataChunkSize;
    DWORD                   dwReqBufferSize;

    dwRiffDataSize = sizeof(DWORD);
    dwFormatDataSize = GetFormatSize(pwfxFormat);

    dwRiffChunkSize = sizeof(RIFFHEADER) + dwRiffDataSize;
    dwFormatChunkSize = sizeof(RIFFHEADER) + dwFormatDataSize;
    dwDataChunkSize = sizeof(RIFFHEADER);

    dwReqBufferSize = dwRiffChunkSize + dwFormatChunkSize + dwDataChunkSize;

    if(dwBufferSize >= dwReqBufferSize)
    {
        pChunk = (LPRIFFHEADER)pvBuffer;

        pChunk->dwChunkId = WAVELDR_FOURCC_RIFF;
        pChunk->dwDataSize = dwRiffDataSize + dwFormatChunkSize + dwDataChunkSize + dwLoopSegmentSize + dwDataSegmentSize;

        pdwFileType = (LPDWORD)(pChunk + 1);

        *pdwFileType = WAVELDR_FOURCC_WAVE;

        pChunk = (LPRIFFHEADER)(pdwFileType + 1);

        pChunk->dwChunkId = WAVELDR_FOURCC_FORMAT;
        pChunk->dwDataSize = dwFormatDataSize;

        pwfxDstFormat = (LPWAVEFORMATEX)(pChunk + 1);

        CopyMemory(pwfxDstFormat, pwfxFormat, dwFormatDataSize);

        pChunk = (LPRIFFHEADER)((LPBYTE)pChunk + dwFormatChunkSize);

        pChunk->dwChunkId = WAVELDR_FOURCC_DATA;
        pChunk->dwDataSize = dwDataSegmentSize;
    }

    return dwReqBufferSize;
}


//--------------------------------------------------------------------------------------
//  Name:   CreateWaveHeader
//  Desc:   Creates a wave file header
//--------------------------------------------------------------------------------------
HRESULT	CWaveFileWriter::CreateWaveHeader(LPCWAVEFORMATEX pwfxFormat, DWORD dwLoopSegmentSize, DWORD dwDataSegmentSize,	LPVOID* ppvBuffer, LPDWORD pdwBufferSize)
{
    HRESULT hr;

    *pdwBufferSize = GetWaveHeader(pwfxFormat, dwLoopSegmentSize, dwDataSegmentSize, NULL, 0);
    *ppvBuffer = new BYTE[*pdwBufferSize];

    hr = HRFROMP(*ppvBuffer);

    if(SUCCEEDED(hr))
    {
        GetWaveHeader(pwfxFormat, dwLoopSegmentSize, dwDataSegmentSize, *ppvBuffer, *pdwBufferSize);
    }

    return hr;
}
