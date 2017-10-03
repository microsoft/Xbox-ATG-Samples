//--------------------------------------------------------------------------------------
// File: DirectXTexEXR.cpp
//
// DirectXTex Auxillary functions for using the OpenEXR library
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//Uncomment if you add DirectXTexEXR to your copy of the DirectXTex library
//#include "DirectXTexp.h"

#include "DirectXTexEXR.h"

#include <DirectXPackedVector.h>

#include <assert.h>
#include <exception>
#include <memory>

//
// Requires the OpenEXR library <http://www.openexr.com/> and ZLIB <http://www.zlib.net>
//

#pragma warning(push)
#pragma warning(disable : 4244 4996)
#include <ImfRgbaFile.h>
#include <ImfIO.h>
#pragma warning(pop)

static_assert(sizeof(Imf::Rgba) == 8, "Mismatch size");

using namespace DirectX;
using PackedVector::XMHALF4;

// Comment out this first anonymous namespace if you add the include of DirectXTexP.h above
namespace
{
    struct handle_closer { void operator()(HANDLE h) { assert(h != INVALID_HANDLE_VALUE); if (h) CloseHandle(h); } };

    typedef public std::unique_ptr<void, handle_closer> ScopedHandle;

    inline HANDLE safe_handle(HANDLE h) { return (h == INVALID_HANDLE_VALUE) ? 0 : h; }

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
                FILE_DISPOSITION_INFO info = { 0 };
                info.DeleteFile = TRUE;
                (void)SetFileInformationByHandle(m_handle, FileDispositionInfo, &info, sizeof(info));
            }
        }

        void clear() { m_handle = 0; }

    private:
        HANDLE m_handle;
    };
}

namespace
{
    class com_exception : public std::exception
    {
    public:
        com_exception(HRESULT hr) : result(hr) {}

        virtual const char* what() const override
        {
            static char s_str[64] = {};
            sprintf_s(s_str, "Failure with HRESULT of %08X", result);
            return s_str;
        }

        HRESULT hr() const { return result; }

    private:
        HRESULT result;
    };

    class InputStream : public Imf::IStream
    {
    public:
        InputStream(HANDLE hFile, const char fileName[]) :
            IStream(fileName), m_hFile(hFile)
        {
            LARGE_INTEGER dist = { 0 };
            LARGE_INTEGER result;
            if (!SetFilePointerEx(m_hFile, dist, &result, FILE_END))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }

            m_EOF = result.QuadPart;

            if (!SetFilePointerEx(m_hFile, dist, nullptr, FILE_BEGIN))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        InputStream(const InputStream &) = delete;
        InputStream& operator = (const InputStream &) = delete;

        virtual bool read(char c[], int n) override
        {
            DWORD bytesRead;
            if (!ReadFile(m_hFile, c, static_cast<DWORD>(n), &bytesRead, nullptr))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }

            LARGE_INTEGER dist = { 0 };
            LARGE_INTEGER result;
            if (!SetFilePointerEx(m_hFile, dist, &result, FILE_CURRENT))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }

            return result.QuadPart >= m_EOF;
        }

        virtual Imf::Int64 tellg() override
        {
            LARGE_INTEGER dist = { 0 };
            LARGE_INTEGER result;
            if (!SetFilePointerEx(m_hFile, dist, &result, FILE_CURRENT))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
            return result.QuadPart;
        }

        virtual void seekg(Imf::Int64 pos) override
        {
            LARGE_INTEGER dist;
            dist.QuadPart = pos;
            if (!SetFilePointerEx(m_hFile, dist, nullptr, FILE_BEGIN))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        virtual void clear() override
        {
            SetLastError(0);
        }

    private:
        HANDLE m_hFile;
        LONGLONG m_EOF;
    };

    class OutputStream : public Imf::OStream
    {
    public:
        OutputStream(HANDLE hFile, const char fileName[]) :
            OStream(fileName), m_hFile(hFile) {}

        OutputStream(const OutputStream &) = delete;
        OutputStream& operator = (const OutputStream &) = delete;

        virtual void write(const char c[], int n) override
        {
            DWORD bytesWritten;
            if (!WriteFile(m_hFile, c, static_cast<DWORD>(n), &bytesWritten, nullptr))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        virtual Imf::Int64 tellp() override
        {
            LARGE_INTEGER dist = { 0 };
            LARGE_INTEGER result;
            if (!SetFilePointerEx(m_hFile, dist, &result, FILE_CURRENT))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
            return result.QuadPart;
        }

        virtual void seekp(Imf::Int64 pos) override
        {
            LARGE_INTEGER dist;
            dist.QuadPart = pos;
            if (!SetFilePointerEx(m_hFile, dist, nullptr, FILE_BEGIN))
            {
                throw com_exception(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

    private:
        HANDLE m_hFile;
    };
}


//=====================================================================================
// Entry-points
//=====================================================================================

//-------------------------------------------------------------------------------------
// Obtain metadata from EXR file on disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::GetMetadataFromEXRFile(const wchar_t* szFile, TexMetadata& metadata)
{
    if (!szFile)
        return E_INVALIDARG;

    char fileName[MAX_PATH];
    int result = WideCharToMultiByte(CP_ACP, 0, szFile, -1, fileName, MAX_PATH, nullptr, nullptr);
    if (result <= 0)
    {
        *fileName = 0;
    }

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, nullptr)));
#endif
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    InputStream stream(hFile.get(), fileName);

    HRESULT hr = S_OK;

    try
    {
        Imf::RgbaInputFile file(stream);

        auto dw = file.dataWindow();

        int width = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;

        if (width < 1 || height < 1)
            return E_FAIL;

        metadata.width = static_cast<size_t>(width);
        metadata.height = static_cast<size_t>(height);
        metadata.depth = metadata.arraySize = metadata.mipLevels = 1;
        metadata.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        metadata.dimension = TEX_DIMENSION_TEXTURE2D;
    }
    catch (const com_exception& exc)
    {
#ifdef _DEBUG
        OutputDebugStringA(exc.what());
#endif
        hr = exc.hr();
    }
    catch (const std::exception& exc)
    {
        exc;
#ifdef _DEBUG
        OutputDebugStringA(exc.what());
#endif
        hr = E_FAIL;
    }
    catch (...)
    {
        hr = E_UNEXPECTED;
    }

    return hr;
}


//-------------------------------------------------------------------------------------
// Load a EXR file from disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::LoadFromEXRFile(const wchar_t* szFile, TexMetadata* metadata, ScratchImage& image)
{
    if (!szFile)
        return E_INVALIDARG;

    image.Release();

    if (metadata)
    {
        memset(metadata, 0, sizeof(TexMetadata));
    }

    char fileName[MAX_PATH];
    int result = WideCharToMultiByte(CP_ACP, 0, szFile, -1, fileName, MAX_PATH, nullptr, nullptr);
    if (result <= 0)
    {
        *fileName = 0;
    }

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, nullptr)));
#endif
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    InputStream stream(hFile.get(), fileName);

    HRESULT hr = S_OK;

    try
    {
        Imf::RgbaInputFile file(stream);

        auto dw = file.dataWindow();

        int width = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;

        if (width < 1 || height < 1)
            return E_FAIL;

        if (metadata)
        {
            metadata->width = static_cast<size_t>(width);
            metadata->height = static_cast<size_t>(height);
            metadata->depth = metadata->arraySize = metadata->mipLevels = 1;
            metadata->format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            metadata->dimension = TEX_DIMENSION_TEXTURE2D;
        }

        hr = image.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, 1, 1);
        if (FAILED(hr))
            return hr;

        file.setFrameBuffer(reinterpret_cast<Imf::Rgba*>(image.GetPixels()) - dw.min.x - dw.min.y * width, 1, width);
        file.readPixels(dw.min.y, dw.max.y);
    }
    catch (const com_exception& exc)
    {
#ifdef _DEBUG
        OutputDebugStringA(exc.what());
#endif
        hr = exc.hr();
    }
    catch (const std::exception& exc)
    {
        exc;
#ifdef _DEBUG
        OutputDebugStringA(exc.what());
#endif
        hr = E_FAIL;
    }
    catch (...)
    {
        hr = E_UNEXPECTED;
    }

    if (FAILED(hr))
    {
        image.Release();
    }

    return hr;
}


//-------------------------------------------------------------------------------------
// Save a EXR file to disk
//-------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::SaveToEXRFile(const Image& image, const wchar_t* szFile)
{
    if (!szFile)
        return E_INVALIDARG;

    if (!image.pixels)
        return E_POINTER;

    if (image.width > INT32_MAX || image.height > INT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    switch (image.format)
    {
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        if ((image.rowPitch % 8) > 0)
            return E_FAIL;
        break;

    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32_FLOAT:
        break;

    default:
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    char fileName[MAX_PATH];
    int result = WideCharToMultiByte(CP_ACP, 0, szFile, -1, fileName, MAX_PATH, nullptr, nullptr);
    if (result <= 0)
    {
        *fileName = 0;
    }
    // Create file and write header
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
    ScopedHandle hFile(safe_handle(CreateFile2(szFile, GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr)));
#else
    ScopedHandle hFile(safe_handle(CreateFileW(szFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr)));
#endif
    if (!hFile)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    auto_delete_file delonfail(hFile.get());

    OutputStream stream(hFile.get(), fileName);

    HRESULT hr = S_OK;

    try
    {
        int width = static_cast<int>(image.width);
        int height = static_cast<int>(image.height);

        Imf::RgbaOutputFile file(stream, Imf::Header(width, height), Imf::WRITE_RGBA);

        if (image.format == DXGI_FORMAT_R16G16B16A16_FLOAT)
        {
            file.setFrameBuffer(reinterpret_cast<const Imf::Rgba*>(image.pixels), 1, image.rowPitch / 8);
            file.writePixels(height);
        }
        else
        {
            std::unique_ptr<XMHALF4> temp(new (std::nothrow) XMHALF4[width * height]);
            if (!temp)
                return E_OUTOFMEMORY;

            file.setFrameBuffer(reinterpret_cast<const Imf::Rgba*>(temp.get()), 1, width);

            auto sPtr = image.pixels;
            auto dPtr = temp.get();
            if (image.format == DXGI_FORMAT_R32G32B32A32_FLOAT)
            {
                for (int j = 0; j < height; ++j)
                {
                    auto srcPtr = reinterpret_cast<const XMFLOAT4*>(sPtr);
                    auto destPtr = dPtr;
                    for (int k = 0; k < width; ++k, ++srcPtr, ++destPtr)
                    {
                        XMVECTOR v = XMLoadFloat4(srcPtr);
                        PackedVector::XMStoreHalf4(destPtr, v);
                    }

                    sPtr += image.rowPitch;
                    dPtr += width;

                    file.writePixels(1);
                }
            }
            else
            {
                assert(image.format == DXGI_FORMAT_R32G32B32_FLOAT);

                for (int j = 0; j < height; ++j)
                {
                    auto srcPtr = reinterpret_cast<const XMFLOAT3*>(sPtr);
                    auto destPtr = dPtr;
                    for (int k = 0; k < width; ++k, ++srcPtr, ++destPtr)
                    {
                        XMVECTOR v = XMLoadFloat3(srcPtr);
                        v = XMVectorSelect(g_XMIdentityR3, v, g_XMSelect1110);
                        PackedVector::XMStoreHalf4(destPtr, v);
                    }

                    sPtr += image.rowPitch;
                    dPtr += width;

                    file.writePixels(1);
                }
            }
        }
    }
    catch (const com_exception& exc)
    {
#ifdef _DEBUG
        OutputDebugStringA(exc.what());
#endif
        hr = exc.hr();
    }
    catch (const std::exception& exc)
    {
        exc;
#ifdef _DEBUG
        OutputDebugStringA(exc.what());
#endif
        hr = E_FAIL;
    }
    catch (...)
    {
        hr = E_UNEXPECTED;
    }

    if (FAILED(hr))
        return hr;

    delonfail.clear();

    return S_OK;
}
