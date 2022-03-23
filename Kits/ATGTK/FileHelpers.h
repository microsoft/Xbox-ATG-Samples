//--------------------------------------------------------------------------------------
// FileHelpers.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------
#pragma once

#include <fileapi.h>

#include <cassert>
#include <cstdio>
#include <tuple>

#include <wrl/client.h>

namespace DX
{
    class auto_delete_file
    {
    public:
        auto_delete_file(HANDLE hFile) noexcept : m_handle(hFile) {}

        ~auto_delete_file()
        {
            if (m_handle)
            {
                FILE_DISPOSITION_INFO info = {};
                info.DeleteFile = TRUE;
                std::ignore = SetFileInformationByHandle(m_handle, FileDispositionInfo, &info, sizeof(info));
            }
        }

        auto_delete_file(const auto_delete_file&) = delete;
        auto_delete_file& operator=(const auto_delete_file&) = delete;

        auto_delete_file(const auto_delete_file&&) = delete;
        auto_delete_file& operator=(const auto_delete_file&&) = delete;

        void clear() noexcept { m_handle = nullptr; }

    private:
        HANDLE m_handle;
    };

#ifdef __IWICStream_FWD_DEFINED__
    class auto_delete_file_wic
    {
    public:
        auto_delete_file_wic(Microsoft::WRL::ComPtr<IWICStream>& hFile, LPCWSTR szFile) noexcept :  m_filename(szFile), m_handle(hFile) {}

        ~auto_delete_file_wic()
        {
            if (m_filename)
            {
                m_handle.Reset();
                DeleteFileW(m_filename);
            }
        }

        auto_delete_file_wic(const auto_delete_file_wic&) = delete;
        auto_delete_file_wic& operator=(const auto_delete_file_wic&) = delete;

        auto_delete_file_wic(const auto_delete_file_wic&&) = delete;
        auto_delete_file_wic& operator=(const auto_delete_file_wic&&) = delete;

        void clear() noexcept { m_filename = nullptr; }

    private:
        LPCWSTR m_filename;
        Microsoft::WRL::ComPtr<IWICStream>& m_handle;
    };
#endif
}
