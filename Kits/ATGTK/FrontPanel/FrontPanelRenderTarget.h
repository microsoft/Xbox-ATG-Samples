//--------------------------------------------------------------------------------------
// FrontPanelRenderTarget.h
//
// Microsoft Xbox One XDK for DirectX 11
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#if _XDK_VER < 0x3AD703ED /* XDK Edition 170300 */
#error This code requires the March 2017 XDK or later
#endif

#include <XboxFrontPanel.h>

#include "BufferDescriptor.h"
#include "PostProcess.h"


namespace ATG
{
    class FrontPanelRenderTarget
    {
    public:
        FrontPanelRenderTarget();
        virtual ~FrontPanelRenderTarget();

        FrontPanelRenderTarget(FrontPanelRenderTarget&&) = default;
        FrontPanelRenderTarget& operator=(FrontPanelRenderTarget&&) = default;

        FrontPanelRenderTarget(const FrontPanelRenderTarget&) = delete;
        FrontPanelRenderTarget& operator=(const FrontPanelRenderTarget&) = delete;

        void CreateDeviceDependentResources(_In_ IXboxFrontPanelControl *frontPanelControl, _In_ ID3D11Device *device);

        void Clear(_In_ ID3D11DeviceContext *context, const float ColorRGBA[4]);
        void SetAsRenderTarget(_In_ ID3D11DeviceContext *context);

        ID3D11Texture2D *GetRenderTarget() const { return m_renderTarget.Get(); }
        ID3D11RenderTargetView*	GetRenderTargetView() const { return m_renderTargetView.Get(); }
        DXGI_FORMAT GetRenderTargetFormat() const { return m_renderTargetFormat; }

        // Render a grayscale image using the provided shader resource view as the source
        void GPUBlit(_In_ ID3D11DeviceContext *context, _In_ ID3D11ShaderResourceView *srcSRV);

        // Copy the render target to a staging texture and then copy it back to the CPU
        void CopyToBuffer(_In_ ID3D11DeviceContext *context, BufferDesc &desc);

        // Copy the render target to a staging texture, copy the result back to the CPU
        // and then present it to the front panel display
        void PresentToFrontPanel(_In_ ID3D11DeviceContext *context);

    protected:
        // Front Panel object
        Microsoft::WRL::ComPtr<IXboxFrontPanelControl>  m_frontPanelControl;
        uint32_t                                        m_displayWidth;
        uint32_t                                        m_displayHeight;

        // Render Target and Depth Buffer
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_renderTarget;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_renderTargetView;
        DXGI_FORMAT                                     m_renderTargetFormat;

        // Resources for GPU Blit / Present
        std::unique_ptr<DirectX::BasicPostProcess>      m_panelBlt;
        std::unique_ptr<uint8_t[]>                      m_buffer;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_staging;
    };
}
