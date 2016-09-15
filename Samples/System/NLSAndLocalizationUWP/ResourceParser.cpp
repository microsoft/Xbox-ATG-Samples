//--------------------------------------------------------------------------------------
// ResourceParser.cpp
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
#include "ResourceParser.h"

#define FILE_CONTENT_SIZE 2048

ResourceParser::ResourceParser(void)
{
    m_localeName = ref new Platform::String();
    m_appendImageStr = ref new Platform::String();
    m_stringMap = ref new Platform::Collections::Map< Platform::String^, Platform::String^ >( );
    m_imageMap  = ref new Platform::Collections::Map< Platform::String^, Platform::String^ >( );
}

ResourceParser::ResourceParser( wchar_t* localeName, wchar_t* appendImageStr )
{
    m_localeName = ref new Platform::String( localeName );
    m_appendImageStr = ref new Platform::String( appendImageStr );
    m_stringMap = ref new Platform::Collections::Map< Platform::String^, Platform::String^ >( );
    m_imageMap  = ref new Platform::Collections::Map< Platform::String^, Platform::String^ >( );
}

// Parse the resources file to get the string and image data
HRESULT ResourceParser::ParseFile( const wchar_t* fileName )
{
    HANDLE hFile = CreateFile2( fileName,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            OPEN_EXISTING,
                            NULL );

    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE)
    {
        return GetLastError();
    }

    // Read the file contents into a buffer
    wchar_t buf[ FILE_CONTENT_SIZE ];
    DWORD charRead;
    if ( hFile )
    {
        DWORD bytesRead;
        if ( !ReadFile( hFile, buf, FILE_CONTENT_SIZE, &bytesRead, NULL ) )
        {
            return GetLastError();
        }

        charRead = bytesRead / sizeof( wchar_t );
        buf[charRead] = '\0';
    }

    std::wstring wstrbuf( buf );
    while ( wstrbuf.length() > 0 )
    {
        std::wstring wstr;
    
        std::string token;
        std::wstring::size_type pos1, pos2, bufEndPos;
        Platform::String^ idToken = ref new Platform::String( L"" );
        Platform::String^ valueToken = ref new Platform::String( L"" );

        pos1 = wstrbuf.find( L'<' );
        pos2 = wstrbuf.find( L'>' );
        bufEndPos = pos2;

        // Handle the case where a / is present just before />
        if( wstrbuf[ pos2-1 ] == '/' )
        {
            --pos2;
        }
        wstr = wstrbuf.substr( pos1+1, pos2-(pos1+1) );
        wstrbuf = wstrbuf.substr( bufEndPos+1 );

        TrimString( wstr );
        pos1 = wstr.find(L' ');
        std::wstring resType = wstr.substr( 0, pos1 );
        wstr = wstr.substr( pos1+1 );
        
        // Parse the data within a node into Id and Value attributes
        // The assumption is that all the nodes in the resource file will have an Id and Value field
        // Put these values into a map which can be retrieved later from the app as and when required
        // Create a separate map for image and string data
        while( wstr.length() > 0 )
        {
            pos1 = wstr.find(L'=');
            std::wstring token1 = wstr.substr( 0, pos1 );
            TrimString( token1 );
            wstr = wstr.substr( pos1+1 );

            // Find the string between quotes
            pos1 = wstr.find( '"' );
            if ( pos1 == std::wstring::npos )
            {
                // Error in structure
                return S_FALSE;
            }
            pos2 = wstr.find( '"', pos1+1 );
        
            while ( pos2 != std::wstring::npos && wstr[pos2-1] == '\\' )
            {
                // Check if a backslash is present within the string. Ignore those quotes
                pos2 = wstr.find( '"', pos2+1 );
            }
            if ( pos2 == std::wstring::npos )
            {
                // Error in structure
                return S_FALSE;
            }

            std::wstring token2 = wstr.substr( pos1+1, pos2-(pos1+1) );
            TrimString( token2 );
            wstr = wstr.substr( pos2+1 );

            if ( _wcsicmp( token1.c_str(), L"ID" ) == 0 )
            {
                idToken = nullptr;
                idToken = ref new Platform::String( token2.c_str() );
            }
            else if ( _wcsicmp( token1.c_str(), L"Value" ) == 0 )
            {
                valueToken = nullptr;
                valueToken = ref new Platform::String( token2.c_str() );
            }

            if ( idToken->Length() > 0 && valueToken->Length() > 0 )
            {
                if ( _wcsicmp( resType.c_str(), L"String" ) == 0 )
                {
                    m_stringMap->Insert( idToken, valueToken );
                }
                else if ( _wcsicmp( resType.c_str(), L"Image" ) == 0 )
                {
                    valueToken = valueToken->Concat( m_appendImageStr, valueToken );
                    m_imageMap->Insert( idToken, valueToken );
                }
                idToken = nullptr;
                valueToken = nullptr;
            }

        }
    }

    CloseHandle( hFile );
    return S_OK;
}

// Trim the string to remove spaces from its beginning and end
void ResourceParser::TrimString( std::wstring& wstr )
{
    // Remove spaces from the begining
    UINT spaceCnt = ( UINT ) wstr.find_first_not_of( L' ' );
    wstr = wstr.substr( spaceCnt );

    // Remove spaces from the end
    spaceCnt = ( UINT ) wstr.find_last_not_of( L' ' );
    wstr = wstr.substr( 0, spaceCnt + 1 );
}

// Get the string value based on the Id
Platform::String^ ResourceParser::GetString( Platform::String^ id )
{
    if ( m_stringMap->HasKey( id ) )
    {
        return m_stringMap->Lookup( id );
    }
    else
    {
        return nullptr;
    }
}

// Get the image name based on the Id
Platform::String^ ResourceParser::GetImage( Platform::String^ id )
{
    if ( m_imageMap->HasKey( id ) )
    {
        return m_imageMap->Lookup( id );
    }
    else
    {
        return nullptr;
    }
}

ResourceParser::~ResourceParser(void)
{
    m_localeName = nullptr;
    m_stringMap  = nullptr;
    m_imageMap   = nullptr;
}
