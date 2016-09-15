//--------------------------------------------------------------------------------------
// File: ControllerHelp.h
//
// Help display for GamePad-based control schemes
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#include "CommonStates.h"
#include "Effects.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "PrimitiveBatch.h"
#include "VertexTypes.h"

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
#include "DescriptorHeap.h"
#include "ResourceUploadBatch.h"
#elif !defined(__d3d11_h__) && !defined(__d3d11_x_h__)
#   error Please #include <d3d11.h> or <d3d12.h>
#endif


namespace ATG
{
    enum HelpID
    {
        TITLE_TEXT = 0,
        DESCRIPTION_TEXT,
        LEFT_STICK,
        LEFT_STICK_CLICK,
        RIGHT_STICK,
        RIGHT_STICK_CLICK,
        DPAD_UP,
        DPAD_DOWN,
        DPAD_LEFT,
        DPAD_RIGHT,
        DPAD_ALL,
        RIGHT_SHOULDER,
        RIGHT_TRIGGER,
        LEFT_SHOULDER,
        LEFT_TRIGGER,
        A_BUTTON,
        B_BUTTON,
        X_BUTTON,
        Y_BUTTON,
        MENU_BUTTON,
        VIEW_BUTTON,
        MAX_COUNT
    };

    struct HelpButtonAssignment
    {
        HelpID          id;
        const wchar_t*  buttonText;
    };

    class Help
    {
    public:
        Help(_In_z_ const wchar_t* title, _In_z_ const wchar_t* description,
             _In_count_(buttonCount) const HelpButtonAssignment* buttons, size_t buttonCount, bool linearColors = false);
        ~Help();

#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
        void Render(ID3D12GraphicsCommandList* commandList);

        void RestoreDevice(ID3D12Device* device, DirectX::ResourceUploadBatch& uploadBatch, const DirectX::RenderTargetState& rtState);
#elif defined(__d3d11_h__) || defined(__d3d11_x_h__)
        void Render();

        void RestoreDevice(ID3D11DeviceContext* context);
#endif

        void ReleaseDevice();

        void SetWindow(const RECT& output);

        struct CalloutBox;

    private:
        std::unique_ptr<DirectX::SpriteBatch>                                   m_spriteBatch;
        std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>  m_primBatch;
        std::unique_ptr<DirectX::BasicEffect>                                   m_lineEffect;
        std::unique_ptr<DirectX::SpriteFont>                                    m_spriteFonts[3];
        
#if defined(__d3d12_h__) || defined(__d3d12_x_h__)
        std::unique_ptr<DirectX::DescriptorHeap>                                m_descriptorHeap;

        Microsoft::WRL::ComPtr<ID3D12Resource>                                  m_circleTex;
        Microsoft::WRL::ComPtr<ID3D12Resource>                                  m_gamepadTex;
        Microsoft::WRL::ComPtr<ID3D12Resource>                                  m_backgroundTex;

        DirectX::XMUINT2                                                        m_circleTexSize;
        DirectX::XMUINT2                                                        m_gamepadTexSize;
        DirectX::XMUINT2                                                        m_backgroundTexSize;
#elif defined(__d3d11_h__) || defined(__d3d11_x_h__)
        std::unique_ptr<DirectX::CommonStates>                                  m_states;

        Microsoft::WRL::ComPtr<ID3D11InputLayout>                               m_lineLayout;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>                        m_circleTex;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>                        m_gamepadTex;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>                        m_backgroundTex;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext>                             m_context;
#endif

        bool                                                                    m_linearColors;
        RECT                                                                    m_screenSize;

        size_t                                                                  m_calloutCount;
        CalloutBox*                                                             m_callouts;
    };
}