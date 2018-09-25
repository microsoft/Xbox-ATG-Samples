//--------------------------------------------------------------------------------------
// XboxDmaCompression.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "XboxDmaCompression.h"
#include "StreamingDmaDecompression.h"

#include <vector>
#include <algorithm>

namespace XboxDmaCompression
{

	uint32_t CompressWithZlib_Fragment(uint8_t* pDest, uint32_t destSize, _In_ uint8_t* pSrc, uint32_t srcSize);
	//--------------------------------------------------------------------------------------
	// Name: ChunkedCompressWithZlib()
	// Desc: Compress a memory buffer using the software zlib library
	//          Based on: http://zlib.net/zlib_how.html
	//--------------------------------------------------------------------------------------
	_Use_decl_annotations_
	void ChunkedCompressWithZlib(std::vector<uint8_t*> &destFragments, std::vector<uint32_t> &compressedSizes, std::vector<uint32_t> &originalSizes, _Out_ uint32_t* fragmentCount, _In_ uint8_t* pSrc, uint32_t srcSize)
	{
		int ret;
		z_stream strm;

		// allocate deflate state
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_out = 0;
		strm.next_out = nullptr;

		// These settings match the maximum settings decompressible by the hardware decoder.  The hardware encoder in
		// instead limited to a 10 bit window, but since decompression the primary scenario, using best settings for it.
		ret = deflateInit2(&strm,           // pointer to zlib structure
			Z_BEST_COMPRESSION,              // compressionLevel = highest
			Z_DEFLATED,						 // method = use DEFLATE algorithm
			12,                              // windowBits = 4KB (largest supported by decompression hardware)
			MAX_MEM_LEVEL,					 // memLevel = default                           
			0);	                             // strategy 

		uint32_t workingBufferSize = deflateBound(&strm, srcSize);   
		uint8_t* workingBuffer = (uint8_t*)VirtualAlloc(NULL, workingBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (!workingBuffer)
		{
			throw new std::exception("Failed to allocate memory for workingBuffer");
		}


		uint32_t bytesProcessed = 0;
		int fragmentIndex = 0;
		uint32_t prospectiveCompressionBytes = srcSize;
		while (bytesProcessed < srcSize)
		{
			prospectiveCompressionBytes = std::min<uint32_t>(prospectiveCompressionBytes, srcSize - bytesProcessed);
			uint32_t requiredFragmentSpace = CompressWithZlib_Fragment(workingBuffer, workingBufferSize, pSrc + bytesProcessed, prospectiveCompressionBytes);

			while (requiredFragmentSpace > MAX_COMPRESSED_BUFFER_SIZE)
			{
				float currentRatio = (float)requiredFragmentSpace / MAX_COMPRESSED_BUFFER_SIZE;
				prospectiveCompressionBytes = (uint32_t)((prospectiveCompressionBytes / currentRatio) * 0.9f);
				prospectiveCompressionBytes &= (~3);	// ensure we're always tackling chunks that are 4 byte aligned.
				requiredFragmentSpace = CompressWithZlib_Fragment(workingBuffer, workingBufferSize, pSrc + bytesProcessed, prospectiveCompressionBytes);
			}

			if (fragmentIndex >= destFragments.size())
			{
				uint8_t* pCompressedDataBuffer = (uint8_t*)VirtualAlloc(NULL, MAX_COMPRESSED_BUFFER_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
				if (!pCompressedDataBuffer)
				{
					throw new std::exception("Failed to allocate memory for m_pCompressedDataBuffer");
				}
				else
				{
					destFragments.push_back(pCompressedDataBuffer);
					originalSizes.push_back(0);
					compressedSizes.push_back(0);
				}
			}
			memcpy_s(destFragments[fragmentIndex], MAX_COMPRESSED_BUFFER_SIZE, workingBuffer, requiredFragmentSpace);
			originalSizes[fragmentIndex] = prospectiveCompressionBytes;
			compressedSizes[fragmentIndex] = requiredFragmentSpace;


			bytesProcessed += prospectiveCompressionBytes;
			fragmentIndex++;
		}
		// clean up 
		(void)deflateEnd(&strm);
		VirtualFree(workingBuffer, 0, MEM_RELEASE);
		*fragmentCount = fragmentIndex;
	}

	uint32_t CompressWithZlib_Fragment(uint8_t* pDest, uint32_t destSize, _In_ uint8_t* pSrc, uint32_t srcSize)
	{
		int ret, flush;
		unsigned have = 0;
		z_stream strm;

		// allocate deflate state
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_out = destSize;
		strm.next_out = pDest;

		// These settings match the maximum settings decompressible by the hardware decoder.  The hardware encoder in
		// instead limited to a 10 bit window, but since decompression the primary scenario, using best settings for it.
		ret = deflateInit2(&strm,           // pointer to zlib structure
			Z_BEST_COMPRESSION,              // compressionLevel = highest
			Z_DEFLATED,						 // method = use DEFLATE algorithm
			12,                              // windowBits = 4KB (largest supported by decompression hardware)
			MAX_MEM_LEVEL,					 // memLevel = default                           
			0);                              // strategy 

		if (ret != Z_OK)
		{
			throw new std::runtime_error("zlib compression failed");
		}

		/* compress until end of file */
		strm.avail_in = srcSize;
		flush = Z_FINISH;
		strm.next_in = (Bytef*)pSrc;

		ret = deflate(&strm, flush);        // no bad return value
		assert(ret != Z_STREAM_ERROR);      // state not clobbered

		have = destSize - strm.avail_out;

		assert(strm.avail_in == 0);         // all input will be used
										    // done when last data in file processed
		assert(ret == Z_STREAM_END);        // stream will be complete 

											// clean up 
		(void)deflateEnd(&strm);
		return have;
	}


	uint32_t CompressWithZopfli_Fragment(uint8_t* pDest, _In_ uint8_t* pSrc, uint32_t srcSize);
	//--------------------------------------------------------------------------------------
	// Name: ChunkedCompressWithZopfli()
	// Desc: Compress a memory buffer using the software zopfli library
	//--------------------------------------------------------------------------------------
	void ChunkedCompressWithZopfli(std::vector<uint8_t*> &destFragments, std::vector<uint32_t> &compressedSizes, std::vector<uint32_t> &originalSizes, _Out_ uint32_t* fragmentCount, _In_ uint8_t* pSrc, uint32_t srcSize)
	{
		uint32_t bytesProcessed = 0;
		int fragmentIndex = 0;
		uint32_t prospectiveCompressionBytes = srcSize;
		while (bytesProcessed < srcSize)
		{
			prospectiveCompressionBytes = std::min<uint32_t>(prospectiveCompressionBytes, srcSize - bytesProcessed);

			// Allocate compressed fragment space if needed...
			if (fragmentIndex >= destFragments.size())
			{
				uint8_t* pCompressedDataBuffer = (uint8_t*)VirtualAlloc(NULL, MAX_COMPRESSED_BUFFER_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
				if (!pCompressedDataBuffer)
				{
					throw new std::exception("Failed to allocate memory for m_pCompressedDataBuffer");
				}
				else
				{
					destFragments.push_back(pCompressedDataBuffer);
					originalSizes.push_back(0);
					compressedSizes.push_back(0);
				}
			}

			uint32_t requiredFragmentSpace = CompressWithZopfli_Fragment(destFragments[fragmentIndex], pSrc + bytesProcessed, prospectiveCompressionBytes);

			while (requiredFragmentSpace > MAX_COMPRESSED_BUFFER_SIZE)
			{
				float currentRatio = (float)requiredFragmentSpace / MAX_COMPRESSED_BUFFER_SIZE;
				prospectiveCompressionBytes = (uint32_t)((prospectiveCompressionBytes / currentRatio) * 0.9f);
				prospectiveCompressionBytes &= (~3);	// ensure we're always tackling chunks that are 4 byte aligned.
				requiredFragmentSpace = CompressWithZopfli_Fragment(destFragments[fragmentIndex], pSrc + bytesProcessed, prospectiveCompressionBytes);
			}

			originalSizes[fragmentIndex] = prospectiveCompressionBytes;
			compressedSizes[fragmentIndex] = requiredFragmentSpace;
			bytesProcessed += prospectiveCompressionBytes;
			fragmentIndex++;
		}
		// clean up 
		*fragmentCount = fragmentIndex;
	}


	_Use_decl_annotations_
	uint32_t CompressWithZopfli_Fragment(uint8_t* pDest, _In_ uint8_t* pSrc, uint32_t srcSize)
	{
		ZopfliOptions options = { 0 };
		options.blocksplitting = true;
		options.blocksplittinglast = false;
		options.blocksplittingmax = 15;
		options.numiterations = 5;

		size_t outputBytes = 0;
		//	unsigned char zopfliBits = 0;
		unsigned char * tempOutput;

		ZopfliZlibCompress(&options,
			(const unsigned char*)pSrc,
			srcSize,
			&tempOutput,
			&outputBytes);

		if (pDest && outputBytes <= MAX_COMPRESSED_BUFFER_SIZE)
		{
			memcpy(pDest, tempOutput, outputBytes);
		}
		free(tempOutput);
		return (uint32_t)outputBytes;
	}
}