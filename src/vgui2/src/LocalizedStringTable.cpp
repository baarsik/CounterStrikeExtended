//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "LocalizedStringTable.h"

#include "platform.h"
#include "KeyValues.h"
#include "UnicodeFileHelpers.h"
#include "FileSystem.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui_internal.h"

#ifndef _WIN32
#include "../../common/legacy_text_encoding.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "../../public/localization_service.h"
#include "../../common/resource_text.h"
#include "Scheme.h"
#include "RuntimeLocalization.h"
#include "LocalizationCatalogue.h"
#include "../vgui_surfacelib/FontManager.h"
#include "../vgui_surfacelib/LocalizationFontBackend.h"
#include <cstring>
#include <cwchar>
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

extern CSchemeManager g_Scheme;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );

#define LOCALIZEDSTRINGTABLE_SOURCE_PATH \
	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\vgui2\\src\\LocalizedStringTable.cpp"
#define snprintf _snprintf
#endif

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

using namespace vgui2;

namespace
{

// helpers in sorting strings
struct LessCtx_t
{
	const char*				m_pUserString;
	CLocalizedStringTable*	m_pTable;

	LessCtx_t() : m_pUserString( NULL ), m_pTable( NULL )
	{
	}
};

}

namespace {
struct ProgrammaticText {
    std::string name;
    std::wstring value;
    std::string filename;
    bool fillMissing;
};
struct LocalizationState {
    std::vector<std::string> requests;
    std::map<std::wstring, int> values;
    std::map<std::string, ProgrammaticText> programmatic;
    std::set<vgui2::StringIndex_t> active;
    bool fileParsing = false;
    bool committing = false;
};
std::map<const CLocalizedStringTable*, LocalizationState>& LocalizationStates() {
    static std::map<const CLocalizedStringTable*, LocalizationState> states;
    return states;
}
LocalizationState& State(const CLocalizedStringTable* table) {
    return LocalizationStates()[table];
}

}

CLocalizedStringTable* CLocalizedStringTable::s_pTable = NULL;

// global instance of table
CLocalizedStringTable g_StringTable;
// expose the interface
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CLocalizedStringTable, ILocalize, "VGUI_Localize003", g_StringTable );

CLocalizedStringTable_VGUI_Localize002 g_StringTable_Localize002;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CLocalizedStringTable_VGUI_Localize002,
	CLocalizedStringTable_VGUI_Localize002, "VGUI_Localize002", g_StringTable_Localize002 );

static LessCtx_t g_LessCtx;

char* Q_strncat( char* pDest, const char* pSrc, size_t destBufferSize, int max_chars_to_copy )
{
	size_t destLen;
	int srcLen;
	size_t n;

	destLen = strlen( pDest );
	srcLen = (int)strlen( pSrc );
	n = (size_t)srcLen;

	if ( max_chars_to_copy >= 0 && srcLen > max_chars_to_copy )
	{
		n = (size_t)max_chars_to_copy;
	}

	if ( n + destLen >= destBufferSize )
	{
		n = destBufferSize - 1 - destLen;
	}

	if ( n != 0 )
	{
		strncat( pDest, pSrc, n );
		pDest[destBufferSize - 1] = 0;
	}

	return pDest;
}

int Q_UCS2ToUTF8( const ucs2* pUCS2, int cubSrcInBytes, char* pUTF8, int cubDestSizeInBytes )
{
#ifdef _WIN32
	int result;

	UNREFERENCED_PARAMETER( cubSrcInBytes );

	*pUTF8 = 0;
	result = WideCharToMultiByte( CP_UTF8, 0, (const wchar_t*)pUCS2, -1, pUTF8, cubDestSizeInBytes, NULL, NULL );
	pUTF8[cubDestSizeInBytes - 1] = 0;
	return result;
#else
    (void)cubSrcInBytes;
    return cse::legacy_text::WideCellsToUtf8(pUCS2, pUTF8, cubDestSizeInBytes, true);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Used to sort strings
//-----------------------------------------------------------------------------
bool CLocalizedStringTable::SymLess( const localizedstring_t& i1, const localizedstring_t& i2 )
{
	const char* s1;
	const char* s2;

	if ( i1.nameIndex == INVALID_LOCALIZE_STRING_INDEX )
	{
		s1 = g_LessCtx.m_pUserString;
	}
	else
	{
		s1 = &g_LessCtx.m_pTable->m_Names[i1.nameIndex];
	}

	if ( i2.nameIndex == INVALID_LOCALIZE_STRING_INDEX )
	{
		s2 = g_LessCtx.m_pUserString;
	}
	else
	{
		s2 = &g_LessCtx.m_pTable->m_Names[i2.nameIndex];
	}

	return strcasecmp( s1, s2 ) < 0;
}

bool CLocalizedStringTable::FastValueLessFunc( const fastvalue_t& lhs, const fastvalue_t& rhs )
{
	const wchar_t* s1;
	const wchar_t* s2;

#ifdef _WIN32
	if ( !s_pTable )
	{
		_wassert( L"s_pTable", LOCALIZEDSTRINGTABLE_SOURCE_PATH, 907 );
	}
#endif

	s1 = lhs.search ? lhs.search : &s_pTable->m_Values[lhs.valueindex];
	s2 = rhs.search ? rhs.search : &s_pTable->m_Values[rhs.valueindex];
	return wcscmp( s1, s2 ) < 0;
}

CLocalizedStringTable::CLocalizedStringTable() :
	m_Lookup( 0, 0, SymLess ),
	m_Names( 1024 ),
	m_Values( 2048 ),
	m_FastValueLookup( 0, 0, FastValueLessFunc )
{
    State(this);
	memset( m_szLanguage, 0, sizeof( m_szLanguage ) );
	m_bUseOnlyLongestLanguageString = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLocalizedStringTable::~CLocalizedStringTable()
{
    LocalizationStates().erase(this);
	m_Names.Purge();
	m_Values.Purge();
	m_LocalizationFiles.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Finds a string in the table
//-----------------------------------------------------------------------------
wchar_t* CLocalizedStringTable::Find(const char* name) {
    return GetValueByIndex(FindIndex(name));
}

const char* CLocalizedStringTable::GetNameByIndex(StringIndex_t index) {
    if (index == INVALID_LOCALIZE_STRING_INDEX || !m_Lookup.IsValidIndex(index)) {
        return nullptr;
    }
    return &m_Names[m_Lookup[index].nameIndex];
}

wchar_t* CLocalizedStringTable::GetValueByIndex(StringIndex_t index) {
    if (index == INVALID_LOCALIZE_STRING_INDEX || !m_Lookup.IsValidIndex(index)
        || State(this).active.count(index) == 0) {
        return nullptr;
    }
    return &m_Values[m_Lookup[index].valueIndex];
}

//-----------------------------------------------------------------------------
// Purpose: returns number of localization files currently loaded
//-----------------------------------------------------------------------------
int CLocalizedStringTable::GetLocalizationFileCount()
{
	return m_LocalizationFiles.Count();
}

void CLocalizedStringTable::ReloadLocalizationFiles(IFileSystem* filesystem) {
    const auto requests = State(this).requests;
    for (const auto& request : requests) {
        AddFile(filesystem, request.c_str());
    }
}

//-----------------------------------------------------------------------------
// Purpose: returns localization filename by index
//-----------------------------------------------------------------------------
const char* CLocalizedStringTable::GetLocalizationFileName( int index )
{
    return index >= 0 && index < m_LocalizationFiles.Count() ? m_LocalizationFiles[index].String() : nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: returns which file a string was loaded from
//-----------------------------------------------------------------------------
const char* CLocalizedStringTable::GetFileNameByIndex( StringIndex_t index )
{
    return index != INVALID_LOCALIZE_STRING_INDEX && m_Lookup.IsValidIndex(index)
        ? m_Lookup[index].filename.String() : nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: sets the value in the index
//-----------------------------------------------------------------------------
void CLocalizedStringTable::SetValueByIndex(StringIndex_t index, wchar_t* value) {
    const char* name = GetNameByIndex(index);
    if (name && value) {
        AddString(name, value, GetFileNameByIndex(index));
    }
}

void CLocalizedStringTable::ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
    const char* tokenName, KeyValues* localizationVariables ) {
    if ( !unicodeOutput || unicodeBufferSizeInBytes < static_cast<int>( sizeof( wchar_t ) ) ) {
        return;
    }

    StringIndex_t index;

    index = FindIndex( tokenName );
    if ( index == INVALID_LOCALIZE_STRING_INDEX ) {
        ConvertANSIToUnicode( tokenName, unicodeOutput, unicodeBufferSizeInBytes );
    }
    else {
        ConstructString( unicodeOutput, unicodeBufferSizeInBytes, index, localizationVariables );
    }
}

void CLocalizedStringTable::ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
    StringIndex_t unlocalizedTextSymbol, KeyValues* localizationVariables ) {
    const wchar_t* searchPos;
    wchar_t* outputPos;
    int remaining;
    wchar_t ch;

    if ( !unicodeOutput || unicodeBufferSizeInBytes < static_cast<int>( sizeof( wchar_t ) ) ) {
        return;
    }

    constexpr int terminatorCharacters = 1;
    const int bufferCharacters = unicodeBufferSizeInBytes / static_cast<int>( sizeof( wchar_t ) );
    unicodeOutput[0] = 0;
    searchPos = GetValueByIndex( unlocalizedTextSymbol );
    if ( !searchPos ) {
        wcsncpy( unicodeOutput, L"[unknown string]", bufferCharacters - terminatorCharacters );
        unicodeOutput[bufferCharacters - terminatorCharacters] = 0;
        return;
    }

    // All copies count content characters; keep the terminator inside the buffer.
    remaining = bufferCharacters - terminatorCharacters;
    outputPos = unicodeOutput;

    while ( 1 ) {
        ch = *searchPos;
        if ( remaining <= 0 || ch == 0 ) {
            *outputPos = 0;
            return;
        }

        if ( ch == L'%' ) {
            for ( ;; ) {
                wchar_t next = searchPos[1];

                if ( next == L's' && (unsigned int)( searchPos[2] - L'0' ) <= 9 ) {
                    ++searchPos;
                    break;
                }

                if ( next == L'%' ) {
                    searchPos += 2;
                    ch = L'%';
                    break;
                }

                if ( !localizationVariables ) {
                    ++searchPos;
                    break;
                }

                ++searchPos;
                {
                    const wchar_t* tokenEnd = wcschr( searchPos, L'%' );
                    char variableName[32];
                    char* nameWrite;
                    const wchar_t* replacement;
                    int copyCount;

                    if ( !tokenEnd || *tokenEnd != L'%' ) {
                        break;
                    }

                    nameWrite = variableName;
                    while ( searchPos < tokenEnd && nameWrite < variableName + 31 ) {
                        *nameWrite++ = (char)*searchPos++;
                    }

                    *nameWrite = 0;

                    replacement = localizationVariables->GetWString( variableName, L"[unknown]" );
                    copyCount = (int)wcslen( replacement );
                    if ( copyCount > remaining ) {
                        copyCount = remaining;
                    }

                    remaining -= copyCount;
                    wcsncpy( outputPos, replacement, copyCount );
                    outputPos += copyCount;

                    searchPos = tokenEnd + 1;
                    ch = *searchPos;
                    if ( remaining <= 0 || ch == 0 ) {
                        *outputPos = 0;
                        return;
                    }

                    if ( ch != L'%' ) {
                        searchPos = tokenEnd + 2;
                        break;
                    }
                }
            }
        }
        else {
            ++searchPos;
        }

        --remaining;
        *outputPos++ = ch;
    }
}

//-----------------------------------------------------------------------------
// Purpose: iteration functions
//-----------------------------------------------------------------------------
StringIndex_t CLocalizedStringTable::GetFirstStringIndex()
{
    StringIndex_t index = m_Lookup.FirstInorder();
    while (index != INVALID_LOCALIZE_STRING_INDEX && State(this).active.count(index) == 0) {
        index = m_Lookup.NextInorder(index);
    }
    return index;
}

//-----------------------------------------------------------------------------
// Remove all symbols in the table.
//-----------------------------------------------------------------------------
void CLocalizedStringTable::RemoveAll()
{
    State(this) = LocalizationState{};
	m_Lookup.RemoveAll();
	m_Names.RemoveAll();
	m_Values.RemoveAll();
	m_LocalizationFiles.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: finds the index of a token by token name
//-----------------------------------------------------------------------------
StringIndex_t CLocalizedStringTable::FindIndex( const char* pName )
{
	localizedstring_t invalidItem;

	if ( !pName )
	{
        return INVALID_LOCALIZE_STRING_INDEX;
	}

	// strip the pound character (which is used elsewhere to indicate that it's a string that should be translated)
	if ( pName[0] == '#' )
	{
		pName++;
	}

	g_LessCtx.m_pTable = this;
	g_LessCtx.m_pUserString = pName;

	invalidItem.nameIndex = INVALID_LOCALIZE_STRING_INDEX;
	invalidItem.valueIndex = INVALID_LOCALIZE_STRING_INDEX;
	invalidItem.filename = CUtlSymbol();

    const StringIndex_t index = m_Lookup.Find(invalidItem);
    return State(this).active.count(index) != 0 ? index : INVALID_LOCALIZE_STRING_INDEX;
}

StringIndex_t CLocalizedStringTable::GetNextStringIndex( StringIndex_t index )
{
    if (index == INVALID_LOCALIZE_STRING_INDEX || !m_Lookup.IsValidIndex(index)) {
        return INVALID_LOCALIZE_STRING_INDEX;
    }
    index = m_Lookup.NextInorder(index);
    while (index != INVALID_LOCALIZE_STRING_INDEX && State(this).active.count(index) == 0) {
        index = m_Lookup.NextInorder(index);
    }
    return index;
}

int CLocalizedStringTable::ConvertANSIToUnicode( const char* ansi, wchar_t* unicode,
	int unicodeBufferSizeInBytes )
{
#ifdef _WIN32
	int result;

	result = MultiByteToWideChar( CP_UTF8, 0, ansi, -1, unicode, unicodeBufferSizeInBytes / sizeof( wchar_t ) );
	unicode[unicodeBufferSizeInBytes / sizeof( wchar_t ) - 1] = 0;
	return result;
#else
    return cse::legacy_text::Utf8ToWideCells(ansi, unicode, unicodeBufferSizeInBytes);
#endif
}

int CLocalizedStringTable::ConvertUnicodeToANSI( const wchar_t* unicode, char* ansi, int ansiBufferSize )
{
#ifdef _WIN32
	int result;

	result = WideCharToMultiByte( CP_UTF8, 0, unicode, -1, ansi, ansiBufferSize, NULL, NULL );
	ansi[ansiBufferSize - 1] = 0;
	return result;
#else
    return cse::legacy_text::WideCellsToUtf8(unicode, ansi, ansiBufferSize);
#endif
}

static const char* s_LanguageToken = "%language%";
static const char* s_English = "english";

static void ConstructStringFromArgs( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
    wchar_t* formatString, int numFormatParameters, va_list argList ) {
    int unicodeBufferSize;
    wchar_t* searchPos;
    wchar_t* outputPos;
    int formatLength;

    if ( !unicodeOutput || unicodeBufferSizeInBytes < static_cast<int>( sizeof( wchar_t ) ) ) {
        return;
    }
    if ( !formatString ) {
        unicodeOutput[0] = 0;
        return;
    }

    unicodeBufferSize = unicodeBufferSizeInBytes / static_cast<int>( sizeof( wchar_t ) );
    searchPos = formatString;
    outputPos = unicodeOutput;
    formatLength = (int)wcslen( formatString );

    while ( unicodeBufferSize > 1 ) {
        wchar_t ch = *searchPos;

        if ( ch == 0 ) {
            break;
        }

        if ( ch == L'%' && formatLength > 2 && searchPos[1] == L's' ) {
            int argindex = (int)searchPos[2] - (int)L'1';

            if ( (unsigned int)argindex <= 8 ) {
                if ( argindex >= numFormatParameters ) {
                    *outputPos = L'%';
                    --unicodeBufferSize;
                    ++outputPos;
                    ++searchPos;
                    --formatLength;
                }
                else {
                    wchar_t* param = ( (wchar_t**)argList )[argindex];
                    int paramSize;

                    if ( !param ) {
#ifdef _WIN32
                        _wassert( L"!(\"CLocalizedStringTable::ConstructString - Found a %s# escape sequence "
                            L"who's index was more than the number of args.\")",
                            LOCALIZEDSTRINGTABLE_SOURCE_PATH, 1095 );
#else
                        *outputPos = 0;
                        return;
#endif
                    }

                    paramSize = (int)wcslen( param );
                    if ( unicodeBufferSize <= paramSize ) {
                        paramSize = unicodeBufferSize - 1;
                    }

                    searchPos += 3;
                    formatLength -= 3;
                    wcsncpy( outputPos, param, paramSize );
                    unicodeBufferSize -= paramSize;
                    outputPos += paramSize;
                }

                continue;
            }
        }

        *outputPos = ch;
        --unicodeBufferSize;
        ++outputPos;
        ++searchPos;
        --formatLength;
    }

    *outputPos = 0;
}

bool CLocalizedStringTable::AddAllLanguageFiles( IFileSystem* fileSystem, const char* baseFileName )
{
	char szFilePath[MAX_PATH];
	char szSearchPath[MAX_PATH];
	char szFile[MAX_PATH];
	char* lastSlash;
	FileFindHandle_t hFind;
	const char* file;
	bool success;

	strncpy( szFilePath, baseFileName, sizeof( szFilePath ) );
	lastSlash = strrchr( szFilePath, '\\' );
	if ( !lastSlash )
	{
		lastSlash = strrchr( szFilePath, '/' );
	}

	if ( lastSlash )
	{
		lastSlash[1] = 0;
	}
	else
	{
		szFilePath[0] = 0;
	}

	snprintf( szSearchPath, sizeof( szSearchPath ), "%s*.txt", baseFileName );

	hFind = 0;
	file = fileSystem->FindFirst( szSearchPath, &hFind, NULL );
	success = true;

	while ( file )
	{
		bool added;

		snprintf( szFile, sizeof( szFile ), "%s%s", szFilePath, file );
		added = AddFile( fileSystem, szFile );
		success = success & added;
		file = fileSystem->FindNext( hFind );
	}

	fileSystem->FindClose( hFind );
	return success;
}

#undef snprintf

void CLocalizedStringTable::DiscardFastValueLookup()
{
	m_FastValueLookup.RemoveAll();
	s_pTable = NULL;
}

int CLocalizedStringTable::FindExistingValueIndex( const wchar_t* value )
{
	fastvalue_t val;
	int idx;

	if ( !s_pTable )
	{
		return -1;
	}

	val.valueindex = -1;
	val.search = value;
	idx = m_FastValueLookup.Find( val );
	if ( idx != -1 )
	{
		return m_FastValueLookup[idx].valueindex;
	}

	return -1;
}

void CLocalizedStringTable::ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
	wchar_t* formatString, int numFormatParameters, va_list argList )
{
	ConstructStringFromArgs( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
}

void CLocalizedStringTable::ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,
    wchar_t* formatString, int numFormatParameters, ... ) {
    va_list argList;

    va_start( argList, numFormatParameters );

    ConstructString( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );

    va_end( argList );
}

//-----------------------------------------------------------------------------
// Purpose: saves the entire contents of the token tree to the file
//-----------------------------------------------------------------------------
namespace {

// Localization resources are UTF-16LE on disk, independently of the host wchar_t width.
bool WriteLocalizationUTF16(IFileSystem* fileSystem, FileHandle_t file, const wchar_t* text,
    bool writeTerminator = false) {
    constexpr size_t WriteBufferBytes = 1024;
    unsigned char bytes[WriteBufferBytes];
    size_t used = 0;
    const auto flush = [&]() {
        const int count = static_cast<int>(used); // Bounded by WriteBufferBytes, never by the string length.
        const bool success = count == 0 || fileSystem->Write(bytes, count, file) == count;
        used = 0;
        return success;
    };
    const auto writeUnit = [&](unsigned int unit) {
        if (used == sizeof(bytes) && !flush()) {
            return false;
        }
        constexpr unsigned int ByteMask = 0xFF;
        constexpr unsigned int BitsPerByte = 8;
        bytes[used++] = static_cast<unsigned char>(unit & ByteMask);
        bytes[used++] = static_cast<unsigned char>((unit >> BitsPerByte) & ByteMask);
        return true;
    };
    constexpr unsigned int MaximumUTF16Unit = 0xFFFF;
    constexpr unsigned int MaximumUnicodeScalar = 0x10FFFF;
    constexpr unsigned int SupplementaryBase = 0x10000;
    constexpr unsigned int SurrogatePayloadBits = 10;
    constexpr unsigned int SurrogatePayloadMask = 0x3FF;
    constexpr unsigned int HighSurrogateBase = 0xD800;
    constexpr unsigned int LowSurrogateBase = 0xDC00;
    while (*text) {
        const unsigned int value = static_cast<unsigned int>(*text++);
        if (value <= MaximumUTF16Unit) {
            // Keep existing UTF-16 surrogate units verbatim, including the Windows wchar_t representation.
            if (!writeUnit(value)) {
                return false;
            }
        } else if (value <= MaximumUnicodeScalar) {
            const unsigned int payload = value - SupplementaryBase;
            if (!writeUnit(HighSurrogateBase + (payload >> SurrogatePayloadBits))
                || !writeUnit(LowSurrogateBase + (payload & SurrogatePayloadMask))) {
                return false;
            }
        } else {
            return false;
        }
    }
    if (writeTerminator && !writeUnit(0)) {
        return false;
    }
    return flush();
}

}

bool CLocalizedStringTable::SaveToFile(IFileSystem* fileSystem, const char* szFileName) {
    FileHandle_t file = fileSystem->Open(szFileName, "wb", NULL);
    if (!file) {
        return false;
    }
    const CUtlSymbol fileSymbol(szFileName);
    const unsigned char marker[] = {0xFF, 0xFE};
    bool success = fileSystem->Write(marker, sizeof(marker), file) == sizeof(marker)
        && WriteLocalizationUTF16(fileSystem, file,
            L"\"lang\"\r\n{\r\n\"Language\" \"English\"\r\n\"Tokens\"\r\n{\r\n");
    for (StringIndex_t idx = GetFirstStringIndex(); success && idx != INVALID_LOCALIZE_STRING_INDEX;
        idx = GetNextStringIndex(idx)) {
        if (!(fileSymbol == m_Lookup[idx].filename)) {
            continue;
        }
        wchar_t unicodeName[1024];
        const char* name = GetNameByIndex(idx);
        const int result = ConvertANSIToUnicode(name, unicodeName, sizeof(unicodeName));
        if (result < 0 || (result == 0 && *name)) {
            success = false;
            break;
        }
        success = WriteLocalizationUTF16(fileSystem, file, L"\t\"")
            && WriteLocalizationUTF16(fileSystem, file, unicodeName)
            && WriteLocalizationUTF16(fileSystem, file, L"\"\t\t\"")
            && WriteLocalizationUTF16(fileSystem, file, GetValueByIndex(idx))
            && WriteLocalizationUTF16(fileSystem, file, L"\"\r\n");
    }
    // The legacy Windows envelope includes the terminating UTF-16 zero after the final braces.
    success = success && WriteLocalizationUTF16(fileSystem, file, L"}\r\n}\r\n", true);
    fileSystem->Close(file);
    return success;
}

void CLocalizedStringTable::AddString(const char* name, wchar_t* value, const char* filename) {
    if (!name || !value) {
        return;
    }
    if (name[0] == '#') {
        ++name;
    }
    auto& state = State(this);
    localizedstring_t key{};
    key.nameIndex = INVALID_LOCALIZE_STRING_INDEX;
    g_LessCtx.m_pTable = this;
    g_LessCtx.m_pUserString = name;
    const StringIndex_t found = m_Lookup.Find(key);
    if (!state.fileParsing && !state.committing) {
        std::string canonical(name);
        for (char& cell : canonical) {
            if (cell >= 'A' && cell <= 'Z') {
                constexpr char CaseOffset = 'a' - 'A';
                cell += CaseOffset;
            }
        }
        // This owner registers emergency English only when the token is absent.
        // Other programmatic AddString/SetValue calls are explicit overrides.
        const bool fillMissing = filename && std::strcmp(filename, "CSEServerBrowser") == 0;
        state.programmatic[canonical] = ProgrammaticText{name, value, filename ? filename : "", fillMissing};
    }
    int valueIndex = 0;
    const auto existing = state.values.find(value);
    if (existing != state.values.end()) {
        valueIndex = existing->second;
    } else {
        const std::size_t length = std::wcslen(value) + 1;
        if (length > static_cast<std::size_t>(INT_MAX)) {
            return;
        }
        valueIndex = m_Values.AddMultipleToTail(static_cast<int>(length));
        std::memcpy(&m_Values[valueIndex], value, length * sizeof(wchar_t));
        state.values.emplace(value, valueIndex);
    }
    if (found == INVALID_LOCALIZE_STRING_INDEX) {
        const std::size_t length = std::strlen(name) + 1;
        if (length > static_cast<std::size_t>(INT_MAX)) {
            return;
        }
        localizedstring_t item{};
        item.nameIndex = m_Names.AddMultipleToTail(static_cast<int>(length));
        std::memcpy(&m_Names[item.nameIndex], name, length);
        item.valueIndex = valueIndex;
        item.filename = filename ? CUtlSymbol(filename) : m_CurrentFile;
        state.active.insert(m_Lookup.Insert(item));
        return;
    }
    if (m_bUseOnlyLongestLanguageString && !state.committing) {
        wchar_t* oldValue = GetValueByIndex(found);
        if (oldValue) {
            int newWidth = 0;
            int oldWidth = 0;
            int height = 0;
            g_pSurface->GetTextSize(1, value, newWidth, height);
            g_pSurface->GetTextSize(1, oldValue, oldWidth, height);
            if (newWidth < oldWidth) {
                return;
            }
        }
    }
    m_Lookup[found].valueIndex = valueIndex;
    m_Lookup[found].filename = filename ? CUtlSymbol(filename) : m_CurrentFile;
    state.active.insert(found);
}

void CLocalizedStringTable::CommitLanguageSnapshot(CLocalizedStringTable& prepared) {
    auto& state = State(this);
    state.active.clear();
    state.committing = true;
    for (StringIndex_t index = prepared.GetFirstStringIndex(); index != INVALID_LOCALIZE_STRING_INDEX;
         index = prepared.GetNextStringIndex(index)) {
        AddString(prepared.GetNameByIndex(index), prepared.GetValueByIndex(index), prepared.GetFileNameByIndex(index));
    }
    state.committing = false;
    m_LocalizationFiles.RemoveAll();
    for (int index = 0; index < prepared.GetLocalizationFileCount(); ++index) {
        m_LocalizationFiles.AddToTail(CUtlSymbol(prepared.GetLocalizationFileName(index)));
    }
    DiscardFastValueLookup();
}

void CLocalizedStringTable::BuildFastValueLookup()
{
	int c;
	int i;

	m_FastValueLookup.RemoveAll();
	s_pTable = this;

	c = m_Lookup.Count();
	for ( i = 0; i < c; ++i )
	{
		fastvalue_t val;

		val.valueindex = m_Lookup[i].valueIndex;
		val.search = NULL;
		m_FastValueLookup.Insert( val );
	}
}

namespace {
enum class CatalogueLoad { Missing, Invalid, Ready };

CatalogueLoad ReadCatalogue(IFileSystem* filesystem, const std::string& path,
    cse::localization::Catalogue& catalogue) {
    FileHandle_t file = filesystem->Open(path.c_str(), "rb", nullptr);
    if (!file) {
        return CatalogueLoad::Missing;
    }
    constexpr unsigned int MaximumCatalogueBytes = 32 * 1024 * 1024;
    const unsigned int size = filesystem->Size(file);
    if (size < 2 || size > MaximumCatalogueBytes) {
        filesystem->Close(file);
        return CatalogueLoad::Invalid;
    }
    std::vector<unsigned char> bytes(size);
    const int read = filesystem->Read(bytes.data(), static_cast<int>(size), file);
    filesystem->Close(file);
    cse::localization::CatalogueParser parser;
    return read == static_cast<int>(size) && parser.Parse(bytes, catalogue)
        ? CatalogueLoad::Ready : CatalogueLoad::Invalid;
}

std::string CataloguePath(const std::string& request, const char* language) {
    std::string path = request;
    const auto marker = path.find(s_LanguageToken);
    if (marker != std::string::npos) {
        path.replace(marker, std::strlen(s_LanguageToken), language);
    }
    return path;
}
} // namespace

bool CLocalizedStringTable::AddFile(IFileSystem* filesystem, const char* filename) {
    if (!filesystem || !filename || !filename[0]) {
        return false;
    }
    const std::string request(filename);
    const bool logical = request.find(s_LanguageToken) != std::string::npos;
    if (logical && g_pSystem && g_pSystem->CommandLineParamExists("-all_languages")) {
        m_bUseOnlyLongestLanguageString = true;
        return AddAllLanguageFiles(filesystem, request.substr(0, request.find(s_LanguageToken)).c_str());
    }
    using Loaded = std::pair<std::string, cse::localization::Catalogue>;
    std::vector<Loaded> loaded;
    const std::string baseline = logical ? CataloguePath(request, s_English) : request;
    cse::localization::Catalogue catalogue;
    if (ReadCatalogue(filesystem, baseline, catalogue) != CatalogueLoad::Ready) {
        return false;
    }
    loaded.emplace_back(baseline, std::move(catalogue));
    if (logical) {
        const char* selected = g_pSurface ? g_pSurface->GetLanguage() : nullptr;
        if (selected && selected[0] && std::strcmp(selected, s_English) != 0) {
            const std::string overlay = CataloguePath(request, selected);
            const CatalogueLoad status = ReadCatalogue(filesystem, overlay, catalogue);
            if (status == CatalogueLoad::Invalid) {
                return false;
            }
            if (status == CatalogueLoad::Ready) {
                loaded.emplace_back(overlay, std::move(catalogue));
            }
        }
    }
    auto& state = State(this);
    const bool previousParsing = state.fileParsing;
    state.fileParsing = true;
    for (auto& file : loaded) {
        m_CurrentFile = CUtlSymbol(file.first.c_str());
        const bool english = file.first.find("_english.txt") != std::string::npos;
        for (auto& token : file.second.tokens) {
            if (english || strncasecmp(token.first.c_str(), "[english]", 9) != 0) {
                AddString(token.first.c_str(), const_cast<wchar_t*>(token.second.c_str()), file.first.c_str());
            }
        }
        std::snprintf(m_szLanguage, sizeof(m_szLanguage), "%s", file.second.language.c_str());
        bool registered = false;
        for (int index = 0; index < m_LocalizationFiles.Count(); ++index) {
            registered = registered || m_LocalizationFiles[index] == m_CurrentFile;
        }
        if (!registered) {
            m_LocalizationFiles.AddToTail(m_CurrentFile);
        }
    }
    state.fileParsing = previousParsing;
    m_CurrentFile = UTL_INVAL_SYMBOL;
    if (std::find(state.requests.begin(), state.requests.end(), request) == state.requests.end()) {
        state.requests.push_back(request);
    }
    return true;
}

CLocalizedStringTable_VGUI_Localize002::~CLocalizedStringTable_VGUI_Localize002()
{
}

bool CLocalizedStringTable_VGUI_Localize002::AddFile( IFileSystem* fileSystem, const char* fileName )
{
	return g_StringTable.AddFile( fileSystem, fileName );
}

void CLocalizedStringTable_VGUI_Localize002::RemoveAll()
{
	g_StringTable.RemoveAll();
}

wchar_t* CLocalizedStringTable_VGUI_Localize002::Find( const char* tokenName )
{
	return g_StringTable.Find( tokenName );
}

int CLocalizedStringTable_VGUI_Localize002::ConvertANSIToUnicode( const char* ansi,
	wchar_t* unicode, int unicodeBufferSizeInBytes )
{
	return g_StringTable.ConvertANSIToUnicode( ansi, unicode, unicodeBufferSizeInBytes );
}

int CLocalizedStringTable_VGUI_Localize002::ConvertUnicodeToANSI( const wchar_t* unicode,
	char* ansi, int ansiBufferSize )
{
	return g_StringTable.ConvertUnicodeToANSI( unicode, ansi, ansiBufferSize );
}

OldStringIndex_t CLocalizedStringTable_VGUI_Localize002::FindIndex( const char* tokenName )
{
	return (OldStringIndex_t)g_StringTable.FindIndex( tokenName );
}

void CLocalizedStringTable_VGUI_Localize002::ConstructString( wchar_t* unicodeOutput,
	int unicodeBufferSizeInBytes, wchar_t* formatString, int numFormatParameters, ... )
{
	va_list argList;

	va_start( argList, numFormatParameters );
	ConstructStringFromArgs( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
	va_end( argList );
}

const char* CLocalizedStringTable_VGUI_Localize002::GetNameByIndex( OldStringIndex_t index )
{
	return g_StringTable.GetNameByIndex( index );
}

wchar_t* CLocalizedStringTable_VGUI_Localize002::GetValueByIndex( OldStringIndex_t index )
{
	return g_StringTable.GetValueByIndex( index );
}

OldStringIndex_t CLocalizedStringTable_VGUI_Localize002::GetFirstStringIndex()
{
	return (OldStringIndex_t)g_StringTable.GetFirstStringIndex();
}

OldStringIndex_t CLocalizedStringTable_VGUI_Localize002::GetNextStringIndex( OldStringIndex_t index )
{
	return (OldStringIndex_t)g_StringTable.GetNextStringIndex( index );
}

void CLocalizedStringTable_VGUI_Localize002::AddString( const char* tokenName,
	wchar_t* unicodeString, const char* fileName )
{
	g_StringTable.AddString( tokenName, unicodeString, fileName );
}

void CLocalizedStringTable_VGUI_Localize002::SetValueByIndex( OldStringIndex_t index, wchar_t* newValue )
{
	g_StringTable.SetValueByIndex( index, newValue );
}

bool CLocalizedStringTable_VGUI_Localize002::SaveToFile( IFileSystem* fileSystem, const char* fileName )
{
	return g_StringTable.SaveToFile( fileSystem, fileName );
}

int CLocalizedStringTable_VGUI_Localize002::GetLocalizationFileCount()
{
	return g_StringTable.GetLocalizationFileCount();
}

const char* CLocalizedStringTable_VGUI_Localize002::GetLocalizationFileName( int index )
{
	return g_StringTable.GetLocalizationFileName( index );
}

const char* CLocalizedStringTable_VGUI_Localize002::GetFileNameByIndex( OldStringIndex_t index )
{
	return g_StringTable.GetFileNameByIndex( index );
}

namespace {
struct PreparedLocalization {
    std::unique_ptr<CLocalizedStringTable> table;
    std::string language;
};
PreparedLocalization& Prepared() {
    static PreparedLocalization prepared;
    return prepared;
}
std::uint64_t runtimeRevision = 0;
cse::localization::FontResetCallback fontReset = nullptr;
const cse::localization::FontBackend* boundFontBackend = nullptr;
const cse::localization::FontBackend* SelectedFontBackend() {
    return boundFontBackend ? boundFontBackend : cse::localization::LocalFontBackend();
}

bool SafeLanguage(const char* language) {
    if (!language || !language[0]) {
        return false;
    }
    constexpr std::size_t MaximumLanguageBytes = 31;
    for (std::size_t index = 0; index <= MaximumLanguageBytes; ++index) {
        const unsigned char cell = static_cast<unsigned char>(language[index]);
        if (!cell) {
            return true;
        }
        if ((cell < 'a' || cell > 'z') && (cell < '0' || cell > '9') && cell != '_') {
            return false;
        }
    }
    return false;
}
std::string ExpandedPath(const std::string& request, const char* language) {
    std::string result = request;
    constexpr char Marker[] = "%language%";
    const auto position = result.find(Marker);
    if (position != std::string::npos) {
        result.replace(position, sizeof(Marker) - 1, language);
    }
    return result;
}
bool CSE_ABI_CALL PrepareLanguage(IFileSystem* filesystem, const char* language) {
    Prepared() = PreparedLocalization{};
    const auto* fonts = SelectedFontBackend();
    fonts->Discard();
    if (!filesystem || !SafeLanguage(language)) {
        return false;
    }
    auto candidate = std::make_unique<CLocalizedStringTable>();
    // Logical requests are copied because a module may add another catalogue later.
    const auto requests = State(&g_StringTable).requests;
    for (const auto& request : requests) {
        const bool localized = request.find("%language%") != std::string::npos;
        const std::string baseline = localized ? ExpandedPath(request, "english") : request;
        if (!candidate->AddFile(filesystem, baseline.c_str())) {
            return false;
        }
        if (localized && std::strcmp(language, "english") != 0) {
            const std::string overlay = ExpandedPath(request, language);
            if (filesystem->FileExists(overlay.c_str())) {
                if (!candidate->AddFile(filesystem, overlay.c_str())) {
                    return false;
                }
            }
        }
    }
    for (const auto& pair : State(&g_StringTable).programmatic) {
        if (!pair.second.fillMissing || !candidate->Find(pair.second.name.c_str())) {
            candidate->AddString(pair.second.name.c_str(), const_cast<wchar_t*>(pair.second.value.c_str()),
                pair.second.filename.c_str());
        }
    }
    if (!fonts->Begin(language)) {
        fonts->Discard();
        return false;
    }
    const bool fontsReady = ::g_Scheme.PrepareLanguageFonts(language);
    fonts->Finish();
    if (!fontsReady) {
        fonts->Discard();
        return false;
    }
    Prepared().table = std::move(candidate);
    Prepared().language = language;
    return true;
}
void CSE_ABI_CALL CommitLanguage(std::uint64_t revision) {
    auto& prepared = Prepared();
    if (!prepared.table) {
        return;
    }
    g_StringTable.CommitLanguageSnapshot(*prepared.table);
    SelectedFontBackend()->Commit();
    runtimeRevision = revision;
    prepared = PreparedLocalization{};
}
void CSE_ABI_CALL DiscardPrepared() {
    Prepared() = PreparedLocalization{};
    SelectedFontBackend()->Discard();
}
std::uint64_t CSE_ABI_CALL RuntimeRevision() {
    return runtimeRevision;
}
void CSE_ABI_CALL BindFontReset(cse::localization::FontResetCallback callback) {
    fontReset = callback;
}
const cse::localization::FontBackend* CSE_ABI_CALL BindFontBackend(
    const cse::localization::FontBackend* backend) {
    if (backend && (backend->size < sizeof(cse::localization::FontBackend)
        || backend->version != cse::localization::InterfaceVersion || !backend->Begin || !backend->Stage
        || !backend->Finish || !backend->Commit || !backend->Discard)) {
        return nullptr;
    }
    DiscardPrepared();
    boundFontBackend = backend;
    return SelectedFontBackend();
}
void* RuntimeFactory() {
    static const cse::localization::RuntimeApi api{sizeof(cse::localization::RuntimeApi),
        cse::localization::InterfaceVersion, PrepareLanguage, CommitLanguage, DiscardPrepared,
        RuntimeRevision, BindFontReset, BindFontBackend};
    return const_cast<cse::localization::RuntimeApi*>(&api);
}
}

bool StageLocalizationFont(std::uint32_t font, const char* name, int tall, int weight,
    int blur, int scanlines, int flags) {
    return SelectedFontBackend()->Stage(font, name, tall, weight, blur, scanlines, flags);
}
void ResetLocalizationFont(std::uint32_t font) {
    if (fontReset) {
        fontReset(font);
    } else {
        FontManager().ResetFont(font);
    }
}
EXPOSE_INTERFACE_FN(RuntimeFactory, CSEVGuiLocalization, cse::localization::RuntimeInterface);
