//--------------------------------------------------------------------------------------
// Hash.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Hash.h"

#include <bcrypt.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#endif

class ScopedBCrypHashHandle
{
public:
    ScopedBCrypHashHandle() : m_handle(nullptr) { }
    ScopedBCrypHashHandle(BCRYPT_HASH_HANDLE h) : m_handle(h) { }
    ~ScopedBCrypHashHandle() { BCryptDestroyHash(m_handle); }

    operator BCRYPT_HASH_HANDLE() { return m_handle; }
    BCRYPT_HASH_HANDLE* operator&() { return &m_handle; }

private:
    BCRYPT_HASH_HANDLE m_handle;
};


HRESULT MD5Checksum(const void* data, size_t byteCount, uint8_t *digest)
{
    if (!data || !digest || byteCount == 0)
        return E_INVALIDARG;

    memset(digest, 0, MD5_DIGEST_LENGTH);

    NTSTATUS status;

    // Ensure have the MD5 algorithm ready
    static BCRYPT_ALG_HANDLE s_algid = 0;
    if (!s_algid)
    {
        status = BCryptOpenAlgorithmProvider(&s_algid, BCRYPT_MD5_ALGORITHM, MS_PRIMITIVE_PROVIDER, 0);
        if (!NT_SUCCESS(status))
        {
            return HRESULT_FROM_NT(status);
        }

        DWORD len = 0, res = 0;
        status = BCryptGetProperty(s_algid, BCRYPT_HASH_LENGTH, (PBYTE)&len, sizeof(DWORD), &res, 0);
        if (!NT_SUCCESS(status) || res != sizeof(DWORD) || len != MD5_DIGEST_LENGTH)
        {
            return E_FAIL;
        }
    }

    // Create hash object
    ScopedBCrypHashHandle hobj;

    status = BCryptCreateHash(s_algid, &hobj, nullptr, 0, nullptr, 0, 0);
    if (!NT_SUCCESS(status))
    {
        return HRESULT_FROM_NT(status);
    }

    status = BCryptHashData(hobj, (PBYTE)data, (ULONG)byteCount, 0);
    if (!NT_SUCCESS(status))
    {
        return HRESULT_FROM_NT(status);
    }

    status = BCryptFinishHash(hobj, (PBYTE)digest, MD5_DIGEST_LENGTH, 0);
    if (!NT_SUCCESS(status))
    {
        return HRESULT_FROM_NT(status);
    }

    return S_OK;
}
