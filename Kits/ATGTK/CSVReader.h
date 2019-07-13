//--------------------------------------------------------------------------------------
// File: CSVReader.h
//
// Simple parser for .csv (Comma-Separated Values) files.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//-------------------------------------------------------------------------------------

#pragma once

#include <exception>
#include <memory>
#include <vector>


namespace DX
{
    class CSVReader
    {
    public:
        enum class Encoding
        {
            UTF16,  // File is Unicode UTF-16
            UTF8,   // File is Unicode UTF-8
        };

        explicit CSVReader(_In_z_ const wchar_t* fileName, Encoding encoding = Encoding::UTF8, bool ignoreComments = false) :
            m_end(nullptr),
            m_currentChar(nullptr),
            m_currentLine(0),
            m_ignoreComments(ignoreComments)
        {
            assert(fileName != 0);

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
            ScopedHandle hFile(safe_handle(CreateFile2(fileName,
                GENERIC_READ,
                FILE_SHARE_READ,
                OPEN_EXISTING,
                nullptr)));
#else
            ScopedHandle hFile(safe_handle(CreateFileW(fileName,
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                nullptr)));
#endif
            if (!hFile)
            {
                throw std::exception("CreateFile");
            }

            FILE_STANDARD_INFO fileInfo;
            if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
            {
                throw std::exception("GetFileInformationByHandleEx");
            }

            if (fileInfo.EndOfFile.HighPart > 0 || fileInfo.EndOfFile.LowPart > 0x7ffffffd)
            {
                // File is 2 GBs or larger
                throw std::exception("CSV too large");
            }

            std::unique_ptr<uint8_t[]> data(new uint8_t[fileInfo.EndOfFile.LowPart + 2]);

            DWORD out;
            if (!ReadFile(hFile.get(), data.get(), fileInfo.EndOfFile.LowPart, &out, nullptr)
                || out != fileInfo.EndOfFile.LowPart)
            {
                throw std::exception("ReadFile");
            }

            // Ensure buffer is nul terminated
            data[out] = data[out + 1] = '\0';

            // Ignore lines in .csv that start with '#' character

            // Handle text encoding
            if (encoding == Encoding::UTF16)
            {
                m_data.reset(reinterpret_cast<wchar_t*>(data.release()));
                m_end = reinterpret_cast<wchar_t*>(&data[out]);
            }
            else
            {
                // If we are not UTF16, we have to convert...
                int cch = ::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<LPCSTR>(data.get()), -1, nullptr, 0);

                if (cch <= 0)
                {
                    throw std::exception("MultiByteToWideChar");
                }

                m_data.reset(new wchar_t[size_t(cch)]);

                int result = ::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<LPCSTR>(data.get()), -1, m_data.get(), cch);

                if (result <= 0)
                {
                    m_data.reset();
                    throw std::exception("MultiByteToWideChar");
                }

                m_end = &m_data[size_t(result - 1)];
            }

            // Locate the start of lines
            bool newline = true;
            for (const wchar_t* ptr = m_data.get(); *ptr != 0 && ptr < m_end; )
            {
                if (*ptr == '\n' || *ptr == '\r')
                {
                    ++ptr;
                    newline = true;
                }
                else if (*ptr == '#' && m_ignoreComments && newline)
                {
                    // Skip to CR
                    for (; *ptr != 0 && *ptr != '\n' && ptr < m_end; ++ptr) {}
                }
                else if (*ptr == '"')
                {
                    if (newline)
                    {
                        m_lines.push_back(ptr);
                        newline = false;
                    }

                    // Skip to next " (skipping "" escapes)
                    for (ptr++; *ptr != 0 && ptr < m_end; ++ptr)
                    {
                        if (*ptr == '"')
                        {
                            ++ptr;
                            if (*ptr != '"')
                                break;
                        }
                    }
                }
                else if (newline)
                {
                    m_lines.push_back(ptr);
                    newline = false;
                    ++ptr;
                }
                else
                    ++ptr;
            }

            TopOfFile();
        }

        CSVReader(const CSVReader&) = delete;
        CSVReader& operator=(const CSVReader&) = delete;

        // Return number of lines of data in CSV
        size_t GetRecordCount() const { return m_lines.size(); }

        // Check for end of file
        bool EndOfFile() const { return m_currentChar == nullptr; }

        // Return current record number (0-based)
        size_t RecordIndex() const { return m_currentLine; }

        // Set to top of file
        void TopOfFile()
        {
            m_currentChar = (m_lines.empty()) ? nullptr : m_lines[0];
            m_currentLine = 0;
        }

        // Start processing next record (returns false when out of data)
        bool NextRecord()
        {
            if (!m_currentChar)
                return false;

            if (++m_currentLine >= m_lines.size())
            {
                m_currentChar = nullptr;
                return false;
            }

            m_currentChar = m_lines[m_currentLine];

            return true;
        }

        // Get next item in record (returns false when reached end of record)
        bool NextItem(_Out_writes_(maxstr) wchar_t* str, _In_ size_t maxstr)
        {
            if (!str || !m_currentChar || !maxstr)
                return false;

            *str = L'\0';

            const wchar_t* end = ((m_currentLine + 1) >= m_lines.size()) ? m_end : (m_lines[(m_currentLine + 1)]);

            if (m_currentChar >= end)
                return false;

            wchar_t* dest = str;
            wchar_t* edest = str + maxstr;

            for (const wchar_t* ptr = m_currentChar; ; )
            {
                if (*ptr == 0 || ptr >= end || *ptr == '\n' || *ptr == '\r')
                {
                    m_currentChar = end;
                    break;
                }
                else if (*ptr == ',')
                {
                    m_currentChar = ptr + 1;
                    break;
                }
                else if (*ptr == '\t' || *ptr == ' ')
                {
                    // Whitespace
                    ++ptr;
                }
                else if (*ptr == '"')
                {
                    // Copy from " to ", respecting "" as double-quotes
                    for (ptr++; *ptr != 0 && ptr < end; ++ptr)
                    {
                        if (*ptr == '"')
                        {
                            ++ptr;
                            if (*ptr != '"')
                                break;
                            else if (dest < edest)
                                *(dest++) = '"';
                        }
                        else if (dest < edest)
                            *(dest++) = *ptr;
                    }
                }
                else
                {
                    for (; *ptr != 0 && ptr < end; ++ptr)
                    {
                        if (*ptr == '\n' || *ptr == '\r' || *ptr == ',')
                            break;
                        else if (dest < edest)
                            *(dest++) = *ptr;
                    }
                }
            }

            if (dest < edest)
                *dest = 0;

            return true;
        }

        template<size_t TNameLength>
        bool NextItem(wchar_t(&name)[TNameLength])
        {
            return NextItem(name, TNameLength);
        }

    private:
        struct handle_closer { void operator()(HANDLE h) { if (h) CloseHandle(h); } };

        typedef std::unique_ptr<void, handle_closer> ScopedHandle;

        inline HANDLE safe_handle(HANDLE h) { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

        std::unique_ptr<wchar_t[]>  m_data;
        const wchar_t*              m_end;
        const wchar_t*              m_currentChar;
        size_t                      m_currentLine;
        bool                        m_ignoreComments;
        std::vector<const wchar_t*> m_lines;
    };
}
