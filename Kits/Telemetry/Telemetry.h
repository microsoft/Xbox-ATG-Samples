//--------------------------------------------------------------------------------------
// Telemetry.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifdef ATG_TELEMETRY_EXPORTS
#define ATG_TELEMETRY_API __declspec(dllexport)
#else
#define ATG_TELEMETRY_API __declspec(dllimport)
#endif

#include <stdint.h>

namespace ATG
{
    // Must register the event provider before writing any events
    ATG_TELEMETRY_API uint32_t EventRegisterATGSampleTelemetry();
    ATG_TELEMETRY_API uint32_t EventUnregisterATGSampleTelemetry();

    // Returns true if you've called EventRegisterATGSampleTelemetry()
    ATG_TELEMETRY_API bool EventEnabledSampleLoaded();

    // Log the SampleLoaded event
    // requires that you call EventRegisterATGSampleTelemetry() first
    ATG_TELEMETRY_API uint32_t EventWriteSampleLoaded(const wchar_t* exeName);
}
