//--------------------------------------------------------------------------------------
// Hash.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#define MD5_DIGEST_LENGTH 16

HRESULT MD5Checksum(_In_ const void* data, size_t byteCount, _Out_bytecap_x_(MD5_DIGEST_LENGTH) uint8_t *digest);
