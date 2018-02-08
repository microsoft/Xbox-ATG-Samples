//--------------------------------------------------------------------------------------
// EtwScopedEvent.h
//
// Simple wrapper class that manages Start / End events correctly for bracketing.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef ETWSCOPEDEVENT_H_INCLUDED
#define ETWSCOPEDEVENT_H_INCLUDED

//--------------------------------------------------------------------------------------
//  These defines and typedefs are required to allow us to include
//  the auto-generated header.

#ifndef _TRACEHANDLE_DEFINED
#define _TRACEHANDLE_DEFINED
typedef ULONG64 TRACEHANDLE, *PTRACEHANDLE;
#endif  // !_TRACEHANDLE_DEFINED

#ifndef EVENT_CONTROL_CODE_DISABLE_PROVIDER
#define EVENT_CONTROL_CODE_DISABLE_PROVIDER 0
#endif // !EVENT_CONTROL_CODE_DISABLE_PROVIDER

#ifndef EVENT_CONTROL_CODE_ENABLE_PROVIDER
#define EVENT_CONTROL_CODE_ENABLE_PROVIDER  1
#endif // !EVENT_CONTROL_CODE_ENABLE_PROVIDER

#ifndef EVENT_CONTROL_CODE_CAPTURE_STATE
#define EVENT_CONTROL_CODE_CAPTURE_STATE    2
#endif // !EVENT_CONTROL_CODE_CAPTURE_STATE

#include "etwproviderGenerated.h"
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: EtwScopedEvent
// Desc: A class that can be used to bracket processing and which ensures the
//       correct information is provided for WPA regions.
//--------------------------------------------------------------------------------------

class EtwScopedEvent final
{
public:

    //  Creation of named region. We don't copy the string
    //  so it will need to remain in scope over the lifetime
    //  of this object.
    EtwScopedEvent( const WCHAR* pTag ) :
        m_cpuId( GetCurrentProcessorNumber() ),
        m_seq( EtwScopedEvent::g_Sequence++ ),
        m_pTag( pTag )
    {
        EventWriteBlockStart( m_cpuId, m_seq, m_pTag );
    }

    ~EtwScopedEvent()
    {
        //  We log the actual processor number at the point the object
        //  is destroyed. If this doesn't match the processor on creation
        //  then WPA will struggle to display things correctly, but it's
        //  not clear that there's a better approach.
        EventWriteBlockStop( GetCurrentProcessorNumber(), m_seq, m_pTag );
    }

private:

    //  Prevent creation of anonymous regions.
    EtwScopedEvent()
    {
    }

    //  Each event gets a unique number allocated from this counter. 
    //  Wrapping is only an issue if you wrap within a SINGLE capture. 
    //  Given the number of events per capture that is unlikely to be a 
    //  problem.
    static UINT32   g_Sequence;

    UINT32          m_cpuId;    // Processor event was created on
    UINT32          m_seq;      // Unique identifier
    const WCHAR*    m_pTag;     // Label for event - must remain valid over lifetime of event object
};

//  Macro that creates uniquely named event objects for bracketing processing.
#define ETWScopedEvent( tag )   EtwScopedEvent etwEvent ## __LINE__ ( tag )

#endif  // !ETWSCOPEDEVENT_H_INCLUDED

