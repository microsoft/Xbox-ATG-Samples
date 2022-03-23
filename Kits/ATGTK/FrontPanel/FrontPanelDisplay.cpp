//--------------------------------------------------------------------------------------
// FrontPanelDisplay.cpp
//
// Microsoft Xbox One XDK
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "pch.h"

#include "FrontPanelDisplay.h"

#include <wincodec.h>

#include "OSHelpers.h"
#include "FileHelpers.h"

#include <stdexcept>

using namespace ATG;
using namespace DirectX;
using namespace DX;

using Microsoft::WRL::ComPtr;

namespace
{
    // For more information, see DirectX Tool Kit's dds.h
    const uint32_t DDS_MAGIC = 0x20534444; // "DDS "

#define DDS_LUMINANCE                   0x00020000  // DDPF_LUMINANCE
#define DDS_HEADER_FLAGS_TEXTURE        0x00001007  // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT 
#define DDS_HEADER_FLAGS_MIPMAP         0x00020000  // DDSD_MIPMAPCOUNT
#define DDS_HEADER_FLAGS_PITCH          0x00000008  // DDSD_PITCH
#define DDS_SURFACE_FLAGS_TEXTURE       0x00001000  // DDSCAPS_TEXTURE

#pragma pack(push, 1)
    struct DDS_PIXELFORMAT
    {
        uint32_t    size;
        uint32_t    flags;
        uint32_t    fourCC;
        uint32_t    RGBBitCount;
        uint32_t    RBitMask;
        uint32_t    GBitMask;
        uint32_t    BBitMask;
        uint32_t    ABitMask;
    };

    struct DDS_HEADER
    {
        uint32_t        size;
        uint32_t        flags;
        uint32_t        height;
        uint32_t        width;
        uint32_t        pitchOrLinearSize;
        uint32_t        depth; // only if DDS_HEADER_FLAGS_VOLUME is set in flags
        uint32_t        mipMapCount;
        uint32_t        reserved1[11];
        DDS_PIXELFORMAT ddspf;
        uint32_t        caps;
        uint32_t        caps2;
        uint32_t        caps3;
        uint32_t        caps4;
        uint32_t        reserved2;
    };
#pragma pack(pop)

    const DDS_PIXELFORMAT DDSPF_L8 = { sizeof(DDS_PIXELFORMAT), DDS_LUMINANCE, 0,  8, 0xff, 0x00, 0x00, 0x00 };
}

namespace DirectX
{
    namespace Internal
    {
    // Reuse the WIC factory function from the DirectX Tool Kit. For implementation details, see WICTextureLoader.cpp
#if defined(__d3d12_x_h__) || defined(__XBOX_D3D12_X__)
        extern IWICImagingFactory2* GetWIC();
#else
        extern IWICImagingFactory* GetWIC();
#endif
    }
}

using namespace DirectX::Internal;

// --------------------------------------------------------------------------------
// FrontPanelDisplay methods
// --------------------------------------------------------------------------------
FrontPanelDisplay* FrontPanelDisplay::s_frontPanelDisplayInstance = nullptr;

FrontPanelDisplay::FrontPanelDisplay(_In_ IXboxFrontPanelControl * frontPanelControl)
    : m_frontPanelControl(frontPanelControl)
    , m_displayWidth(0)
    , m_displayHeight(0)
{
    if (s_frontPanelDisplayInstance)
    {
        throw std::runtime_error("FrontPanelDisplay is a singleton");
    }

    s_frontPanelDisplayInstance = this;

    if (IsAvailable())
    {
        uint32_t displayWidth = 0;
        ThrowIfFailed(
            m_frontPanelControl->GetScreenWidth(&displayWidth)
        );
        m_displayWidth = displayWidth;

        uint32_t displayHeight = 0;
        ThrowIfFailed(
            m_frontPanelControl->GetScreenHeight(&displayHeight)
        );
        m_displayHeight = displayHeight;

        m_buffer = std::make_unique<uint8_t[]>(displayWidth * displayHeight);
    }
}

FrontPanelDisplay::~FrontPanelDisplay()
{
    s_frontPanelDisplayInstance = nullptr;
}

void FrontPanelDisplay::Clear()
{
    if (IsAvailable())
    {
        memset(m_buffer.get(), 0, m_displayWidth * m_displayHeight);
    }
}

void FrontPanelDisplay::Present()
{
    if (IsAvailable())
    {
        m_frontPanelControl->PresentBuffer(m_displayWidth * m_displayHeight, m_buffer.get());
    }
}

BufferDesc FrontPanelDisplay::GetBufferDescriptor() const
{
    BufferDesc result = {};
    result.data = m_buffer.get();
    result.size = m_displayWidth * m_displayHeight;
    result.width = m_displayWidth;
    result.height = m_displayHeight;

    return result;
}

FrontPanelDisplay & FrontPanelDisplay::Get()
{
    if (!s_frontPanelDisplayInstance)
        throw std::runtime_error("FrontPanelDisplay is a singleton");

    return *s_frontPanelDisplayInstance;
}

void FrontPanelDisplay::SaveDDSToFile(_In_z_ const wchar_t * fileName) const
{
    if (!IsAvailable())
    {
        return;
    }

    if (!fileName)
    {
        throw std::invalid_argument("Invalid filename");
    }

    // Create file
    ScopedHandle hFile(safe_handle(CreateFile2(fileName, GENERIC_WRITE | DELETE, 0, CREATE_ALWAYS, nullptr)));

    if (!hFile)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    auto_delete_file delonfail(hFile.get());

    // Setup header
    const size_t HEADER_SIZE = sizeof(uint32_t) + sizeof(DDS_HEADER);
    uint8_t fileHeader[HEADER_SIZE] = {};

    *reinterpret_cast<uint32_t*>(&fileHeader[0]) = DDS_MAGIC;

    auto header = reinterpret_cast<DDS_HEADER*>(&fileHeader[0] + sizeof(uint32_t));
    header->size = sizeof(DDS_HEADER);
    header->flags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
    header->height = m_displayHeight;
    header->width = m_displayWidth;
    header->mipMapCount = 1;
    header->caps = DDS_SURFACE_FLAGS_TEXTURE;
    memcpy_s(&header->ddspf, sizeof(header->ddspf), &DDSPF_L8, sizeof(DDS_PIXELFORMAT));

    UINT rowPitch = m_displayWidth;
    UINT imageSize = m_displayWidth * m_displayHeight;

    header->flags |= DDS_HEADER_FLAGS_PITCH;
    header->pitchOrLinearSize = static_cast<uint32_t>(rowPitch);

    // Write header & pixels
    DWORD bytesWritten = 0;
    if (!WriteFile(hFile.get(), fileHeader, static_cast<DWORD>(HEADER_SIZE), &bytesWritten, nullptr))
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    if (bytesWritten != HEADER_SIZE)
    {
        throw std::runtime_error("WriteFile");
    }

    if (!WriteFile(hFile.get(), m_buffer.get(), static_cast<DWORD>(imageSize), &bytesWritten, nullptr))
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    if (bytesWritten != imageSize)
    {
        throw std::runtime_error("WriteFile");
    }

    delonfail.clear();
}

void FrontPanelDisplay::SaveWICToFile(_In_z_ const wchar_t *filename, REFGUID guidContainerFormat) const
{
    if (!IsAvailable())
    {
        return;
    }

    if (!filename)
    {
        throw std::invalid_argument("Invalid filename");
    }

    auto pWIC = GetWIC();
    if (!pWIC)
    {
        throw std::runtime_error("GetWIC");
    }

    ComPtr<IWICStream> stream;
    ThrowIfFailed(
        pWIC->CreateStream(stream.GetAddressOf())
    );

    ThrowIfFailed(
        stream->InitializeFromFilename(filename, GENERIC_WRITE)
    );

    auto_delete_file_wic delonfail(stream, filename);

    ComPtr<IWICBitmapEncoder> encoder;
    ThrowIfFailed(
        pWIC->CreateEncoder(guidContainerFormat, nullptr, encoder.GetAddressOf())
    );

    ThrowIfFailed(
        encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)
    );

    ComPtr<IWICBitmapFrameEncode> frame;
    ThrowIfFailed(
        encoder->CreateNewFrame(frame.GetAddressOf(), nullptr)
    );

    ThrowIfFailed(
        frame->Initialize(nullptr)
    );

    ThrowIfFailed(
        frame->SetSize(m_displayWidth, m_displayHeight)
    );

    ThrowIfFailed(
        frame->SetResolution(72, 72)
    );

    WICPixelFormatGUID targetGuid = GUID_WICPixelFormat8bppGray;
    ThrowIfFailed(
        frame->SetPixelFormat(&targetGuid)
    );

    UINT rowPitch = m_displayWidth;
    UINT imageSize = m_displayWidth * m_displayHeight;

    if (memcmp(&targetGuid, &GUID_WICPixelFormat8bppGray, sizeof(WICPixelFormatGUID)) != 0)
    {
        // Conversion required to write
        ComPtr<IWICBitmap> source;
        ThrowIfFailed(
            pWIC->CreateBitmapFromMemory(m_displayWidth, m_displayHeight, GUID_WICPixelFormat8bppGray,
                rowPitch, imageSize,
                reinterpret_cast<BYTE*>(m_buffer.get()), source.GetAddressOf())
        );

        ComPtr<IWICFormatConverter> FC;
        ThrowIfFailed(
            pWIC->CreateFormatConverter(FC.GetAddressOf())
        );

        BOOL canConvert = FALSE;
        ThrowIfFailed(
            FC->CanConvert(GUID_WICPixelFormat8bppGray, targetGuid, &canConvert)
        );

        if (!canConvert)
        {
            throw std::runtime_error("CanConvert");
        }

        ThrowIfFailed(
            FC->Initialize(source.Get(), targetGuid, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeFixedGray16)
        );

        WICRect rect = { 0, 0, static_cast<INT>(m_displayWidth), static_cast<INT>(m_displayHeight) };
        ThrowIfFailed(
            frame->WriteSource(FC.Get(), &rect)
        );
    }
    else
    {
        ThrowIfFailed(
            frame->WritePixels(
                m_displayHeight,
                rowPitch, imageSize,
                reinterpret_cast<BYTE*>(m_buffer.get()))
        );
    }

    ThrowIfFailed(
        frame->Commit()
    );

    ThrowIfFailed(
        encoder->Commit()
    );

    delonfail.clear();
}

BufferDesc FrontPanelDisplay::LoadWICFromFile(_In_z_ const wchar_t* filename, std::unique_ptr<uint8_t[]>& data, unsigned int frameindex)
{
    data.reset();

    if (!filename)
    {
        throw std::invalid_argument("Invalid filename");
    }

    BufferDesc result = {};
    if (!IsAvailable())
    {
        return result;
    }

    auto pWIC = GetWIC();
    if (!pWIC)
    {
        throw std::runtime_error("GetWIC");
    }

    ComPtr<IWICBitmapDecoder> decoder;
    ThrowIfFailed(
        pWIC->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf())
    );

    UINT frameCount = 0;
    ThrowIfFailed(
        decoder->GetFrameCount(&frameCount)
    );

    if (frameindex >= frameCount)
    {
        throw std::out_of_range("Frame index invalid");
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    ThrowIfFailed(
        decoder->GetFrame(frameindex, frame.GetAddressOf())
    );

    UINT width, height;
    ThrowIfFailed(
        frame->GetSize(&width, &height)
    );

    WICPixelFormatGUID pixelFormat;
    ThrowIfFailed(
        frame->GetPixelFormat(&pixelFormat)
    );

    UINT rowPitch = m_displayWidth;
    UINT imageSize = m_displayWidth * m_displayHeight;

    // Load image data
    data.reset(new uint8_t[imageSize]);

    if (memcmp(&GUID_WICPixelFormat8bppGray, &pixelFormat, sizeof(GUID)) == 0
        && m_displayWidth == width
        && m_displayHeight == height)
    {
        // No format conversion or resize needed
        ThrowIfFailed(
            frame->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), data.get())
        );
    }
    else if (m_displayWidth != width || m_displayHeight != height)
    {
        // Resize
        ComPtr<IWICBitmapScaler> scaler;
        ThrowIfFailed(
            pWIC->CreateBitmapScaler(scaler.GetAddressOf())
        );

        ThrowIfFailed(
            scaler->Initialize(frame.Get(), m_displayWidth, m_displayHeight, WICBitmapInterpolationModeFant)
        );

        WICPixelFormatGUID pfScaler;
        ThrowIfFailed(
            scaler->GetPixelFormat(&pfScaler)
        );

        if (memcmp(&GUID_WICPixelFormat8bppGray, &pfScaler, sizeof(GUID)) == 0)
        {
            // No format conversion needed
            ThrowIfFailed(
                scaler->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), data.get())
            );
        }
        else
        {
            ComPtr<IWICFormatConverter> FC;
            ThrowIfFailed(
                pWIC->CreateFormatConverter(FC.GetAddressOf())
            );

            BOOL canConvert = FALSE;
            ThrowIfFailed(
                FC->CanConvert(pfScaler, GUID_WICPixelFormat8bppGray, &canConvert)
            );

            if (!canConvert)
            {
                throw std::runtime_error("CanConvert");
            }

            ThrowIfFailed(
                FC->Initialize(scaler.Get(), GUID_WICPixelFormat8bppGray,
                    WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut)
            );

            ThrowIfFailed(
                FC->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), data.get())
            );
        }
    }
    else
    {
        // Format conversion but no resize
        ComPtr<IWICFormatConverter> FC;
        ThrowIfFailed(
            pWIC->CreateFormatConverter(FC.GetAddressOf())
        );

        BOOL canConvert = FALSE;
        ThrowIfFailed(
            FC->CanConvert(pixelFormat, GUID_WICPixelFormat8bppGray, &canConvert)
        );

        if (!canConvert)
        {
            throw std::runtime_error("CanConvert");
        }

        ThrowIfFailed(
            FC->Initialize(frame.Get(), GUID_WICPixelFormat8bppGray,
                WICBitmapDitherTypeErrorDiffusion, nullptr, 0, WICBitmapPaletteTypeMedianCut)
        );

        ThrowIfFailed(
            FC->CopyPixels(nullptr, static_cast<UINT>(rowPitch), static_cast<UINT>(imageSize), data.get())
        );
    }

    result.data = data.get();
    result.size = imageSize;
    result.width = m_displayWidth;
    result.height = m_displayHeight;
    return result;
}

BufferDesc FrontPanelDisplay::LoadWICFromFile(_In_z_ const wchar_t* filename, unsigned int frameindex)
{
    BufferDesc result = LoadWICFromFile(filename, m_buffer, frameindex);

    // LoadWicFromFile is robust and will scale the image dimensions to fit the front panel...
    // ...assert here if this is not true:
    assert(m_displayWidth == result.width);
    assert(m_displayHeight == result.height);

    return result;
}