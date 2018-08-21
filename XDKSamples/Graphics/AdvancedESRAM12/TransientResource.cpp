//--------------------------------------------------------------------------------------
// TransientResource.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "TransientResource.h"

namespace ATG
{
    const ResourceHandle ResourceHandle::Invalid = ResourceHandle{ size_t(-1), size_t(-1), size_t(-1) };
}
