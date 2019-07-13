//--------------------------------------------------------------------------------------
// File: TextConsole.h
//
// Renders a simple on screen console where you can output text information on a
// Direct3D surface
//
// Note: This is best used with monospace rather than proportional fonts
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#pragma once

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
#include "RenderTargetState.h"
#include "ResourceUploadBatch.h"
#endif
#include "SpriteBatch.h"
#include "SpriteFont.h"

#include <mutex>
#include <vector>

#include <wrl/client.h>


namespace DX
{
    class TextConsole
    {
    public:
        TextConsole();
#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
        TextConsole(
            _In_ ID3D12Device* device,
            DirectX::ResourceUploadBatch& upload,
            const DirectX::RenderTargetState& rtState,
            _In_z_ const wchar_t* fontName,
            D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor);
#elif defined(__d3d11_h__) || defined(__d3d11_x_h__)
        TextConsole(_In_ ID3D11DeviceContext* context, _In_z_ const wchar_t* fontName);
#else
#   error Please #include <d3d11.h> or <d3d12.h>
#endif

        TextConsole(TextConsole&&) = delete;
        TextConsole& operator= (TextConsole&&) = delete;

        TextConsole(TextConsole const&) = delete;
        TextConsole& operator= (TextConsole const&) = delete;

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
        void Render(_In_ ID3D12GraphicsCommandList* commandList);
#else
        void Render();
#endif

        void Clear();

        void Write(_In_z_ const wchar_t* str);
        void XM_CALLCONV Write(DirectX::FXMVECTOR color, _In_z_ const wchar_t* str);

        void WriteLine(_In_z_ const wchar_t* str);
        void XM_CALLCONV WriteLine(DirectX::FXMVECTOR color, _In_z_ const wchar_t* str);

        void Format(_In_z_ _Printf_format_string_ const wchar_t* strFormat, ...);
        void Format(DirectX::CXMVECTOR color, _In_z_ _Printf_format_string_ const wchar_t* strFormat, ...);

        void SetWindow(const RECT& layout);

        void XM_CALLCONV SetForegroundColor(DirectX::FXMVECTOR color) { DirectX::XMStoreFloat4(&m_foregroundColor, color); }

        void SetDebugOutput(bool debug) { m_debugOutput = debug; }

        void ReleaseDevice();
#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
        void RestoreDevice(
            _In_ ID3D12Device* device,
            DirectX::ResourceUploadBatch& upload,
            const DirectX::RenderTargetState& rtState,
            _In_z_ const wchar_t* fontName,
            D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor);

        void SetViewport(const D3D12_VIEWPORT& viewPort);
#else
        void RestoreDevice(ID3D11DeviceContext* context, const wchar_t* fontName);

        void SetViewport(const D3D11_VIEWPORT& viewPort);
#endif

        void SetRotation(DXGI_MODE_ROTATION rotation);

    protected:
        void FormatImpl(DirectX::CXMVECTOR color, _In_z_ _Printf_format_string_ const wchar_t* strFormat, va_list args);
        void XM_CALLCONV ProcessString(DirectX::FXMVECTOR color, _In_z_ const wchar_t* str);
        void IncrementLine();

        struct Line
        {
            wchar_t*			m_text;
            DirectX::XMFLOAT4	m_textColor;

            static const DirectX::XMVECTORF32 s_defaultColor;

            Line() : m_text(nullptr), m_textColor(s_defaultColor) {}

            void XM_CALLCONV SetColor(DirectX::FXMVECTOR color = s_defaultColor) { XMStoreFloat4(&m_textColor, color); }
        };

        RECT                                            m_layout;
        DirectX::XMFLOAT4                               m_foregroundColor;

        bool                                            m_debugOutput;

        unsigned int                                    m_columns;
        unsigned int                                    m_rows;
        unsigned int                                    m_currentColumn;
        unsigned int                                    m_currentLine;

        std::unique_ptr<wchar_t[]>                      m_buffer;
        std::unique_ptr<Line[]>                         m_lines;
        std::vector<wchar_t>                            m_tempBuffer;

        std::unique_ptr<DirectX::SpriteBatch>           m_batch;
        std::unique_ptr<DirectX::SpriteFont>            m_font;
#if defined(__d3d11_h__) || defined(__d3d11_x_h__)
        Microsoft::WRL::ComPtr<ID3D11DeviceContext>     m_context;
#endif

        std::mutex                                      m_mutex;
    };

    class TextConsoleImage : public TextConsole
    {
    public:
        TextConsoleImage();
#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
        TextConsoleImage(
            _In_ ID3D12Device* device,
            DirectX::ResourceUploadBatch& upload,
            const DirectX::RenderTargetState& rtState,
            _In_z_ const wchar_t* fontName,
            _In_z_ const wchar_t* image,
            D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorFont, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorFont,
            D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorImage, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorImage);
#else
        TextConsoleImage(_In_ ID3D11DeviceContext* context, _In_z_ const wchar_t* fontName, _In_z_ const wchar_t* image);
#endif

        TextConsoleImage(TextConsoleImage&&) = delete;
        TextConsoleImage& operator= (TextConsoleImage&&) = delete;

        TextConsoleImage(TextConsoleImage const&) = delete;
        TextConsoleImage& operator= (TextConsoleImage const&) = delete;

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
        void Render(_In_ ID3D12GraphicsCommandList* commandList);
#else
        void Render();
#endif

        void SetWindow(const RECT& layout) = delete;
        void SetWindow(const RECT& fullscreen, bool useSafeRect);

        void ReleaseDevice();
#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
        void RestoreDevice(
            _In_ ID3D12Device* device,
            DirectX::ResourceUploadBatch& upload,
            const DirectX::RenderTargetState& rtState,
            _In_z_ const wchar_t* fontName,
            D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor) = delete;
        void RestoreDevice(
            _In_ ID3D12Device* device,
            DirectX::ResourceUploadBatch& upload,
            const DirectX::RenderTargetState& rtState,
            _In_z_ const wchar_t* fontName,
            _In_z_ const wchar_t* image,
            D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorFont, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorFont,
            D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorImage, D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorImage);
#else
        void RestoreDevice(_In_ ID3D11DeviceContext* context, _In_z_ const wchar_t* fontName) = delete;
        void RestoreDevice(_In_ ID3D11DeviceContext* context, _In_z_ const wchar_t* fontName, _In_z_ const wchar_t* image);
#endif

    private:
#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
        D3D12_GPU_DESCRIPTOR_HANDLE                         m_bgGpuDescriptor;
        DirectX::XMUINT2                                    m_bgSize;
        Microsoft::WRL::ComPtr<ID3D12Resource>              m_background;
#else
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_background;
#endif
        RECT                                                m_fullscreen;
    };
}