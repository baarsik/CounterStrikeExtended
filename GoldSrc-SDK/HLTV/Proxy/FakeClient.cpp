#include "FakeClient.h"

#include "common/IBaseSystem.h"
#include "HLTV/common/INetwork.h"
#include "HLTV/common/IWorld.h"
#include "HLTV/common/IServer.h"
#include "HLTV/common/INetSocket.h"
#include "HLTV/common/NetPacket.h"

#include <stdio.h>

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
FakeClient::FakeClient()
{
}

FakeClient::~FakeClient()
{
}

void FakeClient::RunFrame( double time )
{
	BaseSystemModule::RunFrame( time );

	NetPacket* packet;
	while ( ( packet = m_Socket->ReceivePacket() ) != NULL )
	{
		m_System->DPrintf( "FakeClient: WARNING! Packet from %s with invalid sequence number.\n",
				packet->address.ToString() );
		m_Socket->FreePacket( packet );
	}
}

void FakeClient::SetRate( int rate )
{
	m_Server->SetRate( rate );
}

void FakeClient::Connect( NetAddress* adr )
{
	m_Server->Connect( m_World, adr, m_Socket );
}

void FakeClient::Retry()
{
	m_Server->Retry();
}

void FakeClient::Say( char* text )
{
	if ( !m_Server->IsConnected() )
		return;

	char string[1024];
	snprintf( string, sizeof( string ), "say \"%s\"", text );
	m_Server->SendStringCommand( string );
}

void FakeClient::Disconnect()
{
	m_Server->Disconnect();
}

void FakeClient::ShutDown()
{
	if ( m_State == 4 )
		return;

#if CSE_NATIVE_SYSTEM_MODULE
    if (m_NativeShuttingDown) {
        return;
    }
    m_NativeShuttingDown = true;
    if (m_Network) {
        m_Network->RemoveListener(this);
    }
    if (m_Server) {
        m_Server->RemoveListener(this);
        m_System->RemoveModule(m_Server);
    }
    if (m_World) {
        m_World->RemoveListener(this);
        m_System->RemoveModule(m_World);
    }
    // The socket is borrowed. Network::RemoveSocket only unlinks it; keep the
    // provider's ownership until Network::ShutDown closes and destroys it.
    m_Server = nullptr;
    m_World = nullptr;
    m_Network = nullptr;
    m_Socket = nullptr;
#else
	if ( m_World )
		m_World->RemoveListener( this );
#endif

	BaseSystemModule::ShutDown();
	m_System->Printf( "FakeClient module shutdown.\n" );
}

void FakeClient::ReceiveSignal( ISystemModule* module, unsigned int signal, void* )
{
#if CSE_NATIVE_SYSTEM_MODULE
    if (m_Network && module == static_cast<ISystemModule *>(m_Network) &&
        signal == cse::modules::NetworkShutdownSignal) {
        ShutDown();
        return;
    }
    constexpr unsigned int WorldShutdownSignal = 8;
    if (m_World && module == static_cast<ISystemModule*>(m_World) && signal == WorldShutdownSignal) {
        ShutDown();
    }
#else
	if ( module->GetSerial() == m_World->GetSerial() && signal == 8 )
		ShutDown();
#endif
}

char* FakeClient::GetType()
{
	return "fakeclient000";
}

char* FakeClient::GetStatusLine()
{
	return "I am a fake client.\n";
}

bool FakeClient::Init( IBaseSystem* system, int serial, char* name )
{
#if CSE_NATIVE_SYSTEM_MODULE
    m_Server = nullptr;
    m_World = nullptr;
    m_Network = nullptr;
    m_Socket = nullptr;
#endif
	BaseSystemModule::Init( system, serial, name );
	sprintf( m_Name, "fakeclient%i", serial );

#if CSE_NATIVE_SYSTEM_MODULE
    m_Network = cse::modules::Acquire<INetwork>(m_System, "network001", "core", NULL);
#else
	m_Network = dynamic_cast<INetwork*>( m_System->GetModule( "network001", "core", NULL ) );
#endif
	if ( !m_Network )
	{
		m_System->Errorf( "FakeClient::Init: couldn't load network module.\n" );
		return false;
	}

#if CSE_NATIVE_SYSTEM_MODULE
    if (!cse::modules::Subscribe(m_Network, this)) {
        return false;
    }
#endif
	m_Socket = m_Network->CreateSocket( serial + 2048, false, false );
	if ( !m_Socket )
	{
		m_System->Errorf( "FakeClient::Init: Could not create port %i.\n", serial + 2048 );
		return false;
	}

	char temp[256];
	sprintf( temp, "fakeserver%i", serial );

#if CSE_NATIVE_SYSTEM_MODULE
    m_Server = cse::modules::Acquire<IServer>(m_System, "server001", "core", temp);
#else
	m_Server = dynamic_cast<IServer*>( m_System->GetModule( "server001", "core", temp ) );
#endif
	if ( !m_Server )
	{
		m_System->Errorf( "FakeClient::Init: couldn't load server module.\n" );
		return false;
	}

	m_Server->RegisterListener( this );
	m_Server->SetUserInfo( "hspecs", "0" );
	m_Server->SetUserInfo( "hslots", "0" );
	m_Server->SetDelayReconnect( false );
	m_Server->SetPlayerName( m_Name );
	m_Server->SetAutoRetry( false );

	sprintf( temp, "fakeworld%i", serial );

#if CSE_NATIVE_SYSTEM_MODULE
    m_World = cse::modules::Acquire<IWorld>(m_System, "world001", "core", temp);
#else
	m_World = dynamic_cast<IWorld*>( m_System->GetModule( "world001", "core", temp ) );
#endif
	if ( !m_World )
	{
		m_System->Errorf( "FakeClient::Init: couldn't load world module.\n" );
		return false;
	}

	m_World->RegisterListener( this );
	m_State = 3;
	m_System->Printf( "Fake client module initialized (%i).\n", serial );
	return true;
}
