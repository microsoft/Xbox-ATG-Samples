//--------------------------------------------------------------------------------------
// PIXHelpers.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

// Scoped PIX event.
class ScopedPixEvent
{
public:
    ScopedPixEvent(_In_ ID3D11DeviceContextX* context, UINT64 /*metadata*/, PCWSTR pFormat)
        : mContext(context)
    {
        PIXBeginEvent(mContext, 0, pFormat);
    }

    ScopedPixEvent(UINT64 metadata, PCWSTR pFormat)
        : mContext(nullptr)
    {
        PIXBeginEvent(metadata, pFormat);
    }

    ~ScopedPixEvent()
    {
        mContext ? PIXEndEvent(mContext) : PIXEndEvent();
    }

private:
    ID3D11DeviceContextX* mContext;
};