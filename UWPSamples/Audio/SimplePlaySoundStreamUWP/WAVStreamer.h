//--------------------------------------------------------------------------------------
// WAVStreamer.h
//
// Simple .wav file parser
//
// Xbox Advanced Technology Group (ATG).
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#define RIFFCHUNK_FLAGS_VALID   0x00000001

//--------------------------------------------------------------------------------------
// Name: class RiffChunk
// Desc: RIFF chunk utility class
//--------------------------------------------------------------------------------------
class RiffChunk
{
    const RiffChunk* m_pParentChunk;     // Parent chunk
    HANDLE m_hFile;
    uint32_t m_fccChunkId;       // Chunk identifier
    uint32_t m_dwDataOffset;     // Chunk data offset
    uint32_t m_dwDataSize;       // Chunk data size
    uint32_t m_dwFlags;          // Chunk flags

public:
    RiffChunk() noexcept;

    RiffChunk(const RiffChunk&) = delete;
    RiffChunk& operator =(const RiffChunk&) = delete;

    void Initialize(uint32_t fccChunkId,
        _In_opt_ const RiffChunk* pParentChunk,
        HANDLE hFile);

    _Check_return_
        HRESULT Open();

    bool IsValid() const
    {
        return !!(m_dwFlags & RIFFCHUNK_FLAGS_VALID);
    }

    _Check_return_
        HRESULT ReadData(LONG lOffset,
            _Out_writes_(dwDataSize) void* pData,
            uint32_t dwDataSize,
            _In_opt_ OVERLAPPED* pOL) const;

    uint32_t GetChunkId() const
    {
        return m_fccChunkId;
    }

    uint32_t GetDataSize() const
    {
        return m_dwDataSize;
    }

    uint32_t GetDataOffset() const
    {
        return m_dwDataOffset;
    }
};


//--------------------------------------------------------------------------------------
// Name: class WaveFile
// Desc: Wave file utility class
//--------------------------------------------------------------------------------------
class WaveFile
{
    HANDLE m_hFile;             // File handle
    RiffChunk m_RiffChunk;      // RIFF chunk
    RiffChunk m_FormatChunk;    // Format chunk
    RiffChunk m_DataChunk;      // Data chunk

public:
    WaveFile() noexcept;
    ~WaveFile();

    WaveFile(const WaveFile&) = delete;
    WaveFile& operator =(const WaveFile&) = delete;

    HRESULT Open(_In_z_ const wchar_t* strFileName);

    void Close();

    HRESULT GetFormat(_Out_ WAVEFORMATEX* pwfxFormat, _In_ size_t maxsize) const;

    HRESULT ReadSample(
        uint32_t dwPosition,
        _Out_writes_(dwBufferSize) void* pBuffer,
        uint32_t dwBufferSize,
        _Out_opt_ uint32_t* pdwRead) const;

    uint32_t GetDuration() const
    {
        return m_DataChunk.GetDataSize();
    }
};
