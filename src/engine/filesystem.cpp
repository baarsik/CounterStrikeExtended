#include "filesystem.h"
#include "localization.h"
#include "../public/localized_filesystem.h"
#include <vector>
#include "../common/interface_language.h"
#include "../common/interface_language_preference.h"
#include "conprint.h"
#include "../public/native_filesystem_module.h"
#include "registry.h"
#include "common.h"
#include "host.h"
#include "cmd.h"
#include "server.h"
#include "steam_api.h"
#include "utlvector.h"
#include "../external/steamworks/isteamapps.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\filesystem.cpp" )

#define APPID_HALF_LIFE 70

static char s_pBaseDir[512];
static CSysModule* g_pFileSystemModule;
CreateInterfaceFn g_FileSystemFactory;
bool bLowViolenceBuild;
CUtlVector<char*> g_fallbackLocalizationFiles;

namespace {
struct LanguageMount {
    std::uint32_t group;
    std::string directory;
    std::string pathId;
    std::string anchor;
    bool writable;
};

const cse::localized_filesystem::Api* languageFilesystem = nullptr;
std::vector<LanguageMount> languageMounts;

std::string MountPath(const std::string& path) {
    std::string normalized = path;
    for (char& character : normalized) {
        if (character == '\\') {
            character = '/';
        }
    }
    return normalized;
}

void BeginLanguageMount(const char* directory, const char* pathId, bool writable, bool highDefinition,
    const char* anchorOverride = nullptr) {
    const auto group = static_cast<std::uint32_t>(languageMounts.size() + 1);
    const std::string anchor = anchorOverride ? anchorOverride
        : std::string(s_pBaseDir) + "/" + directory + (highDefinition ? "_hd" : "");
    languageMounts.push_back({group, directory, pathId, MountPath(anchor), writable});
    if (languageFilesystem) {
        languageFilesystem->BeginGroup(group);
    }
}

void EndLanguageMount() {
    if (languageFilesystem) {
        languageFilesystem->EndGroup();
    }
}
} // namespace

IFileSystem* FileSystem_PrepareInterfaceLanguage(const char* language) {
    if (!languageFilesystem || !language) {
        return nullptr;
    }
    std::vector<std::vector<std::string>> roots(languageMounts.size());
    std::vector<std::vector<const char*>> pointers(languageMounts.size());
    std::vector<cse::localized_filesystem::Replacement> replacements;
    const bool localized = std::strcmp(language, "english") != 0;
    for (std::size_t index = 0; index < languageMounts.size(); ++index) {
        const LanguageMount& mount = languageMounts[index];
        if (localized) {
            roots[index].push_back(MountPath(std::string(s_pBaseDir) + "/" + mount.directory + "_" + language));
            if (!COM_CheckParm("-steam")) {
                std::string external(s_pBaseDir);
                const auto game = external.find("\\game");
                if (game != std::string::npos) {
                    external.resize(game);
                    roots[index].push_back(MountPath(external + "/localization/" + mount.directory + "_" + language));
                }
            }
        }
        for (const std::string& root : roots[index]) {
            pointers[index].push_back(root.c_str());
        }
        replacements.push_back({mount.group, mount.anchor.c_str(), mount.pathId.c_str(), pointers[index].data(),
            pointers[index].size(), mount.writable});
    }
    return languageFilesystem->Prepare(g_pFileSystem, replacements.data(), replacements.size());
}

IFileSystem* FileSystem_InterfaceLanguageBaseline() {
    return languageFilesystem ? languageFilesystem->Baseline() : nullptr;
}

void FileSystem_CommitInterfaceLanguage() {
    if (languageFilesystem) {
        languageFilesystem->Commit();
    }
}

void FileSystem_DiscardInterfaceLanguage() {
    if (languageFilesystem) {
        languageFilesystem->Discard();
    }
}


bool BEnabledHDAddon()
{
	if ( COM_CheckParm( "-nohdmodels" ) )
		return false;

	return registry->ReadInt( "hdmodels", 0 ) > 0;
}

bool BEnableAddonsFolder()
{
	if ( COM_CheckParm( "-addons" ) )
		return true;

	return registry->ReadInt( "addons_folder", 0 ) > 0;
}

int FileSystem_Init( char* basedir, CreateInterfaceFn factory )
{
	Q_strncpy( s_pBaseDir, basedir, sizeof( s_pBaseDir ) - 1 );
	s_pBaseDir[sizeof( s_pBaseDir ) - 1] = '\0';
	host_parms.basedir = s_pBaseDir;

	if ( factory )
	{
        if (!cse::filesystem_abi::CompatibleFactory(factory)) {
            Con_Printf("Incompatible supplied filesystem factory.\n");
            return 0;
        }
		g_FileSystemFactory = factory;
	}
	else
	{
#if !defined( _WIN32 )
		g_pFileSystemModule = Sys_LoadModule( "filesystem_stdio.so" );
#else
		g_pFileSystemModule = Sys_LoadModule( "filesystem_stdio.dll" );
#endif
#if defined( _WIN32 )
		ASSERT_AT( g_pFileSystemModule, 95 );
#else
		if ( !g_pFileSystemModule )
			return 0;
#endif

        factory = cse::filesystem_abi::CheckedFactory(g_pFileSystemModule);
        if (!factory) {
            Con_Printf("Incompatible owned filesystem module.\n");
            Sys_UnloadModule(g_pFileSystemModule);
            g_pFileSystemModule = nullptr;
            return 0;
        }
        g_FileSystemFactory = factory;
	}

	g_pFileSystem = (IFileSystem*)factory( FILESYSTEM_INTERFACE_VERSION, NULL );
#if defined( _WIN32 )
	ASSERT_AT( g_pFileSystem, 109 );
#else
	if ( !g_pFileSystem )
		return 0;
#endif

    languageFilesystem = cse::localized_filesystem::Query(factory);
    cse::localization::Initialize();
	return COM_SetupDirectories() != 0;
}

void FileSystem_Shutdown()
{
    cse::localization::Shutdown();
    if (languageFilesystem) {
        languageFilesystem->Reset();
    }
    languageFilesystem = nullptr;
    languageMounts.clear();
#if INTPTR_MAX > INT32_MAX
    if (g_pFileSystem) {
        g_pFileSystem->RemoveAllSearchPaths();
    }
    g_pFileSystem = nullptr;
    g_FileSystemFactory = nullptr;
#else
	g_pFileSystem->RemoveAllSearchPaths();
#endif
	if ( g_pFileSystemModule )
	{
		Sys_UnloadModule( g_pFileSystemModule );
		g_pFileSystemModule = NULL;
	}
}

std::string FileSystem_ResolveInterfaceLanguage(const char* preference) {
    const char* current = SteamApps() ? SteamApps()->GetCurrentGameLanguage() : nullptr;
    const int option = COM_CheckParm("-language");
    const char* requested = option > 0 && option < com_argc - 1 && com_argv ? com_argv[option + 1] : nullptr;
    return cse::interface_language::Resolve(current, requested, preference);
}

std::string FileSystem_GetInterfaceLanguage() {
    return cse::localization::Language();
}

int FileSystem_SetGameDirectory( const char* defaultDirectory, const char* gameDirectory )
{
    const std::string selectedLanguage = FileSystem_GetInterfaceLanguage();
    const char* language = selectedLanguage.c_str();
	char path[512];
#if defined( _WIN32 ) && !defined( SWDS_DLL )
	char baseDirectory[MAX_PATH];
#else
	char baseDirectory[4096];
#endif

	g_pFileSystem->RemoveAllSearchPaths();
    if (languageFilesystem) {
        languageFilesystem->Reset();
    }
    languageMounts.clear();

#if defined( _WIN32 )
	char lowViolenceToken[128];
	lowViolenceToken[0] = '\0';
	const char* settingsKey = "Software\\Valve\\Half-Life\\Settings";
	Sys_GetRegKeyValue( settingsKey, "User Token 2", lowViolenceToken, sizeof( lowViolenceToken ) - 1, "" );
	bLowViolenceBuild = Q_strlen( lowViolenceToken ) > 0;
	if ( !bLowViolenceBuild )
	{
		Sys_GetRegKeyValue( settingsKey, "User Token 3", lowViolenceToken, sizeof( lowViolenceToken ) - 1, "" );
		bLowViolenceBuild = Q_strlen( lowViolenceToken ) > 0;
	}
#endif

	if ( !bLowViolenceBuild && SteamApps() && GetGameAppID() == APPID_HALF_LIFE )
		bLowViolenceBuild = SteamApps()->BIsLowViolence();

	if ( !g_bIsDedicatedServer && !IsGameSubscribed( gameDirectory ) )
		return 0;

	SteamAPI_SetBreakpadAppID( GetGameAppID() );

	bool highDefinition = BEnabledHDAddon();
	bool localized = language[0] && _stricmp( language, "english" );
	const char* directory = gameDirectory ? gameDirectory : defaultDirectory;

	if ( directory )
	{
		if ( bLowViolenceBuild )
		{
			snprintf( path, sizeof( path ) - 1, "%s/%s_lv", s_pBaseDir, directory );
			path[sizeof( path ) - 1] = '\0';
			COM_FixSlashes( path );
			g_pFileSystem->AddSearchPathNoWrite( path, "GAME" );
		}

		if ( BEnableAddonsFolder() )
		{
			snprintf( path, sizeof( path ) - 1, "%s/%s_addon", s_pBaseDir, directory );
			path[sizeof( path ) - 1] = '\0';
			COM_FixSlashes( path );
			g_pFileSystem->AddSearchPathNoWrite( path, "GAME" );
		}

        BeginLanguageMount(directory, "GAME", false, highDefinition);
		if ( localized )
		{
			snprintf( path, sizeof( path ) - 1, "%s/%s_%s", s_pBaseDir, directory, language );
			path[sizeof( path ) - 1] = '\0';
			COM_FixSlashes( path );
			g_pFileSystem->AddSearchPathNoWrite( path, "GAME" );

			if ( !COM_CheckParm( "-steam" ) )
			{
				Q_strncpy( baseDirectory, s_pBaseDir, sizeof( baseDirectory ) - 1 );
				baseDirectory[sizeof( baseDirectory ) - 1] = '\0';
				char* gamePath = Q_strstr( baseDirectory, "\\game" );
				if ( gamePath )
				{
					*gamePath = '\0';
					snprintf( path, sizeof( path ) - 1, "%s\\localization\\%s_%s", baseDirectory, directory, language );
					path[sizeof( path ) - 1] = '\0';
					COM_FixSlashes( path );
					g_pFileSystem->AddSearchPathNoWrite( path, "GAME" );
				}
			}
		}

        EndLanguageMount();
		if ( highDefinition )
		{
			snprintf( path, sizeof( path ) - 1, "%s/%s_hd", s_pBaseDir, directory );
			path[sizeof( path ) - 1] = '\0';
			COM_FixSlashes( path );
			g_pFileSystem->AddSearchPathNoWrite( path, "GAME" );
		}

		snprintf( path, sizeof( path ) - 1, "%s/%s", s_pBaseDir, directory );
		path[sizeof( path ) - 1] = '\0';
		COM_FixSlashes( path );
		g_pFileSystem->AddSearchPath( path, "GAME" );
		g_pFileSystem->AddSearchPath( path, "GAMECONFIG" );

		snprintf( path, sizeof( path ) - 1, "%s/%s_downloads", s_pBaseDir, directory );
		path[sizeof( path ) - 1] = '\0';
		COM_FixSlashes( path );
		g_pFileSystem->AddSearchPath( path, "GAMEDOWNLOAD" );

		CheckLiblistForFallbackDir( directory, localized, language, bLowViolenceBuild );
	}

	if ( localized )
	{
		if ( bLowViolenceBuild )
		{
			snprintf( path, sizeof( path ) - 1, "%s/%s_lv", s_pBaseDir, defaultDirectory );
			path[sizeof( path ) - 1] = '\0';
			COM_FixSlashes( path );
			g_pFileSystem->AddSearchPathNoWrite( path, "DEFAULTGAME" );
		}

		if ( BEnableAddonsFolder() )
		{
			snprintf( path, sizeof( path ) - 1, "%s/%s_addon", s_pBaseDir, defaultDirectory );
			path[sizeof( path ) - 1] = '\0';
			COM_FixSlashes( path );
			g_pFileSystem->AddSearchPathNoWrite( path, "DEFAULTGAME" );
		}

        BeginLanguageMount(defaultDirectory, "DEFAULTGAME", false, highDefinition);
		snprintf( path, sizeof( path ) - 1, "%s/%s_%s", s_pBaseDir, defaultDirectory, language );
		path[sizeof( path ) - 1] = '\0';
		COM_FixSlashes( path );
		g_pFileSystem->AddSearchPathNoWrite( path, "DEFAULTGAME" );

		if ( !COM_CheckParm( "-steam" ) )
		{
			Q_strncpy( baseDirectory, s_pBaseDir, sizeof( baseDirectory ) - 1 );
			baseDirectory[sizeof( baseDirectory ) - 1] = '\0';
			char* gamePath = Q_strstr( baseDirectory, "\\game" );
			if ( gamePath )
			{
				*gamePath = '\0';
				snprintf( path, sizeof( path ) - 1, "%s\\localization\\%s_%s", baseDirectory, defaultDirectory, language );
				path[sizeof( path ) - 1] = '\0';
				COM_FixSlashes( path );
				g_pFileSystem->AddSearchPathNoWrite( path, "DEFAULTGAME" );
			}
		}
	}

    if (localized) {
        EndLanguageMount();
    } else {
        BeginLanguageMount(defaultDirectory, "DEFAULTGAME", false, highDefinition);
        EndLanguageMount();
    }

	if ( highDefinition )
	{
		snprintf( path, sizeof( path ) - 1, "%s/%s_hd", s_pBaseDir, defaultDirectory );
		path[sizeof( path ) - 1] = '\0';
		COM_FixSlashes( path );
		g_pFileSystem->AddSearchPathNoWrite( path, "DEFAULTGAME" );
	}

	snprintf( path, sizeof( path ) - 1, "%s", s_pBaseDir );
	path[sizeof( path ) - 1] = '\0';
	COM_FixSlashes( path );
	g_pFileSystem->AddSearchPath( path, "BASE" );

	snprintf( path, sizeof( path ) - 1, "%s/%s", s_pBaseDir, defaultDirectory );
	path[sizeof( path ) - 1] = '\0';
	COM_FixSlashes( path );
	g_pFileSystem->AddSearchPathNoWrite( path, "DEFAULTGAME" );

	snprintf( path, sizeof( path ) - 1, "%s/platform", s_pBaseDir );
	path[sizeof( path ) - 1] = '\0';
	COM_FixSlashes( path );
	g_pFileSystem->AddSearchPath( path, "PLATFORM" );

	return 1;
}


void CheckLiblistForFallbackDir( const char* pGameDir, bool bLanguage, const char* pLanguage, bool lowViolence ){
	char path[512];
	char line[512];
	char fallback[128];
#if defined( _WIN32 ) && !defined( SWDS_DLL )
	char baseDir[MAX_PATH];
#else
	char baseDir[4096];
#endif

	snprintf( path, sizeof( path ) - 1, "%s/liblist.gam", pGameDir );
	COM_FixSlashes( path );
	g_pFileSystem->GetLocalCopy( path );

	FileHandle_t file;
	if ( !_stricmp( com_gamedir, pGameDir ) )
	{
		file = FS_Open( "liblist.gam", "rt" );
	}
	else
	{
		snprintf( path, sizeof( path ) - 1, "../%s/liblist.gam", pGameDir );
		COM_FixSlashes( path );
		file = FS_Open( path, "rt" );
	}

	if ( !file )
		return;

	while ( !FS_EndOfFile( file ) )
	{
		line[0] = 0;
		FS_ReadLine( line, sizeof( line ) - 1, file );
		line[sizeof( line ) - 1] = 0;
		if ( _strnicmp( line, "fallback_dir", strlen( "fallback_dir" ) ) )
			continue;

		char* start = strchr( line, '"' );
		if ( !start )
			break;

		char* end = strchr( start + 1, '"' );
		if ( !end )
			break;

		unsigned int len = (unsigned int)( end - start - 1 );
		if ( len >= sizeof( fallback ) - 1 )
			break;
		if ( !len )
			continue;

		strncpy( fallback, start + 1, len );
		fallback[len] = 0;
		if ( !_stricmp( pGameDir, fallback ) )
			break;

		if ( lowViolence )
		{
			snprintf( path, sizeof( path ) - 1, "%s/%s_lv", s_pBaseDir, fallback );
			path[sizeof( path ) - 1] = 0;
			COM_FixSlashes( path );
			g_pFileSystem->AddSearchPathNoWrite( path, "GAME_FALLBACK" );
		}

		if ( BEnableAddonsFolder() )
		{
			snprintf( path, sizeof( path ) - 1, "%s/%s_addon", s_pBaseDir, fallback );
			path[sizeof( path ) - 1] = 0;
			COM_FixSlashes( path );
			g_pFileSystem->AddSearchPathNoWrite( path, "GAME_FALLBACK" );
		}

        BeginLanguageMount(fallback, "GAME_FALLBACK", true, BEnabledHDAddon());
		if ( bLanguage && pLanguage )
		{
			snprintf( path, sizeof( path ) - 1, "%s/%s_%s", s_pBaseDir, fallback, pLanguage );
			path[sizeof( path ) - 1] = 0;
			COM_FixSlashes( path );
			g_pFileSystem->AddSearchPath( path, "GAME_FALLBACK" );

			if ( !COM_CheckParm( "-steam" ) )
			{
				Q_strncpy( baseDir, s_pBaseDir, sizeof( baseDir ) - 1 );
				baseDir[sizeof( baseDir ) - 1] = 0;
				char* gamePath = Q_strstr( baseDir, "\\game" );
				if ( gamePath )
				{
					*gamePath = 0;
					snprintf( path, sizeof( path ) - 1, "%s\\localization\\%s_%s", baseDir, fallback, pLanguage );
					path[sizeof( path ) - 1] = 0;
					COM_FixSlashes( path );
					g_pFileSystem->AddSearchPath( path, "GAME_FALLBACK" );
				}
			}
		}

        EndLanguageMount();

		if ( BEnabledHDAddon() )
		{
			snprintf( path, sizeof( path ) - 1, "%s/%s_hd", s_pBaseDir, fallback );
			path[sizeof( path ) - 1] = 0;
			COM_FixSlashes( path );
			g_pFileSystem->AddSearchPathNoWrite( path, "GAME_FALLBACK" );
		}

		snprintf( path, sizeof( path ) - 1, "%s/%s", s_pBaseDir, fallback );
		path[sizeof( path ) - 1] = 0;
		COM_FixSlashes( path );
		g_pFileSystem->AddSearchPath( path, "GAME_FALLBACK" );

		if ( strcmp( fallback, "valve" ) )
		{
			char* fileName = new char[128];
			snprintf( fileName, 127, "Resource/%s_%%language%%.txt", fallback );
			fileName[127] = 0;
			g_fallbackLocalizationFiles.InsertBefore( g_fallbackLocalizationFiles.Count(), fileName );
			CheckLiblistForFallbackDir( fallback, bLanguage, pLanguage, lowViolence );
		}
		break;
	}

	FS_Close( file );
}

char* GetBaseDirectory()
{
	return s_pBaseDir;
}

CreateInterfaceFn GetFileSystemFactory()
{
	return g_FileSystemFactory;
}

int FileSystem_AddFallbackGameDir( const char* pGameDir )
{
    const std::string selectedLanguage = FileSystem_GetInterfaceLanguage();
    const char* language = selectedLanguage.c_str();
#if defined( _WIN32 ) && !defined( SWDS_DLL )
	char path[MAX_PATH];
#else
	char path[4096];
#endif

    BeginLanguageMount(pGameDir, "GAME", true, false, pGameDir);
	if ( language[0] && _stricmp( language, "english" ) )
	{
		snprintf( path, sizeof( path ), "%s/%s_%s", s_pBaseDir, pGameDir, language );
		path[sizeof( path ) - 1] = '\0';
		g_pFileSystem->AddSearchPath( path, "GAME" );
	}

    EndLanguageMount();
	g_pFileSystem->AddSearchPath( pGameDir, "GAME" );

	return 1;
}

static void SetGameContentOption( const char* option, bool ( *isEnabled )() )
{
	if ( cls.state == ca_dedicated || Cmd_Argc() != 2 )
		return;

	bool previous = registry->ReadInt( option, 0 ) > 0;
	registry->WriteInt( option, !_stricmp( Cmd_Argv( 1 ), "1" ) );

	if ( previous != isEnabled() )
		COM_SetupDirectories();
}

void Host_SetHDModels_f()
{
	SetGameContentOption( "hdmodels", BEnabledHDAddon );
}

void Host_SetAddonsFolder_f()
{
	SetGameContentOption( "addons_folder", BEnableAddonsFolder );
}
