//--------------------------------------------------------------------------------------
// MyD3DCompiler.cpp
//
// Defines the entry point for the console application.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <assert.h>
#include <regex>
#include <stdio.h>
#include <string>
#include <d3dcompiler_xdk.h>

#include <wrl/client.h>

#include "Shlwapi.h"

using Microsoft::WRL::ComPtr;

namespace
{
    void PrintMessage(_In_z_ _Printf_format_string_ const char* fmt, ...)
    {
        char  buf[1024] = {};

        va_list ap;

        va_start(ap, fmt);

        vsprintf_s(buf, _countof(buf) - 1, fmt, ap);
        buf[_countof(buf) - 1] = 0;

        va_end(ap);

        OutputDebugStringA(buf);
        printf(buf);
    }
}

int __cdecl wmain(_In_ int argc, _In_z_count_(argc) wchar_t* argv[])
{
    // Usage
    // argv[1] -- Flags
    // argv[2] -- Path to HLSL input
    // argv[3] -- Path to binary output
    // argv[4] -- Path to updb output (optional)
    if (argc < 4)
    {
        PrintMessage("Usage: compiler.exe flags HlslFilename BinaryOutputFilename [ShaderPdbFileName]\n"
                     "Shader PDB is always written even if the filename is not specified\n");
        return -1;
    }

    uint32_t flags = 0;
    swscanf_s(argv[1], L"%d", &flags);

    // Extreme size reduction. PIX PDB must be present for PIX to operate properly.
    bool    dxbcLeanAndMean = (flags & 4) != 0;    // 4 -- extreme stripping

    // Whether to embed the PDB path into the shader. Optional.
    bool    embedPdbPath = (flags & 1) == 0;        // 1 - disable pdb path embedding

    // Some extra stripping.
    bool    onlyPrecompilePrimaryStages = (flags & 2) == 0; // 2 -- disable primary stages only

    const char* entryPoint = "main";
    const char* target = "ps_5_0";

    // Externally user specified PDB filename. Otherwise this tool auto-generates the hash.pdb
    // filename next to the output file and stores PDB in there.
    bool pdbNameSpecifiedByUser = argc > 4;

    wchar_t hlslPath[MAX_PATH] = {};
    wchar_t binPath[MAX_PATH] = {};
    wchar_t pdbPath[MAX_PATH] = {};

    wcscpy_s(hlslPath, argv[2]);
    wcscpy_s(binPath, argv[3]);
    
    if (pdbNameSpecifiedByUser)
    {
        wcscpy_s(pdbPath, argv[4]);
    }
    else
    {
        ZeroMemory(pdbPath, sizeof(pdbPath));
    }

    PrintMessage("hlsl = %ls\noutput = %ls\npdb = %ls\n",
        hlslPath,
        binPath,
        pdbNameSpecifiedByUser ? pdbPath : L"<will be generated from the shader hash>");

    // Optional #defines we can pass into the shader compiler
    std::vector<D3D_SHADER_MACRO>   defines;

    // __XBOX_DISABLE_DXBC
    //     a very powerful size reducer, but the PDB must be available for the PIX captures
    // __XBOX_DISABLE_UNIQUE_HASH_EMPLACEMENT
    //     very minor size reducer, no side effects
    // __XBOX_FULL_PRECOMPILE_PROMISE
    //     on DX11 reduces the size of shader memory at runtime, does nothing on DX12
    // __XBOX_DISABLE_SHADER_NAME_EMPLACEMENT
    //     very minor size reducer, no side effects
    static const D3D_SHADER_MACRO disableDxbc       = {"__XBOX_DISABLE_DXBC", "1"};
    static const D3D_SHADER_MACRO disableUniqueHash = {"__XBOX_DISABLE_UNIQUE_HASH_EMPLACEMENT", "1"};
    static const D3D_SHADER_MACRO fullPrecompile    = {"__XBOX_FULL_PRECOMPILE_PROMISE", "1"};
    static const D3D_SHADER_MACRO disableName       = {"__XBOX_DISABLE_SHADER_NAME_EMPLACEMENT", "1"};
    static const D3D_SHADER_MACRO disableEs         = {"__XBOX_DISABLE_PRECOMPILE_ES", "1"};
    static const D3D_SHADER_MACRO disableLs         = {"__XBOX_DISABLE_PRECOMPILE_LS", "1"};
    static const D3D_SHADER_MACRO terminator        = {};

    if (dxbcLeanAndMean)
    {
        defines.push_back(disableDxbc);
        defines.push_back(disableUniqueHash);
        defines.push_back(fullPrecompile);
        defines.push_back(disableName);
    }

    // Further size reductions can be gained if certain hardware shader stages can't be used
    // For example, if a VS is never used with a GS then specify __XBOX_DISABLE_PRECOMPILE_ES
    // This could give a noticeable size reduction as well.
    if (onlyPrecompilePrimaryStages)
    {
        defines.push_back(disableEs);
        defines.push_back(disableLs);
    }

    defines.push_back(terminator);

    // Compile shader.
    // This calls into the D3DCompile first, produces the DXBC, then it passes that DXBC
    // to the Shader Compiler (SC) to produce precompiled Xbox shader objects. It returns a D3D Blob
    // with multiple chunks on it that are of interest.
    //
    // D3D_BLOB_PDB -- the shader PDB with embedded source/line info and other information important to PIX
    //                 including the shader hash (below).
    //
    // D3D_BLOB_XBOX_SHADER_HASH -- the shader hash that PIX will use to find the PDB later. This hash
    //                              can be used for de-duplication. This chunk is Read Only.
    //
    // D3D_BLOB_XBOX_PDB_PATH -- this chunk is not populated by D3DCompile, but can be set by the tools
    //                           to pass the pdb path into PIX. It can also be omitted, then PIX will look
    //                           for the PDB by using the hash.pdb filename + path set in the settings.
    //

    // Time compilation
    UINT64 qpf;
    QueryPerformanceFrequency((LARGE_INTEGER*)&qpf);

    UINT64 startTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&startTime);

    HRESULT hr = S_OK;

    ComPtr<ID3DBlob> pCode;
    ComPtr<ID3DBlob> pErrorMsgs;
    hr = D3DCompileFromFile(
        hlslPath,
        defines.data(),
        nullptr, 
        entryPoint,
        target,
        D3DCOMPILE_DEBUG, 
        0, 
        pCode.GetAddressOf(), 
        pErrorMsgs.GetAddressOf());
    if (FAILED(hr))
    {
        PrintMessage("D3DCompileFromFile returned error (0x%x), message = %s\n",
                     hr,
                     pErrorMsgs->GetBufferPointer());
        return -1;
    }

    UINT64 endTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&endTime);

    // Retireve the shader hash. Shader hash uniquely represents shader inputs. Specific shader permutations will
    // have the same hash (VS/ES/LS).
    // PIX uses it to automatically find the shader pdb when the path isn't available. If a shader pdb gets moved
    // around then the best thing to do is to name the PDB with hash.pdb as illustrated below.
    ComPtr<ID3DBlob> pHash;
    hr = D3DGetBlobPart( pCode->GetBufferPointer(), pCode->GetBufferSize(), D3D_BLOB_XBOX_SHADER_HASH, 0, pHash.GetAddressOf());
    if (FAILED(hr))
    {
        // We fail in this case.
        // If no hash has been produced (older compiler?) and no PDB path has been set, error out. PIX needs
        // to be able to find the PDB and if there is no hash and no path then it's impossible. When the hash isn't in
        // the output, it's also hard work for PIX to compare PDB and the shader object, but we still support it.
        PrintMessage("D3D_BLOB_XBOX_SHADER_HASH isn't found on the compiled D3D Blob. This is probably caused by an old version of the compilers and is deprecated. Failing compilation.\n");
        return -1;
    }
    
    // Extract the hash
    const unsigned long long hash = *(unsigned long long*)pHash->GetBufferPointer();

    // Free memory
    pHash.Reset();

    // Print out some fun stats
    PrintMessage("hash = %I64x, compilation time %d ms, output blob size is %d bytes before stripping\n",
        hash,
        (endTime - startTime) / (qpf / 1000),
        pCode->GetBufferSize());


    // Generate our own PDB filename. It should be hash.pdb
    if (!pdbNameSpecifiedByUser)
    {
        // 1. Separate path from the binary filename
        wchar_t* fname = PathFindFileName(binPath);
        const size_t pathCharCount = fname - binPath;
        wchar_t path[MAX_PATH] = {};
        if (pathCharCount)
        {
            wcsncpy_s(path, binPath, pathCharCount);
        }
        path[pathCharCount] = 0;

        // 2. Generate the good PIX friendly PDB name and paste it back with the path
        swprintf(pdbPath, _countof(pdbPath), L"%s%I64x.pdb", path, hash);
    }

    // The new recommended system is this.
    // 1. Decide if you want to embed the pdb path (PIX can now search for PDBs automatically if the hash is present)
    // 2. Decide if you want a custom name or a canonical hash.pdb name (canonical is easy for PIX to find automatically even
    //    if the PDB has moved).
    // 3. Store the name in the D3D Blob
    // Embedding the path was previously done with #define __XBOX_PDBFILENAME. It's deprecated.
    if (embedPdbPath)
    {
        // Convert unicode to UTF8
        char temp[MAX_PATH+1] = {};
        WideCharToMultiByte(CP_UTF8, 0, pdbPath, (int)wcslen(pdbPath), temp, _countof(temp) - 1, nullptr, 0);

        ComPtr<ID3DBlob> pNewPart;

        hr = D3DSetBlobPart(
            pCode->GetBufferPointer(),
            pCode->GetBufferSize(),
            D3D_BLOB_XBOX_PDB_PATH,
            0,
            temp,
            strlen(temp)+1,
            pNewPart.GetAddressOf());

        if (FAILED(hr))
        {
            PrintMessage("D3DSetBlobPart (D3D_BLOB_XBOX_PDB_PATH) returned error (0x%x)\n", hr);
            return -1;
        }

        pCode.Swap(pNewPart);
    }

    // Retrieve the PDB chunk blob from the d3d blob
    ComPtr<ID3DBlob> pPDB;
    hr = D3DGetBlobPart(pCode->GetBufferPointer(), pCode->GetBufferSize(), D3D_BLOB_PDB, 0, pPDB.GetAddressOf());
    if (FAILED(hr))
    {
        PrintMessage("D3DGetBlobPart (D3D_BLOB_PDB) returned error (0x%x), message = %s\n",
                     hr,
                     pErrorMsgs->GetBufferPointer());
        return -1;
    }

    // Save the PDB chunk
    hr = D3DWriteBlobToFile(pPDB.Get(), pdbPath, TRUE);
    if (FAILED(hr))
    {
        PrintMessage("D3DWriteBlobToFile (D3D_BLOB_PDB) returned error (0x%x)\n", hr);
        return -1;
    }

    PrintMessage("PDB size %d is written here: %ls\n",
        pPDB->GetBufferSize(),
        pdbPath);

    pPDB.Reset();

    // Now we got to the D3D blob stripping. D3D blob contains multiple chunks that can be stripped for size.
    //
    // We have already saved off the PDB and are ready to strip it, but reflection data can be large as well. If you don't
    // want to carry reflection data over into your game at runtime, strip it here. If the tools need to use it, then save
    // it akin to what the PDB does above for your tools before stripping.
    //
    // Here we do this to save maximum amount of space
    //      D3DCOMPILER_STRIP_DEBUG_INFO      |
    //      D3DCOMPILER_STRIP_REFLECTION_DATA |
    //      D3DCOMPILER_STRIP_TEST_BLOBS      |
    //      D3DCOMPILER_STRIP_PRIVATE_DATA
    //

    const uint32_t stripFlags = D3DCOMPILER_STRIP_DEBUG_INFO      |
                            D3DCOMPILER_STRIP_REFLECTION_DATA |
                            D3DCOMPILER_STRIP_TEST_BLOBS      |
                            D3DCOMPILER_STRIP_PRIVATE_DATA;

    // Strip the D3D blob
    ComPtr<ID3DBlob> pPostStripCode;
    hr = D3DStripShader(pCode->GetBufferPointer(), pCode->GetBufferSize(), stripFlags, pPostStripCode.GetAddressOf());
    if (FAILED(hr))
    {
        PrintMessage("D3DStripShader returned error (0x%x)\n", hr);
        return -1;
    }

    PrintMessage("Stripped the blob. Size before %d, size after %d, reduction %d\n",
        pCode->GetBufferSize(),
        pPostStripCode->GetBufferSize(),
        pCode->GetBufferSize() - pPostStripCode->GetBufferSize());

    pCode.Reset();

    // Write shader binary blob to file
    hr = D3DWriteBlobToFile(pPostStripCode.Get(), binPath, TRUE);
    if (FAILED(hr))
    {
        PrintMessage("D3DWriteBlobToFile returned error (0x%x)\n", hr);
        return -1;
    }

    PrintMessage("Shader blob size %d is written here: %ls\n",
        pPostStripCode->GetBufferSize(),
        binPath);

    return 0;
}
