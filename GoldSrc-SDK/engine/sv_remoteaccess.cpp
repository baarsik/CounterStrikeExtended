#if defined(__APPLE__) && defined(SW_RENDERER)
#include "native_software_lifetime.h"
#endif
#include "utlbuffer.h"
#include "utllinkedlist.h"
#include "interface.h"
#include "cmd.h"
#include "cvar.h"
#include "conprint.h"
#include "common.h"
#include "filesystem.h"
#include "server.h"
#include "sys.h"
#include "host.h"
#include "net.h"
#include "filter.h"
#include "eiface.h"
#include "admin_provider_status.h"

#if defined( _WIN32 )
#include <cassert>
#endif

#include <cstdio>
#include <cstring>
#include <cstdint>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\sv_remoteaccess.cpp" )

#define SERVERDATA_REQUESTVALUE 0
#define SERVERDATA_SETVALUE 1
#define SERVERDATA_EXECCOMMAND 2

#define SERVERDATA_RESPONSE_VALUE 0
#define SERVERDATA_UPDATE 1

#include "../public/game_server_data.h"

class CServerRemoteAccess : public IGameServerData
{
public:
	struct DataResponse_t
	{
		CUtlBuffer packet;

		DataResponse_t() : packet( 0, 0, false )
		{
		}
	};

	CServerRemoteAccess();
	~CServerRemoteAccess();

	void WriteDataRequest( const void* buffer, int bufferSize );
	int ReadDataResponse( void* buffer, int bufferSize );
	void SendMessageToAdminUI( const char* message );
#if defined(__APPLE__) && defined(SW_RENDERER)
    bool RetireResponses() noexcept;
#endif

private:
	void SetValue( const char* variable, const char* value );
	void ExecCommand( const char* cmdString );
	const char* LookupStringValue( const char* variable );
	void GetUserBanList( CUtlBuffer& value );
	void GetPlayerList( CUtlBuffer& value );
	void GetMapList( CUtlBuffer& value );
	bool LookupValue( const char* variable, CUtlBuffer& value );
	void RequestValue( int requestID, const char* variable );

	CUtlLinkedList<DataResponse_t, int> m_ResponsePackets;
	std::uint64_t m_iBytesSent;
	std::uint64_t m_iBytesReceived;
};

extern cvar_t mapcyclefile;
void GetStatsString( char* buffer, int bufferSize );

CServerRemoteAccess::CServerRemoteAccess() : m_ResponsePackets( 0, 0 ), m_iBytesSent( 0 ), m_iBytesReceived( 0 )
{
}

CServerRemoteAccess::~CServerRemoteAccess()
{
}

void CServerRemoteAccess::WriteDataRequest( const void* buffer, int bufferSize )
{
#if defined(__APPLE__) && defined(SW_RENDERER)
    cse::software_lifetime::Entry entry;
    if (!entry) {
        return;
    }
#endif
    constexpr int HeaderBytes = 2 * sizeof(std::int32_t);
    constexpr int MaximumVariableBytes = 256;
    constexpr int MaximumValueBytes = 512;
    constexpr int MaximumCommandBytes = 512;
    constexpr int MaximumRequestBytes = HeaderBytes + MaximumVariableBytes + MaximumValueBytes;
    if (!buffer || bufferSize < HeaderBytes + 2 || bufferSize > MaximumRequestBytes) {
        return;
    }
    const auto* bytes = static_cast<const char*>(buffer);
    std::int32_t requestID;
    std::int32_t requestType;
    std::memcpy(&requestID, bytes, sizeof(requestID));
    std::memcpy(&requestType, bytes + sizeof(requestID), sizeof(requestType));
    if (requestID < 0 || requestType < SERVERDATA_REQUESTVALUE || requestType > SERVERDATA_EXECCOMMAND) {
        return;
    }
    const char* variable = bytes + HeaderBytes;
    const auto* separator = static_cast<const char*>(std::memchr(variable, 0, bufferSize - HeaderBytes));
    if (!separator) {
        return;
    }
    const int variableBytes = static_cast<int>(separator - variable) + 1;
    const int variableLimit = requestType == SERVERDATA_SETVALUE ? MaximumVariableBytes : MaximumCommandBytes;
    if (variableBytes > variableLimit) {
        return;
    }
    const char* value = separator + 1;
    const int valueBytes = bufferSize - static_cast<int>(value - bytes);
    if (valueBytes <= 0 || valueBytes > MaximumValueBytes || bytes[bufferSize - 1] != 0 ||
        std::memchr(value, 0, valueBytes) != bytes + bufferSize - 1 ||
        (requestType != SERVERDATA_SETVALUE && valueBytes != 1)) {
        return;
    }
    m_iBytesReceived += static_cast<std::uint64_t>(bufferSize);
    switch (requestType) {
    case SERVERDATA_SETVALUE:
        SetValue(variable, value);
        break;
    case SERVERDATA_EXECCOMMAND:
        ExecCommand(variable);
        break;
    case SERVERDATA_REQUESTVALUE:
        RequestValue(requestID, variable);
        break;
    }

}

int CServerRemoteAccess::ReadDataResponse( void* buffer, int bufferSize )
{
#if defined(__APPLE__) && defined(SW_RENDERER)
    cse::software_lifetime::Entry entry;
    if (!entry) {
        return 0;
    }
#endif
    if (!buffer || bufferSize <= 0) {
        return 0;
    }
    while (m_ResponsePackets.IsValidIndex(m_ResponsePackets.Head())) {
        const int index = m_ResponsePackets.Head();
        CUtlBuffer& response = m_ResponsePackets[index].packet;
        const int packetBytes = response.TellPut();
        if (packetBytes <= 0 || packetBytes > bufferSize) {
            // Never return a partial record, or report an empty queue while later records remain.
            m_ResponsePackets.Remove(index);
            continue;
        }
        std::memcpy(buffer, response.Base(), packetBytes);
        m_iBytesSent += static_cast<std::uint64_t>(packetBytes);
        m_ResponsePackets.Remove(index);
        return packetBytes;
    }
    return 0;
}

void CServerRemoteAccess::SetValue( const char* variable, const char* value )
{
	if ( !_stricmp( variable, "map" ) )
	{
		Cbuf_AddText( "changelevel " );
		Cbuf_AddText( value );
		Cbuf_AddText( "\n" );
		Cbuf_Execute();
		return;
	}

	if ( !_stricmp( variable, "mapcycle" ) )
	{
		FileHandle_t f = FS_Open( mapcyclefile.string, "wt" );
		if ( !f )
		{
			Con_Printf( "Couldn't write to read-only file %s, using file _dev_mapcycle.txt instead.\n", mapcyclefile.string );
			Cvar_DirectSet( &mapcyclefile, "_temp_mapcycle.txt" );
			f = FS_Open( mapcyclefile.string, "wt" );
		}

		if ( f )
		{
			FS_Write( value, (int)strlen( value ) + 1, 1, f );
			FS_Close( f );
		}
		return;
	}

	cvar_t* var = Cvar_FindVar( variable );
	if ( var )
		Cvar_DirectSet( var, value );
}

void CServerRemoteAccess::ExecCommand( const char* cmdString )
{
	Cbuf_AddText( cmdString );
	Cbuf_AddText( "\n" );
	Cbuf_Execute();
}

const char* CServerRemoteAccess::LookupStringValue( const char* variable )
{
	static char s_ReturnBuf[32];

    if (!std::strncmp(variable, "provider_", std::strlen("provider_"))) {
        const auto status = Steam_QueryAdminStatus();
        int value = 0;
        if (!std::strcmp(variable, "provider_available")) {
            value = status.available;
        } else if (!std::strcmp(variable, "provider_connected")) {
            value = status.connected;
        } else if (!std::strcmp(variable, "provider_secure_requested")) {
            value = status.secureRequested;
        } else if (!std::strcmp(variable, "provider_secure")) {
            value = status.secure;
        } else if (!std::strcmp(variable, "provider_restart")) {
            value = status.restartRequested;
        } else if (!std::strcmp(variable, "provider_failure")) {
            value = status.failure;
        } else {
            return nullptr;
        }
        std::snprintf(s_ReturnBuf, sizeof(s_ReturnBuf), "%d", value);
        return s_ReturnBuf;
    }
	cvar_t* var = Cvar_FindVar( variable );
	if ( var )
		return var->string;

	if ( !_stricmp( variable, "map" ) )
		return sv.name;

	if ( !_stricmp( variable, "playercount" ) )
	{
		int i, count;
		client_t* client;

		count = 0;
		for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
		{
			if ( client->active || client->spawned || client->connected )
				count++;
		}

		snprintf( s_ReturnBuf, sizeof( s_ReturnBuf ) - 1, "%d", count );
		return s_ReturnBuf;
	}

	if ( !_stricmp( variable, "maxplayers" ) )
	{
		snprintf( s_ReturnBuf, sizeof( s_ReturnBuf ) - 1, "%d", svs.maxclients );
		return s_ReturnBuf;
	}

	if ( !_stricmp( variable, "gamedescription" ) )
		return gEntityInterface.pfnGetGameDescription();

	return NULL;
}

void CServerRemoteAccess::GetUserBanList( CUtlBuffer& value )
{
	int i;

	for ( i = 0; i < numuserfilters; i++ )
		value.Printf( "%i %s : %.3f min\n", i + 1, SV_GetIDString( &userfilters[i].userid ), userfilters[i].banTime );

	for ( i = 0; i < numipfilters; i++ )
	{
		signed char* b = (signed char*)&ipfilters[i].compare;
		value.Printf( "%i %i.%i.%i.%i : %.3f min\n", numuserfilters + i + 1, b[0], b[1], b[2], b[3], ipfilters[i].banTime );
	}

	value.PutChar( 0 );
}

void CServerRemoteAccess::GetPlayerList( CUtlBuffer& value )
{
	int i;
	client_t* client;

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( !client->active || !client->name[0] )
			continue;

		int time = (int)( realtime - client->netchan.connect_time );
		int loss = (int)client->packet_loss;
		int frags = (int)client->edict->v.frags;
#if defined( _WIN32 )
		float pingMilliseconds = 1000.0f * client->latency;
		int ping = static_cast<int>( pingMilliseconds );
#else
		int ping = (int)( 1000.0 * client->latency );
#endif
		const char* id = SV_GetIDString( &client->network_userid );
		const char* adr = NET_AdrToString( client->netchan.remote_address );

		value.Printf( "\"%s\" %s %s %d %d %d %d\n", client->name, id, adr, ping, loss, frags, time );
	}

	value.PutChar( 0 );
}

void CServerRemoteAccess::GetMapList( CUtlBuffer& value )
{
	char mapwild[64];
	char curDir[4096];
	char mapName[4096];
#if defined( _WIN32 )
	constexpr int localPathBytes = 260;
#else
	constexpr int localPathBytes = sizeof( curDir );
#endif
	const char* findfn;
	char* extension;

	strcpy( mapwild, "maps/*.bsp" );
	for ( findfn = Sys_FindFirst( mapwild, NULL ); findfn; findfn = Sys_FindNext( NULL ) )
	{
		snprintf( curDir, localPathBytes, "maps/%s", findfn );
		FS_GetLocalPath( curDir, curDir, localPathBytes );
		if ( !strstr( curDir, com_gamedir ) )
			continue;

		strcpy( mapName, findfn );
		extension = strstr( mapName, ".bsp" );
		if ( extension )
			*extension = 0;

		value.PutString( mapName );
		value.PutString( "\n" );
	}
	Sys_FindClose();

	value.PutChar( 0 );
}

bool CServerRemoteAccess::LookupValue( const char* variable, CUtlBuffer& value )
{
#if defined( _WIN32 )
	ASSERT_AT( value.IsText(), 312 );
#endif
	const char* strval = LookupStringValue( variable );
	if ( strval )
	{
		value.PutString( strval );
		value.PutChar( 0 );
		return true;
	}

	if ( !_stricmp( variable, "stats" ) )
	{
		char stats[512];
		GetStatsString( stats, sizeof( stats ) );
		value.PutString( stats );
		value.PutChar( 0 );
		return true;
	}

	if ( !_stricmp( variable, "banlist" ) )
	{
		GetUserBanList( value );
		return true;
	}

	if ( !_stricmp( variable, "playerlist" ) )
	{
		GetPlayerList( value );
		return true;
	}

	if ( !_stricmp( variable, "maplist" ) )
	{
		GetMapList( value );
		return true;
	}

	if ( !_stricmp( variable, "uptime" ) )
	{
		value.PutInt( (int)( Sys_FloatTime() - Host_GetStartTime() ) );
		value.PutChar( 0 );
		return true;
	}

	if ( !_stricmp( variable, "ipaddress" ) )
	{
		value.PutString( NET_AdrToString( net_local_adr ) );
		value.PutChar( 0 );
		return true;
	}

	if ( !_stricmp( variable, "mapcycle" ) )
	{
		int len = 0;
		byte* data = COM_LoadFileForMe( mapcyclefile.string, &len );
        if (data) {
            if (len > 0) {
                value.PutString(reinterpret_cast<const char*>(data));
            }
            COM_FreeFile(data);
        }
        // Empty and missing files still complete the value request with a terminated empty string.
        value.PutChar(0);
		return true;
	}

	value.PutChar( 0 );
	return false;
}

void CServerRemoteAccess::RequestValue( int requestID, const char* variable )
{
	CUtlBuffer value( 0, 256, true );
	LookupValue( variable, value );

	int i = m_ResponsePackets.AddToTail();
	CUtlBuffer& response = m_ResponsePackets[i].packet;

	response.PutInt( requestID );
	response.PutInt( SERVERDATA_RESPONSE_VALUE );
	response.PutString( variable );
#if defined( _WIN32 )
	ASSERT_AT( value.TellPut() > 0, 246 );
#endif
	response.PutInt( value.TellPut() );
	if ( value.TellPut() )
		response.Put( value.Base(), value.TellPut() );
}

void CServerRemoteAccess::SendMessageToAdminUI( const char* message )
{
#if defined(__APPLE__) && defined(SW_RENDERER)
    cse::software_lifetime::Entry entry;
    if (!entry) {
        return;
    }
#endif
	int i = m_ResponsePackets.AddToTail();
	CUtlBuffer& response = m_ResponsePackets[i].packet;

	response.PutInt( 0 );
	response.PutInt( SERVERDATA_UPDATE );
	response.PutString( message );
}

#if defined(__APPLE__) && defined(SW_RENDERER)
bool CServerRemoteAccess::RetireResponses() noexcept {
    if (!cse::software_lifetime::RetirementInProgress()) {
        return false;
    }
    m_ResponsePackets.RemoveAll();
    return true;
}
#endif

CServerRemoteAccess g_ServerRemoteAccess;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CServerRemoteAccess, IGameServerData, "GameServerData001", g_ServerRemoteAccess );

void NotifyDedicatedServerUI( const char* message )
{
	g_ServerRemoteAccess.SendMessageToAdminUI( message );
}

#if defined(__APPLE__) && defined(SW_RENDERER)
#include "../public/native_dedicated_abi.h"
extern "C" __attribute__((visibility("default"))) const cse::abi::Descriptor* CseGetGameServerDataAbiV1() {
    return &cse::dedicated_abi::Admin;
}
#endif

#if defined(__APPLE__) && defined(SW_RENDERER)
bool cse::software_lifetime::RetireRemoteAccess() noexcept {
    return g_ServerRemoteAccess.RetireResponses();
}
#endif
