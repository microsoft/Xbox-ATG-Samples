//--------------------------------------------------------------------------------------
// StreamingDmaDecompression.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "StreamingDmaDecompression.h"
#include <vector>
#include <map>
#include <algorithm>
#include <concurrent_queue.h>

#if defined(D3D12X)
#include <d3dx12_x.h>
#endif

#define check(x, e) if (!(x)) {SetLastError(e); goto error;}
#define UNBUFFERED_READ_ALIGNMENT (4*1024ul)

namespace XboxDmaCompression
{
	struct CompressedFileReadRequest;
	DWORD StreamingDmaThreadProc(LPVOID param);


	struct CompressedFileReadInfo
	{
		HANDLE FileHandle;
		uint32_t DmaWorkingMemorySize;
		ULONG64 ReadCompleteTrackingBits;		// 1 bit per IO block, so stream max is 256MB with a 4MB block size
		ULONG64 ReadIssuedTrackingBits;		    // 1 bit per IO block, so stream max is 256MB with a 4MB block size
		LARGE_INTEGER* ChunkDmaTrackingPage;	// 8 bytes per dma block, 
		uint8_t* DmaWorkingBuffer;
		uint8_t* DmaWorkingBufferFirstByte;
		uint8_t* DecompressedBuffer;
		PVOID* CallerBufferPointer;
#if defined(D3D11X)
		uint64_t DmaFence;
#elif defined (D3D12X)
		Microsoft::WRL::ComPtr<ID3D12Fence>            DmaFence;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdAllocator;
#endif

		uint32_t NumberOfBytesToRead;
		uint32_t BytesReadFromDisk;
		uint32_t BytesProcessed;

		uint32_t DecompressedDataSize;
		uint32_t DecompressedDataBufferSize;

		uint8_t ReadRequestsInFlight;
		CompressedFileReadRequest* ReadRequests[MAX_QUEUE_DEPTH_PER_FILE];

		LPOVERLAPPED CallerOverlapped;
		LPOVERLAPPED_COMPLETION_ROUTINE CompletionRoutine;
		AllocatorCallback Allocator;
		PVOID CallerAllocatorParam;
		uint32_t Error;
		bool IsDmaBufferOffset;
	};

	struct CompressedFileReadRequest
	{
		OVERLAPPED Overlapped;
		uint32_t BytesRequested;
		LARGE_INTEGER Offset;
		CompressedFileReadInfo* File;
	};

	struct StreamingCompressionContext
	{
#if defined(D3D11X)
		Microsoft::WRL::ComPtr<ID3D11DeviceX>                   Device;
		Microsoft::WRL::ComPtr<ID3D11DmaEngineContextX>         DmaEngine;
#elif defined (D3D12X)
		Microsoft::WRL::ComPtr<ID3D12Device>                    Device;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>              CommandQueue;
#endif
		bool                                                    RequestExit;
		HANDLE                                                  Thread;
		HANDLE                                                  DmaDispatchEvent;
		HANDLE                                                  NewRequestEvent;
		concurrency::concurrent_queue<CompressedFileReadInfo*>* NewRequests;

		StreamingCompressionContext() :
			Device(nullptr),
#if defined(D3D11X)
			DmaEngine(nullptr),
#elif defined (D3D12X)
			CommandQueue(nullptr),
#endif
			RequestExit(FALSE),
			Thread(NULL),
			DmaDispatchEvent(NULL),
			NewRequestEvent(NULL)
		{}


#if defined(D3D11X)
		HRESULT Init(ID3D11DeviceX* device, ID3D11DmaEngineContextX* dmaEngine, DmaKickoffBehavior behavior, ULONG64 threadAffinity)
		{
			HRESULT hr;
			if (dmaEngine == nullptr)
			{
				D3D11_DMA_ENGINE_CONTEXT_DESC dmaDesc;
				ZeroMemory(&dmaDesc, sizeof(dmaDesc));
				dmaDesc.CreateFlags = D3D11_DMA_ENGINE_CONTEXT_CREATE_SDMA_2;
				hr = device->CreateDmaEngineContext(&dmaDesc, DmaEngine.GetAddressOf());
				if (!SUCCEEDED(hr))
				{
					return hr;
				}
			}
			else
			{
				DmaEngine = dmaEngine;
			}
#elif defined(D3D12X)
		HRESULT Init(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, DmaKickoffBehavior behavior, ULONG64 threadAffinity)
		{
			HRESULT hr;
			if (cmdQueue == nullptr)
			{
				D3D12XBOX_COMMAND_QUEUE_DESC descDmaQueue = {};
				ZeroMemory(&descDmaQueue, sizeof(descDmaQueue));
				descDmaQueue.Type = D3D12XBOX_COMMAND_LIST_TYPE_DMA;
				descDmaQueue.EngineOrPipeIndex = 2;
				descDmaQueue.Flags = D3D12XBOX_COMMAND_QUEUE_FLAG_PRIORITY_LOW;
				if (!SUCCEEDED(hr = device->CreateCommandQueueX(&descDmaQueue, IID_GRAPHICS_PPV_ARGS(CommandQueue.ReleaseAndGetAddressOf()))))
				{
					return hr;
				}
			}
			else
			{
				CommandQueue = cmdQueue;
			}
#endif

			Device = device;
			NewRequests = new concurrency::concurrent_queue<CompressedFileReadInfo*>();

			NewRequestEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (!NewRequestEvent)
			{
				return HRESULT_FROM_WIN32(GetLastError());
			}
			if (behavior == DmaKickoffBehavior::ExplicitTick)
			{
				DmaDispatchEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				if (!DmaDispatchEvent)
				{
					return HRESULT_FROM_WIN32(GetLastError());
				}
			}
			Thread = CreateThread(nullptr, 0, &StreamingDmaThreadProc, this, 0, nullptr);
			if (!Thread)
			{
				return HRESULT_FROM_WIN32(GetLastError());
			}

			SetThreadAffinityMask(Thread, threadAffinity);
			return S_OK;
		}

	~StreamingCompressionContext()
		{
			if (NewRequests) 
				delete NewRequests;
			if (DmaDispatchEvent) 
				CloseHandle(DmaDispatchEvent);
			if (NewRequestEvent)
				CloseHandle(NewRequestEvent);
		}


	};

	StreamingCompressionContext* g_context = nullptr;

	int ReadRequestToBlockNumber(CompressedFileReadRequest* request, CompressedFileReadInfo* file)
	{
		LARGE_INTEGER streamStartOffset;
		streamStartOffset.LowPart = file->CallerOverlapped->Offset;
		streamStartOffset.HighPart = file->CallerOverlapped->OffsetHigh;

		return (int)((request->Offset.QuadPart - streamStartOffset.QuadPart) / READ_BLOCK_SIZE);
	}

	int FindNextReadBlock(CompressedFileReadInfo* file)
	{
		if (file->Error)
		{
			return -1;
		}

		uint32_t totalReadBlocks = (file->NumberOfBytesToRead + (READ_BLOCK_SIZE - 1)) / READ_BLOCK_SIZE;

		DWORD neededBlock;
		_BitScanForward64(&neededBlock, ~file->ReadIssuedTrackingBits);

		return neededBlock < totalReadBlocks ? (int)neededBlock : -1;
	}

	BOOL IssueReadRequest(CompressedFileReadInfo* file, CompressedFileReadRequest* request, int blockNumber)
	{
		int requestSlot = -1;
		for (int r = 0; r < MAX_QUEUE_DEPTH_PER_FILE; r++)
		{
			if (file->ReadRequests[r] == nullptr)
			{
				requestSlot = r;
				break;
			}
		}
		if (file->ReadRequestsInFlight >= MAX_QUEUE_DEPTH_PER_FILE || requestSlot < 0) __debugbreak();

		
		request->Offset.LowPart = file->CallerOverlapped->Offset;
		request->Offset.HighPart = file->CallerOverlapped->OffsetHigh;

		request->Offset.QuadPart += blockNumber * READ_BLOCK_SIZE;

		uint32_t bytesToRead = std::min<uint32_t>(file->NumberOfBytesToRead - (blockNumber * READ_BLOCK_SIZE), READ_BLOCK_SIZE);

		request->File = file;
		request->BytesRequested = bytesToRead;

		// clear stale info from last read...
		request->Overlapped.Internal = 0;
		request->Overlapped.InternalHigh = 0;
		request->Overlapped.Pointer = 0;

		request->Overlapped.Offset = request->Offset.LowPart;
		request->Overlapped.OffsetHigh = request->Offset.HighPart;


		file->ReadRequests[requestSlot] = request;
		file->ReadRequestsInFlight++;

		BOOL ret;
		
		if (file->IsDmaBufferOffset)
		{
			// adjust the destination up to the next page boundary, since the prior read would have been extended to that boundary
			uint32_t readOffset = (uint32_t)(UNBUFFERED_READ_ALIGNMENT - (file->DmaWorkingBufferFirstByte - file->DmaWorkingBuffer));
			LARGE_INTEGER alignedOffset(request->Offset);
			alignedOffset.QuadPart += readOffset;
			request->Overlapped.Offset = alignedOffset.LowPart;
			request->Overlapped.OffsetHigh = alignedOffset.HighPart;
			uint32_t alignedReadSize = (bytesToRead + (UNBUFFERED_READ_ALIGNMENT - 1)) & ~(UNBUFFERED_READ_ALIGNMENT - 1); // Round the reads up to the sector size for unbuffered I/O compat

			ret = ReadFile(
				file->FileHandle,
				file->DmaWorkingBufferFirstByte + readOffset + (READ_BLOCK_SIZE * blockNumber),
				alignedReadSize,
				nullptr,
				&(request->Overlapped));
		}
		else
		{
			// A side effect on not knowing if a handle is unbuffered I/O, and with no way to determine it, all reads must be extended to 4K boundaries.
			ret = ReadFile(
				file->FileHandle,
				file->DmaWorkingBufferFirstByte + (READ_BLOCK_SIZE * blockNumber),
				(bytesToRead + (UNBUFFERED_READ_ALIGNMENT - 1)) & ~(UNBUFFERED_READ_ALIGNMENT - 1),
				nullptr,
				&(request->Overlapped));
		}

		// if this is the first block, and the read is unligned, and the read indicates invalid param, likely we have an unbuffered handle, and need to retry...
		if (!ret && GetLastError() == ERROR_INVALID_PARAMETER && blockNumber == 0 && 
			(file->DmaWorkingBufferFirstByte != file->DmaWorkingBuffer || bytesToRead % UNBUFFERED_READ_ALIGNMENT != 0))
		{
			uint32_t additionalBytes = (uint32_t)(file->DmaWorkingBufferFirstByte - file->DmaWorkingBuffer);
			LARGE_INTEGER alignedOffset(request->Offset);
			alignedOffset.QuadPart -= additionalBytes;
			request->Overlapped.Offset = alignedOffset.LowPart;
			request->Overlapped.OffsetHigh = alignedOffset.HighPart;
			uint32_t alignedReadSize = (bytesToRead + additionalBytes + (UNBUFFERED_READ_ALIGNMENT - 1)) & ~(UNBUFFERED_READ_ALIGNMENT - 1); // Round the reads up to the sector size for unbuffered I/O compat
			ret = ReadFile(
				file->FileHandle,
				file->DmaWorkingBuffer,
				alignedReadSize,
				nullptr,
				&(request->Overlapped));
			if (ret)
			{
				file->IsDmaBufferOffset = TRUE;
				SetEvent(request->Overlapped.hEvent);
			}
			else if (GetLastError() == ERROR_IO_PENDING)
			{
				file->IsDmaBufferOffset = TRUE;
			}
		}

		file->ReadIssuedTrackingBits |= (1ull << blockNumber);
		return ret;
	}


#if defined D3D11X
	void ProcessDmaKickoff(CompressedFileReadInfo* file, ID3D11DmaEngineContextX* pDma)
#elif defined D3D12X
	void ProcessDmaKickoff(CompressedFileReadInfo* file, ID3D12CommandQueue* pCmdQueue)
#endif
	{
		CompressedFileHeader* header = (CompressedFileHeader*)(file->DmaWorkingBufferFirstByte);

		// if the fence shows that all DMA decompression blocks submitted, exit
#if defined D3D11X
		if (file->DmaFence)
#elif defined D3D12X
		Microsoft::WRL::ComPtr<ID3D12XboxDmaCommandList> pCmdList;
		Microsoft::WRL::ComPtr<ID3D12Device> pDevice;
		if (file->DmaFence && file->DmaFence->GetCompletedValue())
#endif
		{
			return;
		}

		unsigned int chunk = 0;

		uint32_t chunkCompressedStreamStartOffset = 4 + (header->ChunkCount * 8);
		uint32_t chunkDecompressedStreamStartOffset = 0;

		bool allChunksQueued = true;
		bool chunksQueued = false;

		while (chunk < header->ChunkCount)
		{
			uint32_t chunkCompressedStreamEndOffset = chunkCompressedStreamStartOffset + header->Chunks[chunk].CompressedSize - 1;

			// first check is this is a chunk that's already been queued for decompression....
			if (file->ChunkDmaTrackingPage[chunk].LowPart == 0)
			{
				int chunkStartBlockId = chunkCompressedStreamStartOffset / READ_BLOCK_SIZE;
				int chunkEndBlockId = chunkCompressedStreamEndOffset / READ_BLOCK_SIZE;

				bool chunkIsContainedInLoadedBlocks = true;
				for (int b = chunkStartBlockId; b <= chunkEndBlockId; b++)
				{
					if (!(file->ReadCompleteTrackingBits & (1ull << b)))
					{
						chunkIsContainedInLoadedBlocks = false;
						allChunksQueued = false;
						break;
					}
				}

				// issue the request...
				if (chunkIsContainedInLoadedBlocks)
				{
#if defined(D3D11X)
					if (chunk == header->ChunkCount - 1)
					{
						// if this is the last chunk, fill the back of the page with zeros, since VirtualAlloc doens't appear to zero large pages
						file->ChunkDmaTrackingPage[chunk + 1].LowPart = 1;
						uint32_t zeroFillStartOffset = chunkDecompressedStreamStartOffset + (header->Chunks[chunk].OriginalSize & (~3));
						pDma->FillMemoryWithValue(file->DecompressedBuffer + zeroFillStartOffset, file->DecompressedDataBufferSize - zeroFillStartOffset, 0);
						pDma->CopyLastErrorCodeToMemory(&(file->ChunkDmaTrackingPage[chunk + 1].HighPart));
						//uint64_t clearMemFence = pDma->InsertFence(0);
						//pDma->InsertWaitOnFence(0, clearMemFence);
					}
					pDma->LZDecompressMemory(file->DecompressedBuffer + chunkDecompressedStreamStartOffset, file->DmaWorkingBufferFirstByte + chunkCompressedStreamStartOffset, header->Chunks[chunk].CompressedSize, 0);
					pDma->CopyLastErrorCodeToMemory(&(file->ChunkDmaTrackingPage[chunk].HighPart));
#elif defined(D3D12X)

					HRESULT hr;
					if (nullptr == pDevice.Get())
					{
						pCmdQueue->GetDevice(__uuidof(ID3D12Device), (void**)pDevice.ReleaseAndGetAddressOf());
					}
					if (nullptr == pCmdList.Get())
					{
						if (!SUCCEEDED(hr = pDevice->CreateCommandList(D3D12XBOX_NODE_MASK, D3D12XBOX_COMMAND_LIST_TYPE_DMA, file->CmdAllocator.Get(), nullptr, IID_GRAPHICS_PPV_ARGS(pCmdList.ReleaseAndGetAddressOf()))))
						{
							file->Error = hr;
							return;
						}
					}
					if (chunk == header->ChunkCount - 1)
					{
						file->ChunkDmaTrackingPage[chunk + 1].LowPart = 1;
						uint32_t zeroFillStartOffset = chunkDecompressedStreamStartOffset + (header->Chunks[chunk].OriginalSize & (~3));
						pCmdList->FillMemoryWith32BitValueX(
							reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(file->DecompressedBuffer + zeroFillStartOffset),
							file->DecompressedDataBufferSize - zeroFillStartOffset,
							0, D3D12XBOX_COPY_FLAG_NONE);
						pCmdList->CopyLastErrorCodeToMemoryX(
							reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(&(file->ChunkDmaTrackingPage[chunk + 1].HighPart)));
					}
					pCmdList->LZDecompressMemoryX(
						reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(file->DecompressedBuffer + chunkDecompressedStreamStartOffset),
						reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(file->DmaWorkingBufferFirstByte + chunkCompressedStreamStartOffset),
						header->Chunks[chunk].CompressedSize);

					pCmdList->CopyLastErrorCodeToMemoryX(
						reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS>(&(file->ChunkDmaTrackingPage[chunk].HighPart))
					);
#endif
					chunksQueued = true;
					file->ChunkDmaTrackingPage[chunk].LowPart = 1;
				}
			}

			chunkDecompressedStreamStartOffset += header->Chunks[chunk].OriginalSize;
			chunk++;
			chunkCompressedStreamStartOffset = chunkCompressedStreamEndOffset + 1;
			chunkCompressedStreamStartOffset = (chunkCompressedStreamStartOffset + 3) & (~3);
		}

#if defined(D3D11X)
		if (allChunksQueued)
		{
			file->DmaFence = pDma->InsertFence(0);   // Insert a fence and kickoff
		}
#elif defined(D3D12X)
		if (nullptr != pCmdList.Get())
		{
			pCmdList->Close();
			pCmdQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList*const*>(pCmdList.GetAddressOf()));
		}

		if (allChunksQueued)
		{
			pCmdQueue->Signal(file->DmaFence.Get(), 1);
		}
#endif
	}

	DWORD StreamingDmaThreadProc(LPVOID param)
	{
		StreamingCompressionContext*		    context = reinterpret_cast<StreamingCompressionContext*>(param);
		HANDLE								    requestEvents[MAX_CONCURRENT_REQUESTS + 2];
		CompressedFileReadRequest			    readRequests[MAX_CONCURRENT_REQUESTS] = { 0 };
		std::vector<CompressedFileReadRequest*>	idleRequests;

		std::vector<CompressedFileReadInfo*> filesInFlight;

		for (int i = 0; i < MAX_CONCURRENT_REQUESTS; i++)
		{
			requestEvents[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			readRequests[i].Overlapped.hEvent = requestEvents[i];
			idleRequests.push_back(&(readRequests[i]));
		}
		requestEvents[MAX_CONCURRENT_REQUESTS] = context->NewRequestEvent;
		requestEvents[MAX_CONCURRENT_REQUESTS + 1] = context->DmaDispatchEvent;

		int eventCount = context->DmaDispatchEvent != NULL ? MAX_CONCURRENT_REQUESTS + 2 : MAX_CONCURRENT_REQUESTS + 1;

		while (true)
		{
			uint32_t waitResult = WaitForMultipleObjects(eventCount, requestEvents, FALSE, 1);

			if (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + MAX_CONCURRENT_REQUESTS)
			{
				// one of the async I/O requests completed, handle it....
				int idx = waitResult - WAIT_OBJECT_0;
				CompressedFileReadRequest* readRequest = &(readRequests[idx]);
				CompressedFileReadInfo* fileInfo = readRequest->File;
				DWORD bytesRead = 0;

				if (GetOverlappedResult(fileInfo->FileHandle, &(readRequest->Overlapped), &bytesRead, FALSE))
				{
					int readBlockNumber = ReadRequestToBlockNumber(readRequest, fileInfo);

					if (readBlockNumber == 0)
					{
						uint32_t targetBytesRead = fileInfo->IsDmaBufferOffset ? bytesRead - (uint32_t)(fileInfo->DmaWorkingBufferFirstByte - fileInfo->DmaWorkingBuffer) : bytesRead;
						// this is the first block for the file
						CompressedFileHeader* header = (CompressedFileHeader*)(fileInfo->DmaWorkingBufferFirstByte);
						if (bytesRead > (fileInfo->DmaWorkingBufferFirstByte - fileInfo->DmaWorkingBuffer) &&
							targetBytesRead >= sizeof(uint32_t) && targetBytesRead >= header->ChunkCount * sizeof(uint32_t))
						{
							uint32_t compressedDataSize = 0;
							uint32_t compressedStreamSize = sizeof(uint32_t); // first 4 bytes are the chunk count...
							uint32_t decompressedDataSize = 0;

							for (unsigned int i = 0; i < header->ChunkCount; i++)
							{
								compressedDataSize += header->Chunks[i].CompressedSize;
								compressedStreamSize += 8 + ((header->Chunks[i].CompressedSize + 3) & ~3);
								decompressedDataSize += header->Chunks[i].OriginalSize;
							}

							if (compressedStreamSize == fileInfo->NumberOfBytesToRead)
							{
								uint32_t minExpectedUnbufferedBytes = READ_BLOCK_SIZE;
								if (fileInfo->DmaWorkingBufferFirstByte != fileInfo->DmaWorkingBuffer)
								{
									minExpectedUnbufferedBytes += UNBUFFERED_READ_ALIGNMENT - (uint32_t)(fileInfo->DmaWorkingBufferFirstByte - fileInfo->DmaWorkingBuffer);
								}
								minExpectedUnbufferedBytes = min(minExpectedUnbufferedBytes, fileInfo->NumberOfBytesToRead);

								// header looks good, set first tracking bit...
								assert((fileInfo->IsDmaBufferOffset == FALSE && targetBytesRead == readRequest->BytesRequested) ||
									   (fileInfo->IsDmaBufferOffset == TRUE && targetBytesRead >= minExpectedUnbufferedBytes));

								fileInfo->ReadCompleteTrackingBits |= 1;
								fileInfo->BytesReadFromDisk += targetBytesRead;

								// and since this is the first time we know the decompressed size, get the output buffer ready...
								fileInfo->DecompressedDataSize = decompressedDataSize;
								fileInfo->DecompressedDataBufferSize = (decompressedDataSize + (DMA_MEMORY_ALLOCATION_SIZE - 1)) &  ~(DMA_MEMORY_ALLOCATION_SIZE - 1);

								fileInfo->DecompressedBuffer = reinterpret_cast<uint8_t*>(fileInfo->Allocator(fileInfo->DecompressedDataBufferSize, fileInfo->CallerAllocatorParam));
								if (fileInfo->CallerBufferPointer)
								{
									*(fileInfo->CallerBufferPointer) = fileInfo->DecompressedBuffer;
								}
								fileInfo->ChunkDmaTrackingPage = (LARGE_INTEGER*)VirtualAlloc(nullptr, DMA_MEMORY_ALLOCATION_SIZE, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);

								if (!fileInfo->DecompressedBuffer || !fileInfo->ChunkDmaTrackingPage)
								{
									fileInfo->Error = (uint32_t)MAKE_SCODE(SEVERITY_ERROR, FACILITY_WIN32, ERROR_OUTOFMEMORY);
								}
								else
								{
									ZeroMemory(fileInfo->ChunkDmaTrackingPage, 1024);
								}

#if defined(D3D12X)
								if (!fileInfo->Error)
								{
									fileInfo->Error = context->Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_GRAPHICS_PPV_ARGS(fileInfo->DmaFence.ReleaseAndGetAddressOf()));
								}
#endif
							}
							else
							{
								fileInfo->Error = (uint32_t)MAKE_SCODE(SEVERITY_ERROR, FACILITY_WIN32, ERROR_DATA_CHECKSUM_ERROR);
							}
						}
						else
						{
							fileInfo->Error = (uint32_t)MAKE_SCODE(SEVERITY_ERROR, FACILITY_WIN32, ERROR_DATA_CHECKSUM_ERROR);
						}
					}
					else  // else it's a later block that just completed...
					{
						uint32_t remainingBytes = fileInfo->NumberOfBytesToRead - readBlockNumber * READ_BLOCK_SIZE;
						if (fileInfo->IsDmaBufferOffset && (fileInfo->DmaWorkingBufferFirstByte != fileInfo->DmaWorkingBuffer))
						{
							remainingBytes += (uint32_t)(fileInfo->DmaWorkingBufferFirstByte - fileInfo->DmaWorkingBuffer);
							if (remainingBytes < UNBUFFERED_READ_ALIGNMENT)
							{
								remainingBytes = 0;	// prior read overflow for alignment already grabed the last bytes...
							}
							else
							{
								remainingBytes -= UNBUFFERED_READ_ALIGNMENT;
							}
						}
						uint32_t expectedReadSize = min(READ_BLOCK_SIZE, remainingBytes); expectedReadSize;
						assert((fileInfo->IsDmaBufferOffset == FALSE && bytesRead == readRequest->BytesRequested) ||
							   (fileInfo->IsDmaBufferOffset == TRUE && bytesRead >= expectedReadSize));

						fileInfo->ReadCompleteTrackingBits |= (1ull << readBlockNumber);
						fileInfo->BytesReadFromDisk += bytesRead;
					}

					// done processing read, break the linkage....
					readRequest->File = nullptr;
					for (int r = 0; r < MAX_QUEUE_DEPTH_PER_FILE; r++)
					{
						if (fileInfo->ReadRequests[r] == readRequest)
						{
							fileInfo->ReadRequests[r] = nullptr;
							for (int rr = r + 1; rr < MAX_QUEUE_DEPTH_PER_FILE; rr++)
							{
								fileInfo->ReadRequests[rr - 1] = fileInfo->ReadRequests[rr];
								fileInfo->ReadRequests[rr] = nullptr;
							}
							break;
						}
					}
					fileInfo->ReadRequestsInFlight--;

					// check if we should kick another async request...
					int neededBlock = FindNextReadBlock(fileInfo);
					if (neededBlock != -1)
					{
						IssueReadRequest(fileInfo, readRequest, neededBlock);
					}
					else
					{
						bool requestIsIdle = true;
						// otherwise pick the first pending file that should get another
						for (int f = 0; f < filesInFlight.size(); f++)
						{
							if (filesInFlight[f]->ReadRequestsInFlight < MAX_QUEUE_DEPTH_PER_FILE)
							{
								int possibleBlock = FindNextReadBlock(filesInFlight[f]);
								if (possibleBlock != -1)
								{
									requestIsIdle = false;
									IssueReadRequest(filesInFlight[f], readRequest, possibleBlock);
									break;
								}
							}
						}
						if (requestIsIdle)
						{
							idleRequests.push_back(readRequest);
						}
					}

					// then queue the new DMA requests if we're in immediate mode, and the first chunk is in, such that allocations are done.

					if (context->DmaDispatchEvent == NULL && !fileInfo->Error && 
						(fileInfo->ReadCompleteTrackingBits & 1))
					{
#if defined(D3D11X)
						ProcessDmaKickoff(fileInfo, context->DmaEngine.Get());
#elif defined(D3D12X)
						ProcessDmaKickoff(fileInfo, context->CommandQueue.Get());
#endif
					}
				}
				else
				{
					// shouldn't ever get here...
					__debugbreak();
				}
			}
			else if (waitResult == WAIT_OBJECT_0 + MAX_CONCURRENT_REQUESTS)
			{
				// new queued request(s)...
				CompressedFileReadInfo* newFile = nullptr;
				while (context->NewRequests->try_pop(newFile))
				{

					filesInFlight.push_back(newFile);

					for (unsigned int ar = 0; ar < MAX_QUEUE_DEPTH_PER_FILE && (ar * READ_BLOCK_SIZE) < newFile->NumberOfBytesToRead; ar++)
					{
						CompressedFileReadRequest* request;
						if (idleRequests.size() > 0)
						{
							request = idleRequests.back();
							idleRequests.pop_back();
						}
						else
						{
							break;
						}
						IssueReadRequest(newFile, request, ar);

					}
				} // while additional new file requests to kick off...
			}
			else if (waitResult == WAIT_OBJECT_0 + MAX_CONCURRENT_REQUESTS + 1)
			{
				// explicit DMA kickoff request
				for (int i = 0; i < filesInFlight.size(); i++)
				{
#if defined(D3D11X)
					ProcessDmaKickoff(filesInFlight[i], context->DmaEngine.Get());
#elif defined(D3D12X)
					ProcessDmaKickoff(filesInFlight[i], context->CommandQueue.Get());
#endif
				}
			}   // end of WaitForMultipleObjects handers section
			

			// then on the periodic timeout poll the completion fences for the files that are in flight...
			for (auto itr = filesInFlight.begin(); itr != filesInFlight.end();)
			{
				CompressedFileReadInfo* file = *itr;
				CompressedFileHeader* header = reinterpret_cast<CompressedFileHeader*>(file->DmaWorkingBufferFirstByte);
#if defined(D3D11X)
				if ((file->Error && file->ReadRequestsInFlight == 0)
					|| (file->DmaFence != 0 && !context->Device->IsFencePending(file->DmaFence)))
#elif defined(D3D12X) 
				if ((file->Error && file->ReadRequestsInFlight == 0)
					|| (file->DmaFence != 0 && file->DmaFence->GetCompletedValue()))
#endif			
				{
					// if there was an I/O error, that has priority, otherwise check for Dma errors...
					if (!file->Error)
					{
						for (unsigned int c = 0; c <= header->ChunkCount; c++)
						{
							if (file->ChunkDmaTrackingPage[c].QuadPart != 1)
							{
								file->Error = MAKE_SCODE(SEVERITY_ERROR, FACILITY_XBOX, file->ChunkDmaTrackingPage[c].HighPart);
								break;
							}
						}
					}

					file->CallerOverlapped->InternalHigh = file->DecompressedDataSize;
					file->CallerOverlapped->Internal = file->Error;

					if (file->CallerOverlapped->hEvent != NULL && file->CallerOverlapped->hEvent != INVALID_HANDLE_VALUE)
					{
						SetEvent(file->CallerOverlapped->hEvent);
					}
					if (file->CompletionRoutine)
					{
						file->CompletionRoutine(file->Error, file->DecompressedDataSize, file->CallerOverlapped);
					}

					VirtualFree(file->DmaWorkingBuffer, 0, MEM_RELEASE);
					VirtualFree(file->ChunkDmaTrackingPage, 0, MEM_RELEASE);
#if defined(D3D12X)
					file->DmaFence.ReleaseAndGetAddressOf();
					file->CmdAllocator->Reset();
					file->CmdAllocator.ReleaseAndGetAddressOf();
#endif

					itr = filesInFlight.erase(itr);
					delete file;
				}
				else
				{
					itr++;
				}
			}

			if (filesInFlight.size() == 0 && context->RequestExit)
			{
				break;
			}

		} // end while loop for worker...

		delete context;
		return 0;
	}


#if defined(D3D11X)
	HRESULT InitStreamingDma11(ID3D11DeviceX* pDevice, ID3D11DmaEngineContextX* pDma, DmaKickoffBehavior behavior, ULONG64 threadAffinity)
	{
		if (g_context == nullptr)
		{
			StreamingCompressionContext* newContext = new StreamingCompressionContext(); //deleted on worker thread clean up

			if (nullptr == InterlockedCompareExchangePointer(reinterpret_cast<PVOID*>(&g_context), newContext, nullptr))
			{
				// confirmed Init ownership
				return newContext->Init(pDevice, pDma, behavior, threadAffinity);
			}
			else // else initialization race condition
			{
				free(newContext);
			}
		}
		return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
	}
#endif

#if defined(D3D12X)
	HRESULT InitStreamingDma12(ID3D12Device* pDevice, ID3D12CommandQueue* pCmdQueue, DmaKickoffBehavior behavior, ULONG64 threadAffinity)
	{
		if (g_context == nullptr)
		{
			StreamingCompressionContext* newContext = new StreamingCompressionContext(); //deleted on worker thread clean up

			if (nullptr == InterlockedCompareExchangePointer(reinterpret_cast<PVOID*>(&g_context), newContext, nullptr))
			{
				// confirmed Init ownership
				return newContext->Init(pDevice, pCmdQueue, behavior, threadAffinity);
			}
			else // else initialization race condition
			{
				free(newContext);
			}
		}
		return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
	}
#endif

	BOOL ReadFileCompressed(_In_ HANDLE hFile, _Out_ PVOID* lppBuffer, _In_ DWORD nNumberOfBytesToRead, _Inout_ LPOVERLAPPED lpOverlapped, _In_ AllocatorCallback allocatorCallback, _In_ PVOID allocatorParam, _In_ LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
	{
		StreamingCompressionContext* context = g_context;
		CompressedFileReadInfo* fileInfo = nullptr;
		AllocatorCallback defaultAllocator = [](uint32_t byteCount, PVOID param)
		{
			UNREFERENCED_PARAMETER(param);
			return VirtualAlloc(NULL, byteCount, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT);
		};

		check(0 != hFile && INVALID_HANDLE_VALUE != hFile && 0 != nNumberOfBytesToRead && nullptr != lpOverlapped, ERROR_BAD_ARGUMENTS);
		check(nullptr != context, ERROR_DEVICE_NOT_AVAILABLE);
		check(nullptr != lppBuffer || nullptr != allocatorCallback, ERROR_BAD_ARGUMENTS); // caller must provide either an pointer to export the allocated buffer to, or a custom allocator to do the same
		check(lpOverlapped->Offset % 4 == 0, ERROR_BAD_ARGUMENTS);                        // Reads must be 4 byte aligned, so ensure that any composite files with embedded compressed streams take this into account

		fileInfo = new CompressedFileReadInfo();  // deleted upon removal from in-flight list;

		uint32_t unalignedOffset = lpOverlapped->Offset % UNBUFFERED_READ_ALIGNMENT;
		if (unalignedOffset == 0)
		{
			fileInfo->DmaWorkingMemorySize = (nNumberOfBytesToRead + (DMA_MEMORY_ALLOCATION_SIZE - 1)) &  ~(DMA_MEMORY_ALLOCATION_SIZE - 1);
		}
		else
		{
			// We add an additional 4KB page here for the interim buffer, this is to account for if the handle
			// was opened with FILE_FLAG_NO_BUFFERING and we need to start the read at the preceeding 4KB boundary
			fileInfo->DmaWorkingMemorySize = (nNumberOfBytesToRead + UNBUFFERED_READ_ALIGNMENT + (DMA_MEMORY_ALLOCATION_SIZE - 1)) &  ~(DMA_MEMORY_ALLOCATION_SIZE - 1);
		}

		fileInfo->DmaWorkingBuffer = (uint8_t*)VirtualAlloc(NULL, fileInfo->DmaWorkingMemorySize, MEM_LARGE_PAGES | MEM_GRAPHICS | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GPU_COHERENT | PAGE_GPU_READONLY);
		check(nullptr != fileInfo->DmaWorkingBuffer, ERROR_OUTOFMEMORY);
		fileInfo->DmaWorkingBufferFirstByte = fileInfo->DmaWorkingBuffer + unalignedOffset;

#if defined(D3D12X)
		HRESULT hr;
		check(SUCCEEDED(hr = context->Device->CreateCommandAllocator(D3D12XBOX_COMMAND_LIST_TYPE_DMA, IID_GRAPHICS_PPV_ARGS(fileInfo->CmdAllocator.ReleaseAndGetAddressOf()))), hr);
#endif

		fileInfo->ReadIssuedTrackingBits = 0;
		fileInfo->ReadCompleteTrackingBits = 0;
		fileInfo->FileHandle = hFile;
		fileInfo->NumberOfBytesToRead = nNumberOfBytesToRead;
		fileInfo->CallerOverlapped = lpOverlapped;
		fileInfo->CompletionRoutine = lpCompletionRoutine;
		fileInfo->BytesReadFromDisk = 0;
		fileInfo->BytesProcessed = 0;
		fileInfo->ReadRequestsInFlight = 0;
		fileInfo->Allocator = (allocatorCallback != nullptr ? allocatorCallback : defaultAllocator);
		fileInfo->CallerAllocatorParam = allocatorParam;
		fileInfo->CallerBufferPointer = lppBuffer;
		// Actually unknown, assume false until first read determines if an unbuffered read with an offset is required.
		// Alternatively, API could be changed to take this as a per-read additional parameter
		fileInfo->IsDmaBufferOffset = FALSE;

		context->NewRequests->push(fileInfo);
		SetEvent(context->NewRequestEvent);

		SetLastError(ERROR_IO_PENDING);
		lpOverlapped->Internal = STATUS_PENDING;  // to be API compatible with existing async IO

		return FALSE;

	error:
		if (fileInfo && fileInfo->DmaWorkingBuffer)
			VirtualFree(fileInfo->DmaWorkingBuffer, 0, MEM_RELEASE);
		if (fileInfo)
			delete fileInfo;
		return FALSE;
	}

	void StreamingDmaExplicitTick()
	{
		StreamingCompressionContext* context = g_context;
		SetEvent(context->DmaDispatchEvent);
	}

	void ShutdownStreamingDma(uint32_t waitTimeoutMs)
	{
		StreamingCompressionContext* context = g_context;
		HANDLE thread = context->Thread;
		context->RequestExit = 1;
		if (waitTimeoutMs > 0)
		{
			WaitForSingleObject(thread, waitTimeoutMs);
			CloseHandle(thread);
		}
		g_context = nullptr;
	}
}

