//--------------------------------------------------------------------------------------
// StreamingDmaDecompression.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "pch.h"

#define MAX_COMPRESSED_BUFFER_SIZE (0x3fffe0)
#define DMA_MEMORY_ALLOCATION_SIZE (64*1024)
#define MAX_CONCURRENT_REQUESTS (MAXIMUM_WAIT_OBJECTS-2)
#define MAX_QUEUE_DEPTH_PER_FILE 3
#define READ_BLOCK_SIZE (4*1024*1024)

namespace XboxDmaCompression 
{
	struct CompressedFileHeaderChunkInfo
	{
		uint32_t CompressedSize;
		uint32_t OriginalSize;
	};

	struct CompressedFileHeader
	{
		uint32_t ChunkCount;
		#pragma warning( suppress: 4200)
		CompressedFileHeaderChunkInfo Chunks[];
	};

	enum DmaKickoffBehavior
	{
		Immediate,
		ExplicitTick,
	};

	void ShutdownStreamingDma(uint32_t waitTimeoutMs);
	void StreamingDmaExplicitTick();

	typedef void*(*AllocatorCallback)(uint32_t byteCount, PVOID param);

	BOOL ReadFileCompressed(_In_ HANDLE hFile, _Out_ PVOID* lppBuffer, _In_ DWORD nNumberOfBytesToRead, _Inout_ LPOVERLAPPED lpOverlapped, _In_ AllocatorCallback allocatorCallback, _In_ PVOID allocatorParam, _In_ LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

}
