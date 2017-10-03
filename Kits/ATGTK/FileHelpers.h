//--------------------------------------------------------------------------------------
// FileHelpers.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <fileapi.h>
#include <assert.h>
#include <stdio.h>

#include <wrl/client.h>

namespace ATG
{
    class auto_delete_file
    {
    public:
        auto_delete_file(HANDLE hFile) : m_handle(hFile) {}

        auto_delete_file(const auto_delete_file&) = delete;
        auto_delete_file& operator=(const auto_delete_file&) = delete;

        ~auto_delete_file()
        {
            if (m_handle)
            {
                FILE_DISPOSITION_INFO info = {};
                info.DeleteFile = TRUE;
                BOOL b = true;
                b = SetFileInformationByHandle(m_handle, FileDispositionInfo, &info, static_cast<DWORD>(sizeof(info)));
#ifdef _DEBUG
                if (!b)
                {
                    DWORD error = GetLastError();
                    wchar_t buff[128] = {};
                    swprintf_s(buff, L"ERROR: SetFileInformationByHandle failed (0x%08X)\n", error);
                    OutputDebugStringW(buff);
                }
#endif
                assert(b);
            }
        }

        void clear() { m_handle = 0; }

    private:
        HANDLE m_handle;
    };

#ifdef __IWICStream_FWD_DEFINED__
    class auto_delete_file_wic
    {
    public:
        auto_delete_file_wic(Microsoft::WRL::ComPtr<IWICStream>& hFile, LPCWSTR szFile) : m_handle(hFile), m_filename(szFile) {}

        auto_delete_file_wic(const auto_delete_file_wic&) = delete;
        auto_delete_file_wic& operator=(const auto_delete_file_wic&) = delete;

        ~auto_delete_file_wic()
        {
            if (m_filename)
            {
                m_handle.Reset();
                DeleteFileW(m_filename);
            }
        }

        void clear() { m_filename = 0; }

    private:
        LPCWSTR m_filename;
        Microsoft::WRL::ComPtr<IWICStream>& m_handle;
    };
#endif
}