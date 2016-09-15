//--------------------------------------------------------------------------------------
// File: ToastManager.h
//
// A helper for showing text messages as toast notifications
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY != WINAPI_FAMILY_APP)
#error This header only works with UWP apps
#endif

#include <exception>
#include <vector>
#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>

#include <stdio.h>


namespace DX
{
    class ToastManager
    {
    public:
        ToastManager()
        {
            using namespace Microsoft::WRL;
            using namespace Microsoft::WRL::Wrappers;
            
            ComPtr<ABI::Windows::UI::Notifications::IToastNotificationManagerStatics> mgrStatics;
            HRESULT hr = Windows::Foundation::GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(), mgrStatics.GetAddressOf());
            if (FAILED(hr))
            {
                throw std::exception("CreateToastNotifier");
            }

            hr = mgrStatics->CreateToastNotifier(mToastNotifier.GetAddressOf());
            if (FAILED(hr))
            {
                throw std::exception("CreateToastNotifier");
            }

            hr = Windows::Foundation::GetActivationFactory(HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(), mNotificationFactory.GetAddressOf());
            if (FAILED(hr))
            {
                throw std::exception("ToastNotificationFactory");
            }
        }

        inline void Show(_In_z_ const wchar_t* line)
        {
            static const wchar_t* s_format = L"<toast>"
                " <visual version='1'>"
                "<binding template='ToastText01'>"
                "<text id ='1'>%ls</text>"
                "</binding>"
                "</visual>"
                "</toast>";

            #pragma warning(suppress: 4996)
            int len = _snwprintf(nullptr, 0, s_format, line);
            if (len < 0)
                return;

            if (static_cast<size_t>(len) >= mBuffer.size())
            {
                mBuffer.resize(len + 1);
            }

            if (swprintf_s(mBuffer.data(), mBuffer.size(), s_format, line) < 0)
                return;

            using namespace Microsoft::WRL;
            using namespace Microsoft::WRL::Wrappers;

            ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocumentIO> xmlDocument;
            HRESULT hr = Windows::Foundation::ActivateInstance(HStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(), xmlDocument.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                hr = xmlDocument->LoadXml(HStringReference(mBuffer.data()).Get());
                if (SUCCEEDED(hr))
                {
                    Microsoft::WRL::ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocument> xml;
                    hr = xmlDocument.CopyTo(xml.GetAddressOf());
                    if (SUCCEEDED(hr))
                    {
                        ComPtr<ABI::Windows::UI::Notifications::IToastNotification> notification;
                        hr = mNotificationFactory->CreateToastNotification(xml.Get(), notification.GetAddressOf());
                        if (SUCCEEDED(hr))
                        {
                            mToastNotifier->Show(notification.Get());
                        }
                    }
                }
            }
        }

        inline void Show(_In_z_ const wchar_t* line1, _In_z_ const wchar_t* line2)
        {
            static const wchar_t* s_format = L"<toast>"
                " <visual version='1'>"
                "<binding template='ToastText02'>"
                "<text id ='1'>%ls</text>"
                "<text id ='2'>%ls</text>"
                "</binding>"
                "</visual>"
                "</toast>";

            #pragma warning(suppress: 4996)
            int len = _snwprintf(nullptr, 0, s_format, line1, line2);
            if (len < 0)
                return;

            if (static_cast<size_t>(len) >= mBuffer.size())
            {
                mBuffer.resize(len + 1);
            }

            if (swprintf_s(mBuffer.data(), mBuffer.size(), s_format, line1, line2) < 0)
                return;

            using namespace Microsoft::WRL;
            using namespace Microsoft::WRL::Wrappers;

            ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocumentIO> xmlDocument;
            HRESULT hr = Windows::Foundation::ActivateInstance(HStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(), xmlDocument.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                hr = xmlDocument->LoadXml(HStringReference(mBuffer.data()).Get());
                if (SUCCEEDED(hr))
                {
                    Microsoft::WRL::ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocument> xml;
                    hr = xmlDocument.CopyTo(xml.GetAddressOf());
                    if (SUCCEEDED(hr))
                    {
                        ComPtr<ABI::Windows::UI::Notifications::IToastNotification> notification;
                        hr = mNotificationFactory->CreateToastNotification(xml.Get(), notification.GetAddressOf());
                        if (SUCCEEDED(hr))
                        {
                            mToastNotifier->Show(notification.Get());
                        }
                    }
                }
            }
        }

        inline void Show(_In_z_ const wchar_t* line1, _In_z_ const wchar_t* line2, _In_z_ const wchar_t* line3)
        {
            static const wchar_t* s_format = L"<toast>"
                " <visual version='1'>"
                "<binding template='ToastText04'>"
                "<text id ='1'>%ls</text>"
                "<text id ='2'>%ls</text>"
                "<text id ='3'>%ls</text>"
                "</binding>"
                "</visual>"
                "</toast>";

            #pragma warning(suppress: 4996)
            int len = _snwprintf(nullptr, 0, s_format, line1, line2, line3);
            if (len < 0)
                return;

            if (static_cast<size_t>(len) >= mBuffer.size())
            {
                mBuffer.resize(len + 1);
            }

            if (swprintf_s(mBuffer.data(), mBuffer.size(), s_format, line1, line2, line3) < 0)
                return;

            using namespace Microsoft::WRL;
            using namespace Microsoft::WRL::Wrappers;

            ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocumentIO> xmlDocument;
            HRESULT hr = Windows::Foundation::ActivateInstance(HStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(), xmlDocument.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                hr = xmlDocument->LoadXml(HStringReference(mBuffer.data()).Get());
                if (SUCCEEDED(hr))
                {
                    Microsoft::WRL::ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocument> xml;
                    hr = xmlDocument.CopyTo(xml.GetAddressOf());
                    if (SUCCEEDED(hr))
                    {
                        ComPtr<ABI::Windows::UI::Notifications::IToastNotification> notification;
                        hr = mNotificationFactory->CreateToastNotification(xml.Get(), notification.GetAddressOf());
                        if (SUCCEEDED(hr))
                        {
                            mToastNotifier->Show(notification.Get());
                        }
                    }
                }
            }
        }

    private:
        std::vector<wchar_t>                                                                mBuffer;
        Microsoft::WRL::ComPtr<ABI::Windows::UI::Notifications::IToastNotifier>             mToastNotifier;
        Microsoft::WRL::ComPtr<ABI::Windows::UI::Notifications::IToastNotificationFactory>  mNotificationFactory;
    };
}
