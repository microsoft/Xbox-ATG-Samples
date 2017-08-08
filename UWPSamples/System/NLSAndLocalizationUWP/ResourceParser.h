//--------------------------------------------------------------------------------------
// ResourceParser.h
//
// Class to handle resource parsing. 
//
// This is just a sample class parsing some temporary resource files. 
// The resource files can be in any format you choose.
// This class doesn't perform many checks against wrong file formatting.
// It assumes that the resource file will not be tampered with. The goal is to show
// how to use resources for localization.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#include "pch.h"
#include <collection.h>

class ResourceParser
{
public:
    ResourceParser(void);
    ResourceParser( wchar_t* localeName, wchar_t* appendImageStr );
    ~ResourceParser(void);

    HRESULT ParseFile( const wchar_t* filename );
    Platform::String^ GetString( Platform::String^  id );
    Platform::String^ GetImage( Platform::String^  id );
    
private:
    void TrimString( std::wstring& wstr );
    
    Platform::String^ m_localeName;
    Platform::String^ m_appendImageStr;
    Platform::Collections::Map< Platform::String^, Platform::String^ >^ m_stringMap;
    Platform::Collections::Map< Platform::String^, Platform::String^ >^ m_imageMap;
};
