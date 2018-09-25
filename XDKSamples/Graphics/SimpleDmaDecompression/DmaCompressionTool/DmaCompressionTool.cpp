//--------------------------------------------------------------------------------------
// DmaCompressionTool.cpp
//
// Command line wrapper for the DmaCompression lib, this tool uses third party
// compression libraries to produce files consisting of a single fragmented compressed stream
// that is compatible with the DMA decompression hardware on the XboxOne.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"

using namespace XboxDmaCompression;


enum CompressionLibrary
{
	Zlib,
	Zopfli,
};

bool CompressFile(wchar_t* srcFile, wchar_t* destFile, CompressionLibrary library, bool verbose);

wchar_t* g_usageString =
L"Usage DmaCompressionTool <src> <dest> [-zlib] [-v]\n\
\n\
If <src> is a single file, it is compressed to <dest>.\n\
If <src> is a folder or wildcard, the files are compressed to the <dest> folder with the \".cmp\" extension.\n\
-zlib  faster compression, but lower ratio than the Zopfli default.\n\
-v  include compression details in output.\n";


int wmain(int argc, wchar_t* argv[])
{
	bool verbose = false;
	CompressionLibrary library = CompressionLibrary::Zopfli;
	if (argc < 3 || argc > 5)
	{
		wprintf(g_usageString);
		return -1;
	}

	for (int opt = 3; opt < argc; opt++)
	{
		if (wcslen(argv[opt]) == 2 &&
			argv[opt][0] == L'-' &&
			towlower(argv[opt][1]) == L'v')
		{
			verbose = true;
		}
		else if (wcslen(argv[opt]) == 5 &&
			argv[opt][0] == L'-' &&
			towlower(argv[opt][1]) == L'z' &&
			towlower(argv[opt][2]) == L'l' &&
			towlower(argv[opt][3]) == L'i' &&
			towlower(argv[opt][4]) == L'b')
		{
			library = CompressionLibrary::Zlib;
		}
		else
		{
			wprintf(g_usageString);
			return -1;
		}
	}

	std::vector<wchar_t*> fileNames;

	WIN32_FIND_DATA info = { 0 };
	HANDLE searchHandle = INVALID_HANDLE_VALUE;
	
	wchar_t fullSrcPath[32 * 1024];
	wchar_t fullDestPath[32 * 1024];

	uint64_t pathLength = wcslen(argv[1]);
	if ((pathLength > 3 && isalpha(argv[1][0]) && argv[1][1] == L':' && argv[1][2] == L'\\') ||
		(pathLength > 2 && (argv[1][0] == L'\\' || argv[1][0] == L'/')))
	{
		wcscat_s(fullSrcPath, _countof(fullSrcPath), argv[1]);
	}
	else
	{
		GetCurrentDirectoryW(_countof(fullSrcPath), fullSrcPath);
		wcscat_s(fullSrcPath, _countof(fullSrcPath), L"\\");
		wcscat_s(fullSrcPath, _countof(fullSrcPath), argv[1]);
	}

	pathLength = wcslen(argv[2]);
	if ((pathLength > 3 && isalpha(argv[2][0]) && argv[2][1] == L':' && argv[2][2] == L'\\') ||
		(pathLength > 2 && (argv[2][0] == L'\\' || argv[2][0] == L'/')))
	{
		wcscat_s(fullDestPath, _countof(fullDestPath), argv[2]);
	}
	else
	{
		GetCurrentDirectoryW(_countof(fullDestPath), fullDestPath);
		wcscat_s(fullDestPath, _countof(fullDestPath), L"\\");
		wcscat_s(fullDestPath, _countof(fullDestPath), argv[2]);
	}

	searchHandle = FindFirstFile(fullSrcPath, &info);

	if (searchHandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			fileNames.push_back(_wcsdup(info.cFileName));
		} while (FindNextFile(searchHandle, &info));
		FindClose(searchHandle);
	}

	wchar_t srcLocation[32 * 1024] = { 0 };

	wchar_t* namePart = fullSrcPath;
	wchar_t* nextPathseparator = max(wcschr(namePart, L'/'), wcschr(namePart, L'\\'));
	while (nextPathseparator)
	{
		namePart = nextPathseparator + 1;
		nextPathseparator = max(wcschr(namePart, L'/'), wcschr(namePart, L'\\'));
	}
	wcsncpy_s(srcLocation, _countof(srcLocation), fullSrcPath, namePart - fullSrcPath);



	if (fileNames.size() == 0)
	{
		wprintf(L"No matching files found.\n");
		wprintf(g_usageString);
		return -1;
	}
	else if (fileNames.size() == 1 && wcschr(argv[1], L'*') == NULL)
	{
		// single file mode
		wchar_t srcFile[32 * 1024] = { 0 };
		swprintf_s(srcFile, _countof(srcFile), L"%s%s", srcLocation, fileNames[0]);

		CompressFile(srcFile, fullDestPath, library, verbose);
	}
	else
	{
		// multifile mode
		for (int i = 0; i < fileNames.size(); i++)
		{

			wchar_t targetLocation[32 * 1024] = { 0 };
			wchar_t srcFile[32 * 1024] = { 0 };

			swprintf_s(srcFile, _countof(srcFile), L"%s%s", srcLocation, fileNames[i]);

			swprintf_s(targetLocation, _countof(targetLocation), argv[2]);

			wcscat_s(targetLocation, _countof(targetLocation), L"\\");
			wcscat_s(targetLocation, _countof(targetLocation), fileNames[i]);
			wcscat_s(targetLocation, _countof(targetLocation), L".dcmp");

			CompressFile(srcFile, targetLocation, library, verbose);

		}
	}

	for (int i = 0; i < fileNames.size(); i++)
	{
		free(fileNames[i]);
	}


    return 0;
}



bool CompressFile(wchar_t* srcFile, wchar_t* destFile, CompressionLibrary library, bool verbose) 
{
	HANDLE hFile = CreateFileW(srcFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		wprintf(L"Failed to load input file %s.\n", srcFile);
		return false;
	}
	// Get the file size and allocate space to load the file
	LARGE_INTEGER large;
	GetFileSizeEx(hFile, &large);
	uint32_t originalFileSize = large.LowPart; // assuming item is >4GB
	uint32_t originalDataBufferSize = originalFileSize + (DMA_MEMORY_ALLOCATION_SIZE - 1) & ~(DMA_MEMORY_ALLOCATION_SIZE - 1);
	
	BYTE* originalDataBuffer = (BYTE*)VirtualAlloc(nullptr, originalDataBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!originalDataBuffer )
	{
		wprintf(L"Failed to allocate buffer for input file %s. (0x%8x)\n", srcFile, GetLastError());
		return false;
	}

	// Clear the buffers to zero initially so we can compare them later
	ZeroMemory(originalDataBuffer, originalDataBufferSize);

	// Load the file contents into memory
	DWORD bytesRead = 0;
	if (!ReadFile(hFile, originalDataBuffer, originalDataBufferSize, &bytesRead, NULL))
	{
		wprintf(L"Failed to read input file %s.\n", srcFile);
		return false;
	}
	CloseHandle(hFile);


	hFile = CreateFileW(destFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		wprintf(L"Failed to open output file %s.\n", srcFile);
		return false;
	}



	std::vector<BYTE*> destFragments;
	std::vector<uint32_t> compressedSizes;
	std::vector<uint32_t> originalSizes;
	uint32_t fragementCount = 0;
	if (library == CompressionLibrary::Zlib)
	{
		ChunkedCompressWithZlib(destFragments, compressedSizes, originalSizes, &fragementCount, originalDataBuffer, originalFileSize);
	}
	else
	{
		ChunkedCompressWithZopfli(destFragments, compressedSizes, originalSizes, &fragementCount, originalDataBuffer, originalFileSize);
	}

	BYTE headerBuffer[4096];

	CompressedFileHeader* header = reinterpret_cast<CompressedFileHeader*>(headerBuffer);

	header->ChunkCount = fragementCount;
	for (unsigned int i = 0; i < fragementCount; i++)
	{
		header->Chunks[i].CompressedSize = compressedSizes[i];
		header->Chunks[i].OriginalSize = originalSizes[i];
	}

	DWORD bytesWritten;
	WriteFile(hFile, header, (1 + 2 * fragementCount) * sizeof(uint32_t), &bytesWritten, nullptr);
	uint32_t bytesWrittenTotal = bytesWritten;

	for (unsigned int i = 0; i < fragementCount; i++)
	{
		// round writes up to the nearest 4 bytes, decompression hardware requires this, and sacrificing these
		// bytes in the file ensures that no realignment is necessary between read DMA and decompress DMA operations at runtime
		uint32_t writeSize = (compressedSizes[i] + 3) & ~3;
		for (unsigned int b = compressedSizes[i]; b < writeSize; b++)
		{
			destFragments[i][b] = 0;
		}
		WriteFile(hFile, destFragments[i], writeSize, &bytesWritten, nullptr);
		bytesWrittenTotal += bytesWritten;
	}

	CloseHandle(hFile);

	if (verbose) 
	{
		wprintf(L"%s compressed (%d --> %d) %f%%\n",
			srcFile,
			originalFileSize,
			bytesWrittenTotal,
			(1.0f - ((bytesWrittenTotal * 1.0f) / originalFileSize)) * 100);
	}
	return true;
}