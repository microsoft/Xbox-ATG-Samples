//--------------------------------------------------------------------------------------
// DumpTool.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include <DbgHelp.h>

#include "CommandLineHelpers.h"
#include "FileHelpers.h"
#include "OSHelpers.h"

using namespace ATG;

#define HEX_FMT L"0x%08x"

namespace
{
    enum options : uint32_t
    {
        OPT_MINI_DUMP_TYPE = 1,
        OPT_PREDEFINED_TYPE,
        OPT_MAX
    };

    static_assert(OPT_MAX <= sizeof(options) * 8, "Options must fit into a sizeof(options) * 8 bitfield");

    const SValue g_Options[] =
    {
        { L"mdt", OPT_MINI_DUMP_TYPE },
        { L"pdt", OPT_PREDEFINED_TYPE }
    };

#define DEF_MDT(mdt) { L#mdt, MiniDump ## mdt }
    const SValue g_MDTs[] =
    {
        DEF_MDT(Normal),
        DEF_MDT(WithDataSegs),
        DEF_MDT(WithFullMemory),
        DEF_MDT(WithHandleData),
        DEF_MDT(FilterMemory),
        DEF_MDT(ScanMemory),
        DEF_MDT(WithUnloadedModules),
        DEF_MDT(WithIndirectlyReferencedMemory),
        DEF_MDT(FilterModulePaths),
        DEF_MDT(WithProcessThreadData),
        DEF_MDT(WithPrivateReadWriteMemory),
        DEF_MDT(WithoutOptionalData),
        DEF_MDT(WithFullMemoryInfo),
        DEF_MDT(WithThreadInfo),
        DEF_MDT(WithCodeSegs),
        DEF_MDT(WithoutAuxiliaryState),
        DEF_MDT(WithFullAuxiliaryState),
        DEF_MDT(WithPrivateWriteCopyMemory),
        DEF_MDT(IgnoreInaccessibleMemory),
        DEF_MDT(WithTokenInformation),
        DEF_MDT(WithModuleHeaders),
        DEF_MDT(FilterTriage)
    };
#undef DEF_MDT

    const SValue g_PDTS[] =
    {
        { L"heap",
        MiniDumpWithDataSegs
        | MiniDumpWithProcessThreadData
        | MiniDumpWithHandleData
        | MiniDumpWithPrivateReadWriteMemory
        | MiniDumpWithUnloadedModules
        | MiniDumpWithPrivateWriteCopyMemory
        | MiniDumpWithFullMemoryInfo
        | MiniDumpWithThreadInfo
        | MiniDumpWithTokenInformation
        | MiniDumpIgnoreInaccessibleMemory },

        { L"mini",
        MiniDumpWithDataSegs
        | MiniDumpWithUnloadedModules
        | MiniDumpWithProcessThreadData
        | MiniDumpWithTokenInformation
        | MiniDumpIgnoreInaccessibleMemory },

        { L"micro",
        MiniDumpFilterMemory
        | MiniDumpFilterModulePaths
        | MiniDumpIgnoreInaccessibleMemory },

        { L"triage",
        MiniDumpWithHandleData
        | MiniDumpWithUnloadedModules
        | MiniDumpFilterModulePaths
        | MiniDumpWithProcessThreadData
        | MiniDumpFilterTriage
        | MiniDumpIgnoreInaccessibleMemory
        },

        { L"native",
        MiniDumpWithFullMemory
        | MiniDumpWithHandleData
        | MiniDumpWithUnloadedModules
        | MiniDumpWithFullMemoryInfo
        | MiniDumpWithThreadInfo
        | MiniDumpWithTokenInformation
        | MiniDumpIgnoreInaccessibleMemory }
    };

    void PrintLogo()
    {
        wprintf(L"Microsoft (R) Minidump tool for for Xbox One\n");
        wprintf(L"Copyright (C) Microsoft Corp. All rights reserved.\n");
#ifdef _DEBUG
        wprintf(L"*** Debug build ***\n");
#endif
        wprintf(L"\n");
    }

    void PrintUsage()
    {
        PrintLogo();

        wprintf(L"Usage: DumpTool [-mdt:<minidump type> ...] [-pdt:<predefined type>] <executable name> \n\n");

        const wchar_t mdts[] = L"\n     <minidump type>: ";
        wprintf(mdts);
        PrintTable(_countof(mdts) - 2, g_MDTs);

        const wchar_t pdts[] = L"\n   <predefined type>: ";
        wprintf(pdts);
        PrintTable(_countof(pdts) - 2, g_PDTS);
    }

#if defined(_XBOX_ONE) && defined(_TITLE)
#else
    BOOL EnableDebugPrivilege(BOOL bEnable)
    {
        HANDLE hToken = nullptr;
        LUID luid;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) return FALSE;
        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) return FALSE;

        TOKEN_PRIVILEGES tokenPriv;
        tokenPriv.PrivilegeCount = 1;
        tokenPriv.Privileges[0].Luid = luid;
        tokenPriv.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;

        if (!AdjustTokenPrivileges(hToken, FALSE, &tokenPriv, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) return FALSE;

        return TRUE;
    }
#endif

    inline HANDLE safe_proc_handle(HANDLE h) { return (h == INVALID_HANDLE_VALUE) ? 0 : h; }

    class CProcHandle
    {
        HANDLE m_handle;
    public:

        CProcHandle()
            : CProcHandle(0)
        {
        }

        CProcHandle(HANDLE h)
            : m_handle(safe_proc_handle(h))
        {
        }

        CProcHandle(const CProcHandle &) = delete;
        CProcHandle &operator=(const CProcHandle &) = delete;

        CProcHandle(CProcHandle &&other)
            : m_handle(other.detach())
        {
        }


        CProcHandle &operator=(CProcHandle &&other)
        {
            close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
            return *this;
        }

        ~CProcHandle()
        {
            close();
        }


        operator bool() const
        {
            return m_handle != nullptr;
        }

        HANDLE get() const
        {
            return m_handle;
        }

        HANDLE detach()
        {
            HANDLE result = m_handle;
            m_handle = nullptr;
            return result;
        }

        void close()
        {
            assert(m_handle != INVALID_HANDLE_VALUE);
            if (m_handle)
            {
                CloseHandle(m_handle);
            }
        }
    };

    bool FileExists(const wchar_t *fileName)
    {
        uint32_t attrs = ::GetFileAttributesW(fileName);

        return (attrs != INVALID_FILE_ATTRIBUTES
            && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
    }

    wchar_t *GetDumpFileName(const wchar_t *exeName)
    {
        static wchar_t outFile[MAX_PATH] = L"d:\\";
        {
            wcscat_s(outFile, exeName);

            wchar_t *dot = wcsrchr(outFile, '.');
            if (dot)
            {
                *dot = L'\0';
            }
            wcscat_s(outFile, L".dmp");

            int num = 1;
            wchar_t suffix[16] = L"";

            wchar_t *underscore = dot;
            while (FileExists(outFile))
            {
                *underscore = L'\0';
                swprintf_s(suffix, L"_%i.dmp", ++num);
                wcscat_s(outFile, suffix);
            }
        }

        return outFile;
    }

    void WriteDump(HANDLE proc, DWORD procId, MINIDUMP_TYPE mdt, const wchar_t *dumpFileName)
    {
        if (!dumpFileName)
        {
            DX::ThrowIfFailed(E_INVALIDARG);
        }

        wprintf(L"Writing dump file: %s (type: 0x%08x)\n", dumpFileName, mdt);

        // Create the dump file
        ScopedHandle dumpFile(safe_handle(
            CreateFile(
                dumpFileName,
                 GENERIC_READ |  GENERIC_WRITE,
                0,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL)));

        if (!dumpFile)
        {
            DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }


        if (!MiniDumpWriteDump(proc, procId, dumpFile.get(), mdt, nullptr, nullptr, nullptr))
        {
            DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        if (!FlushFileBuffers(dumpFile.get()))
        {
            DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    void WriteDumpForFileName(const wchar_t *filename, MINIDUMP_TYPE mdt)
    {
        int processCount = 0;
        DWORD procIds[256];
        {
            DWORD resultSize = 0;
            if (EnumProcesses(procIds, sizeof(procIds), &resultSize))
            {
                processCount = resultSize / sizeof(DWORD);
                wchar_t buf[64] = L"";
                swprintf_s(buf, L"Found %i processes\n", processCount);
                OutputDebugString(buf);
            }
            else
            {
                auto err = GetLastError();
                wchar_t buf[64] = L"";
                swprintf_s(buf, L"EnumProcesses failed with error code:" HEX_FMT L"\n", err);
                OutputDebugString(buf);
                DX::ThrowIfFailed(HRESULT_FROM_WIN32(err));
            }
        }

        CProcHandle proc;

        for (int i = 0; i < processCount; ++i)
        {
            proc = OpenProcess(PROCESS_ALL_ACCESS, true, procIds[i]);
            if (proc)
            {
                wchar_t procName[MAX_PATH];

                size_t nameLength = GetProcessImageFileName(proc.get(), procName, MAX_PATH);
                auto err = (nameLength == 0) ? GetLastError() : 0;

                if (err)
                {
                    wchar_t buf[64] = L"";
                    swprintf_s(buf, L"GetProcessImageFileName failed with error code:" HEX_FMT L"\n", err);
                    OutputDebugString(buf);
                }
                else
                {
                    procName[MAX_PATH - 1] = 0;
                    wchar_t *fn = wcsrchr(procName, L'\\');
                    fn = fn ? fn + 1 : procName;

                    if (_wcsicmp(fn, filename) == 0)
                    {
                        WriteDump(proc.get(), procIds[i], mdt, GetDumpFileName(fn));
                        return;
                    }
                }
            }
            else
            {
                auto err = GetLastError();
                wchar_t buf[64] = L"";
                swprintf_s(buf, L"OpenProcess failed with error code:" HEX_FMT L"\n", err);
                OutputDebugString(buf);
                continue;
            }
        }

        wprintf(L"Could not find executable: %s\n", filename);
    }


} // ANONYMOUS namespace

int wmain(int argc, wchar_t **argv)
{
    // These will be filled in during argument parsing
    unsigned       mdt = MiniDumpNormal;
    unsigned       oneMdt = MiniDumpNormal;
    unsigned       pdt = MiniDumpNormal;
    const wchar_t *exeName = nullptr;

    unsigned individualOptions = 0;

    // Starting at 1 to skip the name of *this* executable
    for (int argIdx = 1; argIdx < argc; ++argIdx)
    {
        wchar_t *arg = argv[argIdx];

        if (('-' == arg[0]) || ('/' == arg[0]))
        {
            arg++;

            // This voodoo will separate the option from the corresponding value:
            wchar_t *value = nullptr;
            for (value = arg; *value && (':' != *value); ++value);
            if (*value) *value++ = 0;

            // Lookup the option part
            unsigned option = 0;
            if (!LookupByName(arg, g_Options, option) || (individualOptions & (1 << option)))
            {
                if (individualOptions & (1 << option))
                {
                    wprintf(L"The option, -%s, can only be specified once\n\n", arg);
                }
                PrintUsage();
                return 1;
            }

            // Keep track of uses of individual options
            if (option != OPT_MINI_DUMP_TYPE)
            {
                individualOptions |= (1 << option);
            }

            // Handle options that consume an additional argument
            switch (option)
            {
            case OPT_MINI_DUMP_TYPE:
            case OPT_PREDEFINED_TYPE:
                if (!*value)
                {
                    if (argIdx + 1 >= argc)
                    {
                        PrintUsage();
                        return 1;
                    }

                    ++argIdx;
                    value = argv[argIdx];
                }
                break;
            }

            switch (option)
            {
            case OPT_MINI_DUMP_TYPE:
                if (ParseTableValue(option, value, g_MDTs, oneMdt))
                {
                    mdt |= oneMdt;
                }
                else
                {
                    PrintUsage();
                    return 1;
                }
                break;

            case OPT_PREDEFINED_TYPE:
                if (ParseTableValue(option, value, g_PDTS, pdt))
                {
                    mdt |= pdt;
                }
                else
                {
                    PrintUsage();
                    return 1;
                }
            }
        }
        else
        {
            if (exeName == nullptr)
            {
                exeName = arg;
            }
            else
            {
                wprintf(L"<executable name> already has a value (%s). Found another one: %s\n\n", exeName, arg);
                PrintUsage();
                return 1;
            }
        }
    }

    if (exeName == nullptr)
    {
        wprintf(L"<executable name> was not specified.\n\n");
        PrintUsage();
        return 1;
    }

    try
    {
#if defined(_XBOX_ONE) && defined(_TITLE)
#else
        EnableDebugPrivilege(true);
#endif
        WriteDumpForFileName(exeName, (MINIDUMP_TYPE)mdt);
    }
    catch (std::exception exn)
    {
        wprintf(L"Failed to write dump:\n%S\n", exn.what());
    }
    catch (...)
    {
        wprintf(L"Unexpected failure while attempting to write the dump\n");
    }

    return 0;
}
