//--------------------------------------------------------------------------------------
// AggregateNavigation.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <stdint.h>

//
// Class to aggregate navigation readings multiple controllers
//
class AggregateNavigation
{
    public:
        AggregateNavigation() : aggregation_(static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::None))
        {
        }

        void Reset()
        {
            aggregation_ = static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::None);
        }

        void AddReading(Windows::Xbox::Input::IController^ controller)
        {
            if (controller == nullptr)
            {
                return;
            }

            try
            {
                auto iNavigation = (Windows::Xbox::Input::INavigationController^)controller;
                auto iReading = iNavigation->GetNavigationReading();
                if (iReading != nullptr)
                {
                    aggregation_ |= static_cast<uint32_t>(iReading->Buttons);
                }
            }
            catch (Platform::InvalidCastException^)
            {
            }
        }

        bool Up()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::Up)) != 0;
        }
        bool Down()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::Down)) != 0;
        }
        bool Left()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::Left)) != 0;
        }
        bool Right()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::Right)) != 0;
        }

        bool Accept()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::Accept)) != 0;
        }
        bool Cancel()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::Cancel)) != 0;
        }
        bool X()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::X)) != 0;
        }
        bool Y()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::Y)) != 0;
        }

        bool Menu()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::Menu)) != 0;
        }
        bool View()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::View)) != 0;
        }

        bool PreviousPage()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::PreviousPage)) != 0;
        }
        bool NextPage()
        {
            return (aggregation_ & static_cast<uint32_t>(Windows::Xbox::Input::NavigationButtons::NextPage)) != 0;
        }

    private:
        uint32_t aggregation_;
};
