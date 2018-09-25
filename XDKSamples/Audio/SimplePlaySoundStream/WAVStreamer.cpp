//--------------------------------------------------------------------------------------
// WAVStreamer.cpp
//
// Simple .wav file parser
//
// Xbox Advanced Technology Group (ATG).
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "WAVStreamer.h"

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
                (static_cast<uint32_t>(static_cast<uint8_t>(ch0)) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) \
                | (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))
#endif /* defined(MAKEFOURCC) */

namespace
{
    //---------------------------------------------------------------------------------
    // .WAV files
    //---------------------------------------------------------------------------------
    const uint32_t FOURCC_RIFF_TAG = MAKEFOURCC('R', 'I', 'F', 'F');
    const uint32_t FOURCC_FORMAT_TAG = MAKEFOURCC('f', 'm', 't', ' ');
    const uint32_t FOURCC_DATA_TAG = MAKEFOURCC('d', 'a', 't', 'a');
    const uint32_t FOURCC_WAVE_FILE_TAG = MAKEFOURCC('W', 'A', 'V', 'E');

    // For parsing WAV files
    struct RIFFHEADER
    {
        uint32_t fccChunkId;
        uint32_t dwDataSize;
    };
}

//--------------------------------------------------------------------------------------
// Name: RiffChunk()
// Desc: Constructor
//--------------------------------------------------------------------------------------
RiffChunk::RiffChunk() noexcept :
    m_pParentChunk(nullptr),
    m_hFile(INVALID_HANDLE_VALUE),
    m_fccChunkId(0),
    m_dwDataOffset(0),
    m_dwDataSize(0),
    m_dwFlags(0)
{
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the Riff chunk for use
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void RiffChunk::Initialize(
    uint32_t fccChunkId,
    const RiffChunk* pParentChunk,
    HANDLE hFile)
{
    m_fccChunkId = fccChunkId;
    m_pParentChunk = pParentChunk;
    m_hFile = hFile;
}


//--------------------------------------------------------------------------------------
// Name: Open()
// Desc: Opens an existing chunk
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT RiffChunk::Open()
{
    LONG lOffset = 0;

    // Seek to the first uint8_t of the parent chunk's data section
    if (m_pParentChunk)
    {
        lOffset = m_pParentChunk->m_dwDataOffset;

        // Special case the RIFF chunk
        if (FOURCC_RIFF_TAG == m_pParentChunk->m_fccChunkId)
        {
            lOffset += sizeof(uint32_t);
        }
    }

    // Read each child chunk header until we find the one we're looking for
    for (; ; )
    {
        LARGE_INTEGER Offset = { static_cast<uint32_t>(lOffset), 0 };
        if (INVALID_SET_FILE_POINTER == SetFilePointerEx(m_hFile, Offset, nullptr, FILE_BEGIN))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        RIFFHEADER rhRiffHeader;
        DWORD dwRead;
        if (0 == ReadFile(m_hFile, &rhRiffHeader, sizeof(rhRiffHeader), &dwRead, nullptr))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (dwRead != sizeof(rhRiffHeader))
        {
            return E_FAIL;
        }

        // Hit EOF without finding it
        if (0 == dwRead)
        {
            return HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT);
        }

        // Check if we found the one we're looking for
        if (m_fccChunkId == rhRiffHeader.fccChunkId)
        {
            // Save the chunk size and data offset
            m_dwDataOffset = lOffset + sizeof(rhRiffHeader);
            m_dwDataSize = rhRiffHeader.dwDataSize;

            // Success
            m_dwFlags |= RIFFCHUNK_FLAGS_VALID;

            return S_OK;
        }

        lOffset += sizeof(rhRiffHeader) + rhRiffHeader.dwDataSize;
    }
}


//--------------------------------------------------------------------------------------
// Name: ReadData()
// Desc: Reads from the file
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT RiffChunk::ReadData(LONG lOffset, void* pData, uint32_t dwDataSize, OVERLAPPED* pOL) const
{
    HRESULT hr = S_OK;

    OVERLAPPED defaultOL = {};
    OVERLAPPED* pOverlapped = pOL;
    if (!pOL)
    {
        pOverlapped = &defaultOL;
    }

    // Seek to the offset
    pOverlapped->Offset = m_dwDataOffset + lOffset;

    // Read from the file
    if (0 == ReadFile(m_hFile, pData, dwDataSize, nullptr, pOverlapped))
    {
        DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING)
            hr = HRESULT_FROM_WIN32(error);
    }

    if (SUCCEEDED(hr) && !pOL)
    {
        // we're using the default overlapped structure, which means that even if the
        // read was async, we need to act like it was synchronous.
        DWORD dwRead;
        if (!GetOverlappedResultEx(m_hFile, pOverlapped, &dwRead, INFINITE, false))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    return hr;
}


//--------------------------------------------------------------------------------------
// Name: WaveFile()
// Desc: Constructor
//--------------------------------------------------------------------------------------
WaveFile::WaveFile() noexcept :
    m_hFile(INVALID_HANDLE_VALUE)
{
}


//--------------------------------------------------------------------------------------
// Name: ~WaveFile()
// Desc: Denstructor
//--------------------------------------------------------------------------------------
WaveFile::~WaveFile()
{
    Close();
}


//--------------------------------------------------------------------------------------
// Name: Open()
// Desc: Initializes the object
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT WaveFile::Open(const wchar_t* strFileName)
{
    // If we're already open, close
    Close();

    wchar_t tmp[1024] = {};
    //_snwprintf_s( tmp, _countof( tmp ), _TRUNCATE, L"%s%s", GetContentFileRoot(), strFileName );
    _snwprintf_s(tmp, _countof(tmp), _TRUNCATE, L"%ls", strFileName);

    // Open the file
    m_hFile = CreateFile2(
        tmp,
        GENERIC_READ,
        FILE_SHARE_READ,
        OPEN_EXISTING,
        nullptr);

    if (INVALID_HANDLE_VALUE == m_hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Initialize the chunk objects
    m_RiffChunk.Initialize(FOURCC_RIFF_TAG, nullptr, m_hFile);
    m_FormatChunk.Initialize(FOURCC_FORMAT_TAG, &m_RiffChunk, m_hFile);
    m_DataChunk.Initialize(FOURCC_DATA_TAG, &m_RiffChunk, m_hFile);

    DX::ThrowIfFailed(m_RiffChunk.Open());

    DX::ThrowIfFailed(m_FormatChunk.Open());

    DX::ThrowIfFailed(m_DataChunk.Open());

    // Validate the file type
    uint32_t fccType;
    DX::ThrowIfFailed(m_RiffChunk.ReadData(0, &fccType, sizeof(fccType), nullptr));

    if (FOURCC_WAVE_FILE_TAG != fccType)
    {
        // Note this code does not support loading xWMA files (which use 'XWMA' instead of 'WAVE')
        return HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE);
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GetFormat()
// Desc: Gets the wave file format.
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT WaveFile::GetFormat(WAVEFORMATEX* pwfxFormat, size_t maxsize) const
{
    if (!pwfxFormat || (maxsize < sizeof(WAVEFORMATEX)))
    {
        return E_INVALIDARG;
    }

    uint32_t dwValidSize = m_FormatChunk.GetDataSize();

    // Must be at least as large as a WAVEFORMAT to be valid
    if (dwValidSize < sizeof(WAVEFORMAT))
    {
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }

    // Need enough space to load format
    if (dwValidSize > maxsize)
    {
        return E_FAIL;
    }

    // Read the format chunk into the buffer
    HRESULT hr = m_FormatChunk.ReadData(0, pwfxFormat, dwValidSize, nullptr);
    if (FAILED(hr))
    {
        return hr;
    }

    switch (pwfxFormat->wFormatTag)
    {
    case WAVE_FORMAT_PCM:
    case WAVE_FORMAT_IEEE_FLOAT:
        // PCMWAVEFORMAT (16 uint8_ts) or WAVEFORMATEX (18 uint8_ts)
        if (dwValidSize < sizeof(PCMWAVEFORMAT))
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }
        break;

    case WAVE_FORMAT_ADPCM:
        if (dwValidSize < sizeof(ADPCMWAVEFORMAT))
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }
        break;

    case WAVE_FORMAT_EXTENSIBLE:
        if (dwValidSize < sizeof(WAVEFORMATEXTENSIBLE))
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }
        else
        {
            static const GUID s_wfexBase = { 0x00000000, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };

            auto wfex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(pwfxFormat);

            if (memcmp(reinterpret_cast<const uint8_t*>(&wfex->SubFormat) + sizeof(uint32_t),
                reinterpret_cast<const uint8_t*>(&s_wfexBase) + sizeof(uint32_t), sizeof(GUID) - sizeof(uint32_t)) != 0)
            {
                // Unsupported!
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }

            switch (wfex->SubFormat.Data1)
            {
            case WAVE_FORMAT_PCM:
            case WAVE_FORMAT_IEEE_FLOAT:
            case WAVE_FORMAT_ADPCM:
                break;

            default:
                // Unsupported!
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
        }
        break;

    default:
        // Unsupported!
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    // Zero out remaining uint8_ts, in case enough uint8_ts were not read
    if (dwValidSize < maxsize)
    {
        memset(reinterpret_cast<uint8_t*>(pwfxFormat) + dwValidSize, 0, maxsize - dwValidSize);
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ReadSample
// Desc: Reads data from the audio file.
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT WaveFile::ReadSample(
    uint32_t dwPosition,
    void* pBuffer,
    uint32_t dwBufferSize,
    uint32_t* pdwRead) const
{
    // Don't read past the end of the data chunk
    uint32_t dwDuration = GetDuration();

    if (dwPosition + dwBufferSize > dwDuration)
    {
        dwBufferSize = dwDuration - dwPosition;
    }

    HRESULT hr = S_OK;
    if (dwBufferSize)
    {
        hr = m_DataChunk.ReadData(static_cast<LONG>(dwPosition), pBuffer, dwBufferSize, nullptr);
    }

    if (pdwRead)
    {
        *pdwRead = dwBufferSize;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Close()
// Desc: Closes the object
//--------------------------------------------------------------------------------------
void WaveFile::Close()
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }
}
