//--------------------------------------------------------------------------------------
// File: TextConsole.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "TextConsole.h"

#include "SimpleMath.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include <assert.h>

using Microsoft::WRL::ComPtr;

using namespace DirectX;
using namespace DX;

const XMVECTORF32 TextConsole::Line::s_defaultColor = Colors::Transparent;

TextConsole::TextConsole()
    : m_layout{},
    m_foregroundColor(1.f, 1.f, 1.f, 1.f),
    m_debugOutput(false),
    m_columns(0),
    m_rows(0)
{
    Clear();
}

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
_Use_decl_annotations_
TextConsole::TextConsole(
    ID3D12Device* device,
    ResourceUploadBatch& upload,
    const RenderTargetState& rtState,
    const wchar_t* fontName,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor)
    : m_layout{},
    m_foregroundColor(1.f, 1.f, 1.f, 1.f),
    m_debugOutput(false),
    m_columns(0),
    m_rows(0)
{
    RestoreDevice(device, upload, rtState, fontName, cpuDescriptor, gpuDescriptor);

    Clear();
}
#else
_Use_decl_annotations_
TextConsole::TextConsole(ID3D11DeviceContext* context, const wchar_t* fontName)
    : m_layout{},
    m_foregroundColor(1.f, 1.f, 1.f, 1.f),
    m_debugOutput(false),
    m_columns(0),
    m_rows(0)
{
    RestoreDevice(context, fontName);

    Clear();
}
#endif

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
void TextConsole::Render(_In_ ID3D12GraphicsCommandList* commandList)
#else
void TextConsole::Render()
#endif
{
    if (!m_lines)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);

    float lineSpacing = m_font->GetLineSpacing();

    float x = float(m_layout.left);
    float y = float(m_layout.top);

    XMVECTOR foregroundColor = XMLoadFloat4(&m_foregroundColor);

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
    m_batch->Begin(commandList);
#else
    m_batch->Begin();
#endif

    auto textLine = static_cast<unsigned int>(m_currentLine + 1) % m_rows;

    for (unsigned int line = 0; line < m_rows; ++line)
    {
        XMFLOAT2 pos(x, y + lineSpacing * float(line));

        if (*(m_lines[textLine].m_text))
        {
            XMVECTOR lineColor = XMLoadFloat4(&m_lines[textLine].m_textColor);
            m_font->DrawString(m_batch.get(), m_lines[textLine].m_text, pos, XMColorEqual(lineColor, Line::s_defaultColor) ? foregroundColor : lineColor);
        }

        textLine = static_cast<unsigned int>(textLine + 1) % m_rows;
    }

    m_batch->End();
}

void TextConsole::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_buffer)
    {
        memset(m_buffer.get(), 0, sizeof(wchar_t) * (m_columns + 1) * m_rows);
    }

    if (m_lines)
    {
        for (unsigned int line = 0; line < m_rows; ++line)
        {
            m_lines[line].SetColor();
        }
    }

    m_currentColumn = m_currentLine = 0;
}

_Use_decl_annotations_
void TextConsole::Write(const wchar_t* str)
{
    Write(Line::s_defaultColor, str);
}

_Use_decl_annotations_
void XM_CALLCONV TextConsole::Write(FXMVECTOR color, const wchar_t* str)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ProcessString(color, str);

#ifndef NDEBUG
    if (m_debugOutput)
    {
        OutputDebugStringW(str);
    }
#endif
}

_Use_decl_annotations_
void TextConsole::WriteLine(const wchar_t* str)
{
    WriteLine(Line::s_defaultColor, str);
}

_Use_decl_annotations_
void XM_CALLCONV TextConsole::WriteLine(FXMVECTOR color, const wchar_t* str)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ProcessString(color, str);
    IncrementLine();

#ifndef NDEBUG
    if (m_debugOutput)
    {
        OutputDebugStringW(str);
        OutputDebugStringW(L"\n");
    }
#endif
}

_Use_decl_annotations_
void TextConsole::Format(const wchar_t* strFormat, ...)
{
    va_list argList;
    va_start(argList, strFormat);

    FormatImpl(Line::s_defaultColor, strFormat, argList);

    va_end(argList);
}

_Use_decl_annotations_
void TextConsole::Format(CXMVECTOR color, const wchar_t* strFormat, ...)
{
    va_list argList;
    va_start(argList, strFormat);

    FormatImpl(color, strFormat, argList);

    va_end(argList);
}

_Use_decl_annotations_
void TextConsole::FormatImpl(CXMVECTOR color, const wchar_t* strFormat, va_list args)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto len = size_t(_vscwprintf(strFormat, args) + 1);

    if (m_tempBuffer.size() < len)
        m_tempBuffer.resize(len);

    memset(m_tempBuffer.data(), 0, sizeof(wchar_t) * len);

    vswprintf_s(m_tempBuffer.data(), m_tempBuffer.size(), strFormat, args);

    ProcessString(color, m_tempBuffer.data());

#ifndef NDEBUG
    if (m_debugOutput)
    {
        OutputDebugStringW(m_tempBuffer.data());
    }
#endif
}

void TextConsole::SetWindow(const RECT& layout)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_layout = layout;

    assert(m_font != 0);

    float lineSpacing = m_font->GetLineSpacing();
    unsigned int rows = std::max<unsigned int>(1, static_cast<unsigned int>(float(layout.bottom - layout.top) / lineSpacing));

    RECT fontLayout = m_font->MeasureDrawBounds(L"X", XMFLOAT2(0, 0));
    unsigned int columns = std::max<unsigned int>(1, static_cast<unsigned int>(float(layout.right - layout.left) / float(fontLayout.right - fontLayout.left)));

    std::unique_ptr<wchar_t[]> buffer(new wchar_t[(columns + 1) * rows]);
    memset(buffer.get(), 0, sizeof(wchar_t) * (columns + 1) * rows);

    std::unique_ptr<Line[]> lines(new Line[rows]);
    for (unsigned int line = 0; line < rows; ++line)
    {
        lines[line].m_text = buffer.get() + (columns + 1) * line;
    }

    if (m_lines)
    {
        unsigned int c = std::min<unsigned int>(columns, m_columns);
        unsigned int r = std::min<unsigned int>(rows, m_rows);

        for (unsigned int line = 0; line < r; ++line)
        {
            memcpy(lines[line].m_text, m_lines[line].m_text, c * sizeof(wchar_t));
            lines[line].m_textColor = m_lines[line].m_textColor;
        }
    }

    std::swap(columns, m_columns);
    std::swap(rows, m_rows);
    std::swap(buffer, m_buffer);
    std::swap(lines, m_lines);

    if ((m_currentColumn >= m_columns) || (m_currentLine >= m_rows))
    {
        IncrementLine();
    }
}

void TextConsole::ReleaseDevice()
{
    m_batch.reset();
    m_font.reset();
#if defined(__d3d11_h__) || defined(__d3d11_x_h__)
    m_context.Reset();
#endif
}

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
_Use_decl_annotations_
void TextConsole::RestoreDevice(
    ID3D12Device* device,
    ResourceUploadBatch& upload,
    const RenderTargetState& rtState,
    const wchar_t* fontName,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor)

{
    {
        SpriteBatchPipelineStateDescription pd(rtState);
        m_batch = std::make_unique<SpriteBatch>(device, upload, pd);
    }

    m_font = std::make_unique<SpriteFont>(device, upload, fontName, cpuDescriptor, gpuDescriptor);

    m_font->SetDefaultCharacter(L' ');
}

void TextConsole::SetViewport(const D3D12_VIEWPORT& viewPort)
{
    if (m_batch)
    {
        m_batch->SetViewport(viewPort);
    }
}
#else
void TextConsole::RestoreDevice(ID3D11DeviceContext* context, const wchar_t* fontName)
{
    m_context = context;

    m_batch = std::make_unique<SpriteBatch>(context);

    ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());

    m_font = std::make_unique<SpriteFont>(device.Get(), fontName);

    m_font->SetDefaultCharacter(L' ');
}

void TextConsole::SetViewport(const D3D11_VIEWPORT& viewPort)
{
    if (m_batch)
    {
        m_batch->SetViewport(viewPort);
    }
}
#endif

void TextConsole::SetRotation(DXGI_MODE_ROTATION rotation)
{
    if (m_batch)
    {
        m_batch->SetRotation(rotation);
    }
}

_Use_decl_annotations_
void TextConsole::ProcessString(FXMVECTOR color, const wchar_t* str)
{
    if (!m_lines)
        return;

    m_lines[m_currentLine].SetColor(color);

    float width = float(m_layout.right - m_layout.left);

    for (const wchar_t* ch = str; *ch != 0; ++ch)
    {
        if (*ch == '\n')
        {
            IncrementLine();
            m_lines[m_currentLine].SetColor(color);
            continue;
        }

        bool increment = false;

        if (m_currentColumn >= m_columns)
        {
            increment = true;
        }
        else
        {
            m_lines[m_currentLine].m_text[m_currentColumn] = *ch;

            auto fontSize = m_font->MeasureString(m_lines[m_currentLine].m_text);
            if (XMVectorGetX(fontSize) > width)
            {
                m_lines[m_currentLine].m_text[m_currentColumn] = L'\0';

                increment = true;
            }
        }

        if (increment)
        {
            IncrementLine();
            m_lines[m_currentLine].m_text[0] = *ch;
            m_lines[m_currentLine].SetColor(color);
        }

        ++m_currentColumn;
    }
}

void TextConsole::IncrementLine()
{
    if (!m_lines)
        return;

    m_currentLine = (m_currentLine + 1) % m_rows;
    m_currentColumn = 0;
    memset(m_lines[m_currentLine].m_text, 0, sizeof(wchar_t) * (m_columns + 1));
}

//--------------------------------------------------------------------------------------
#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
TextConsoleImage::TextConsoleImage() :
    TextConsole(),
    m_bgGpuDescriptor{},
    m_bgSize{}
{
}
#else
TextConsoleImage::TextConsoleImage() :
    TextConsole()
{
}
#endif

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
_Use_decl_annotations_
TextConsoleImage::TextConsoleImage(
    ID3D12Device* device,
    ResourceUploadBatch& upload,
    const RenderTargetState& rtState,
    const wchar_t* fontName,
    const wchar_t* image,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorFont, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorFont,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorImage, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorImage) :
    TextConsole(),
    m_bgGpuDescriptor{},
    m_bgSize{}
{
    RestoreDevice(device, upload, rtState, fontName, image,
        cpuDescriptorFont, gpuDescriptorFont,
        cpuDescriptorImage, gpuDescriptorImage);
}
#else
_Use_decl_annotations_
TextConsoleImage::TextConsoleImage(ID3D11DeviceContext* context, const wchar_t* fontName, const wchar_t* image) :
    TextConsole()
{
    RestoreDevice(context, fontName, image);
}
#endif

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
void TextConsoleImage::Render(_In_ ID3D12GraphicsCommandList* commandList)
{
    m_batch->Begin(commandList);

    m_batch->Draw(m_bgGpuDescriptor, m_bgSize, m_fullscreen);

    m_batch->End();

    TextConsole::Render(commandList);
}
#else
void TextConsoleImage::Render()
{
    m_batch->Begin();

    m_batch->Draw(m_background.Get(), m_fullscreen);

    m_batch->End();

    TextConsole::Render();
}
#endif

void TextConsoleImage::SetWindow(const RECT& fullscreen, bool useSafeRect)
{
    m_fullscreen = fullscreen;

    if (useSafeRect)
    {
        TextConsole::SetWindow(
            SimpleMath::Viewport::ComputeTitleSafeArea(UINT(fullscreen.right - fullscreen.left),
                UINT(fullscreen.bottom - fullscreen.top)));
    }
    else
    {
        TextConsole::SetWindow(fullscreen);
    }

    auto width = UINT(std::max<LONG>(fullscreen.right - fullscreen.left, 1));
    auto height = UINT(std::max<LONG>(fullscreen.bottom - fullscreen.top, 1));

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
    D3D12_VIEWPORT vp = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height),
        D3D12_DEFAULT_VIEWPORT_MIN_DEPTH, D3D12_DEFAULT_VIEWPORT_MAX_DEPTH };
    m_batch->SetViewport(vp);
#else
    auto vp = CD3D11_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    m_batch->SetViewport(vp);
#endif
}

void TextConsoleImage::ReleaseDevice()
{
    TextConsole::ReleaseDevice();

    m_background.Reset();
}

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
_Use_decl_annotations_
void TextConsoleImage::RestoreDevice(
    ID3D12Device* device,
    DirectX::ResourceUploadBatch& upload,
    const DirectX::RenderTargetState& rtState,
    const wchar_t* fontName,
    const wchar_t* image,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorFont, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorFont,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorImage, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorImage)
{
    TextConsole::RestoreDevice(device, upload, rtState, fontName, cpuDescriptorFont, gpuDescriptorFont);

    wchar_t ext[_MAX_EXT];
    _wsplitpath_s(image, nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);

    if (_wcsicmp(ext, L".dds") == 0)
    {
        DX::ThrowIfFailed(CreateDDSTextureFromFile(device, upload, image, m_background.ReleaseAndGetAddressOf()));
    }
    else
    {
        DX::ThrowIfFailed(CreateWICTextureFromFile(device, upload, image, m_background.ReleaseAndGetAddressOf()));
    }

    auto desc = m_background->GetDesc();
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    {
        throw std::exception("Only supports 2D images");
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = desc.Format;
    SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = (!desc.MipLevels) ? UINT(-1) : desc.MipLevels;

    device->CreateShaderResourceView(m_background.Get(), &SRVDesc, cpuDescriptorImage);

    m_bgGpuDescriptor = gpuDescriptorImage;
    m_bgSize = XMUINT2(static_cast<uint32_t>(desc.Width), desc.Height);
}
#else
_Use_decl_annotations_
void TextConsoleImage::RestoreDevice(ID3D11DeviceContext* context, const wchar_t* fontName, const wchar_t* image)
{
    TextConsole::RestoreDevice(context, fontName);

    ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());

    wchar_t ext[_MAX_EXT];
    _wsplitpath_s(image, nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);

    if (_wcsicmp(ext, L".dds") == 0)
    {
        DX::ThrowIfFailed(CreateDDSTextureFromFile(device.Get(), image, nullptr, m_background.ReleaseAndGetAddressOf()));
    }
    else
    {
        DX::ThrowIfFailed(CreateWICTextureFromFile(device.Get(), image, nullptr, m_background.ReleaseAndGetAddressOf()));
    }
}
#endif