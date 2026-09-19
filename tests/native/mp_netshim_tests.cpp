// mp_netshim_tests.cpp -- loopback tests for the RakNet-compat netshim over
// SDL3_net (Multiplayer/netshim/). Links ONLY the shim + SDL3_net, no game code.
// Drives real TCP sockets on 127.0.0.1; exercises the exact semantics the JA2
// MP wrapper depends on (see RakPeerInterface.h).

#include <cstdio>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

#include "RakPeerInterface.h"
#include "RakNetworkFactory.h"
#include "MessageIdentifiers.h"
#include "FileListTransfer.h"

static int g_failures = 0;
#define CHECK( cond, msg ) do { if ( !( cond ) ) { ++g_failures; printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, msg ); } else { printf( "ok   %s\n", msg ); } } while ( 0 )

static unsigned short g_port = 0;

static bool BytesEqual( const std::vector<unsigned char>& actual, const void* expected, size_t size )
{
	return actual.size() >= size && memcmp( actual.data(), expected, size ) == 0;
}

static bool BytesEqual( const std::vector<char>& actual, const void* expected, size_t size )
{
	return actual.size() >= size && memcmp( actual.data(), expected, size ) == 0;
}

struct PeerLog
{
	explicit PeerLog( RakPeerInterface* p ) : peer( p ) {}
	RakPeerInterface* peer;
	std::vector<unsigned char> ids;
	std::vector<SystemAddress> addrs;
	bool Got( unsigned char id ) const
	{
		for ( unsigned char x : ids ) if ( x == id ) return true;
		return false;
	}
};

// Pump all peers, collecting synthesized packets, until pred() or timeout.
template <typename Pred>
static bool PumpUntil( std::vector<PeerLog*> peers, Pred pred, int timeoutMs = 5000 )
{
	Uint64 start = SDL_GetTicks();
	for ( ;; )
	{
		for ( PeerLog* pl : peers )
		{
			for ( Packet* pk = pl->peer->Receive(); pk; pk = pl->peer->Receive() )
			{
				pl->ids.push_back( pk->data[0] );
				pl->addrs.push_back( pk->systemAddress );
				pl->peer->DeallocatePacket( pk );
			}
		}
		if ( pred() )
			return true;
		if ( SDL_GetTicks() - start >= (Uint64)timeoutMs )
			return false;
		SDL_Delay( 2 );
	}
}

// ---- RPC capture (static handlers, RakNet-style) ----------------------------
struct Captured
{
	int count = 0;
	std::vector<unsigned char> bytes;   // payload + 2 peeked pad bytes
	unsigned int bits = 0;
	SystemAddress sender;
};
static Captured g_capSrv, g_capA, g_capB;
static RakPeerInterface* g_relayPeer = nullptr;

static void Capture( Captured& c, RPCParameters* p )
{
	c.count++;
	c.bits = p->numberOfBitsOfData;
	unsigned int n = ( p->numberOfBitsOfData + 7 ) / 8;
	c.bytes.assign( p->input, p->input + n + 2 );   // +2: verify zero-padding
	c.sender = p->sender;
}
static void srvPING( RPCParameters* p )
{
	Capture( g_capSrv, p );
	if ( g_relayPeer )   // canonical server.cpp relay: everyone EXCEPT sender
		g_relayPeer->RPC( "clPONG", (const char*)p->input, p->numberOfBitsOfData, HIGH_PRIORITY, RELIABLE, 0,
		                  p->sender, true, 0, UNASSIGNED_NETWORK_ID, 0 );
}
static void clPONG_A( RPCParameters* p ) { Capture( g_capA, p ); }
static void clPONG_B( RPCParameters* p ) { Capture( g_capB, p ); }

// ---- file transfer capture ---------------------------------------------------
struct FtCap : public FileListTransferCBInterface
{
	int files = 0, progress = 0, complete = 0;
	std::string lastName;
	std::vector<char> lastData;
	bool OnFile( OnFileStruct* s ) override
	{
		files++;
		lastName = s->fileName;
		lastData.clear();
		if ( s->finalDataLength )
			lastData.assign( s->fileData, s->fileData + s->finalDataLength );
		return true;
	}
	void OnFileProgress( OnFileStruct*, unsigned, unsigned, unsigned, char* ) override { progress++; }
	bool OnDownloadComplete( void ) override { complete++; return false; }
};

struct ReentrantFtCap : public FileListTransferCBInterface
{
	RakPeerInterface* peer = nullptr;
	std::vector<std::string> events;
	int nestedReceives = 0;

	void OnFileProgress( OnFileStruct* s, unsigned, unsigned, unsigned, char* ) override
	{
		events.push_back( "progress" + std::to_string( s->fileIndex ) );
		++nestedReceives;
		Packet* packet = peer->Receive();
		if ( packet ) peer->DeallocatePacket( packet );
	}
	bool OnFile( OnFileStruct* s ) override
	{
		events.push_back( "file" + std::to_string( s->fileIndex ) );
		return true;
	}
	bool OnDownloadComplete( void ) override
	{
		events.push_back( "complete" );
		return false;
	}
};

struct ShutdownFtCap : public FileListTransferCBInterface
{
	RakPeerInterface* peer = nullptr;
	int progress = 0;
	int files = 0;
	int complete = 0;

	void OnFileProgress( OnFileStruct*, unsigned, unsigned, unsigned, char* ) override
	{
		++progress;
		peer->Shutdown( 0 );
	}
	bool OnFile( OnFileStruct* ) override { ++files; return true; }
	bool OnDownloadComplete( void ) override { ++complete; return false; }
};

struct CloseConnectionFtCap : public FileListTransferCBInterface
{
	RakPeerInterface* peer = nullptr;
	SystemAddress sender;
	int progress = 0;
	bool borrowedDataSurvived = false;

	void OnFileProgress( OnFileStruct* s, unsigned, unsigned, unsigned, char* ) override
	{
		++progress;
		peer->CloseConnection( sender, false );
		// RakNet promises these borrowed fields for the entire callback. Reading
		// them after the close request catches eager RxSet/RxFile destruction.
		borrowedDataSurvived = s && strcmp( s->fileName, "close-during-callback" ) == 0 &&
			s->fileData && s->fileData[0] == 'a';
	}
	bool OnFile( OnFileStruct* ) override { return true; }
};

struct DetachPluginFtCap : public FileListTransferCBInterface
{
	RakPeerInterface* peer = nullptr;
	FileListTransfer* plugin = nullptr;
	int progress = 0;
	bool borrowedDataSurvived = false;

	void OnFileProgress( OnFileStruct* s, unsigned, unsigned, unsigned, char* ) override
	{
		++progress;
		peer->DetachPlugin( plugin );
		borrowedDataSurvived = s && strcmp( s->fileName, "detach-during-callback" ) == 0 &&
			s->fileData && s->fileData[0] == 'b';
	}
	bool OnFile( OnFileStruct* ) override { return true; }
};

static void WireU16( std::vector<unsigned char>& out, unsigned int value )
{
	out.push_back( (unsigned char)( value & 0xff ) );
	out.push_back( (unsigned char)( ( value >> 8 ) & 0xff ) );
}

static void WireU32( std::vector<unsigned char>& out, unsigned int value )
{
	out.push_back( (unsigned char)( value & 0xff ) );
	out.push_back( (unsigned char)( ( value >> 8 ) & 0xff ) );
	out.push_back( (unsigned char)( ( value >> 16 ) & 0xff ) );
	out.push_back( (unsigned char)( ( value >> 24 ) & 0xff ) );
}

static std::vector<unsigned char> WireFrame( unsigned char type, const std::vector<unsigned char>& body )
{
	std::vector<unsigned char> out;
	WireU32( out, (unsigned int)body.size() );
	out.push_back( type );
	out.insert( out.end(), body.begin(), body.end() );
	return out;
}

static std::vector<unsigned char> FileBody( unsigned short setID, unsigned int fileIndex,
	unsigned int setCount, unsigned int setTotal, const std::string& name,
	unsigned int fileLen, unsigned int offset, const std::vector<unsigned char>& chunk,
	unsigned int encodedChunkLen = 0xffffffffu )
{
	std::vector<unsigned char> body;
	WireU16( body, setID );
	WireU32( body, fileIndex );
	WireU32( body, setCount );
	WireU32( body, setTotal );
	WireU16( body, (unsigned int)name.size() );
	body.insert( body.end(), name.begin(), name.end() );
	WireU32( body, fileLen );
	WireU32( body, offset );
	WireU32( body, encodedChunkLen == 0xffffffffu ? (unsigned int)chunk.size() : encodedChunkLen );
	body.insert( body.end(), chunk.begin(), chunk.end() );
	return body;
}

struct RawConn
{
	NET_StreamSocket* sock = nullptr;
	SystemAddress onServer;
};

static bool ConnectRaw( RakPeerInterface*, PeerLog& log, RawConn& raw )
{
	NET_Address* address = NET_ResolveHostname( "127.0.0.1" );
	if ( !address || NET_WaitUntilResolved( address, 5000 ) != NET_SUCCESS )
	{
		if ( address ) NET_UnrefAddress( address );
		return false;
	}
	raw.sock = NET_CreateClient( address, g_port, 0 );
	NET_UnrefAddress( address );
	if ( !raw.sock || NET_WaitUntilConnected( raw.sock, 5000 ) != NET_SUCCESS )
	{
		if ( raw.sock ) NET_DestroyStreamSocket( raw.sock );
		raw.sock = nullptr;
		return false;
	}
	size_t before = log.ids.size();
	if ( !PumpUntil( { &log }, [&] {
		for ( size_t i = before; i < log.ids.size(); ++i )
			if ( log.ids[i] == ID_NEW_INCOMING_CONNECTION )
			{
				raw.onServer = log.addrs[i];
				return true;
			}
		return false;
	} ) )
	{
		NET_DestroyStreamSocket( raw.sock );
		raw.sock = nullptr;
		return false;
	}
	return true;
}

static bool SendRaw( RawConn& raw, const std::vector<unsigned char>& bytes )
{
	return raw.sock && NET_WriteToStreamSocket( raw.sock, bytes.data(), (int)bytes.size() );
}

static bool LostSince( const PeerLog& log, size_t before, const SystemAddress& address )
{
	for ( size_t i = before; i < log.ids.size(); ++i )
		if ( log.ids[i] == ID_CONNECTION_LOST && log.addrs[i] == address )
			return true;
	return false;
}

#pragma pack( push, 1 )
struct WirePayload { int a; short b; char name[10]; };
#pragma pack( pop )

int main( int, char** )
{
	SDL_Init( 0 );

	// ---------- 1. handshake: accept + connect events ----------
	RakPeerInterface* srv = RakNetworkFactory::GetRakPeerInterface();
	// A fixed port makes concurrent CI jobs and an immediately repeated test run
	// contend with one another. Pick a high per-run starting point and probe a
	// small range. Startup() is explicitly retry-safe after a bind failure.
	const Uint64 seed = (Uint64)std::chrono::steady_clock::now().time_since_epoch().count();
	bool serverStarted = false;
	for ( unsigned int attempt = 0; attempt < 128 && !serverStarted; ++attempt )
	{
		g_port = (unsigned short)( 40000 + ( seed + attempt ) % 20000 );
		SocketDescriptor sd( g_port, "127.0.0.1" );
		serverStarted = srv->Startup( 4, 30, &sd, 1 );
	}
	CHECK( serverStarted, "server Startup binds listener" );
	if ( !serverStarted )
	{
		RakNetworkFactory::DestroyRakPeerInterface( srv );
		SDL_Quit();
		return 1;
	}
	srv->SetMaximumIncomingConnections( 2 );
	srv->SetOccasionalPing( true );
	srv->SetTimeoutTime( 120000, UNASSIGNED_SYSTEM_ADDRESS );
	REGISTER_STATIC_RPC( srv, srvPING );
	g_relayPeer = srv;

	RakPeerInterface* clA = RakNetworkFactory::GetRakPeerInterface();
	SocketDescriptor sd0;
	CHECK( clA->Startup( 1, 30, &sd0, 1 ), "client A Startup" );
	clA->RegisterAsRemoteProcedureCall( "clPONG", clPONG_A );
	CHECK( clA->Connect( "127.0.0.1", g_port, 0, 0 ), "client A Connect initiated" );

	PeerLog L_srv{ srv }, L_A{ clA };
	const bool handshakeComplete = PumpUntil( { &L_srv, &L_A }, [&] {
		return L_srv.Got( ID_NEW_INCOMING_CONNECTION ) && L_A.Got( ID_CONNECTION_REQUEST_ACCEPTED );
	} );
	CHECK( handshakeComplete, "handshake events on both sides" );
	if ( !handshakeComplete )
	{
		clA->Shutdown( 0 );
		srv->Shutdown( 0 );
		RakNetworkFactory::DestroyRakPeerInterface( clA );
		RakNetworkFactory::DestroyRakPeerInterface( srv );
		SDL_Quit();
		return 1;
	}

	// the wrapper's empty-slot sentinel must stay valid: real peers are nonzero/non-UNASSIGNED
	SystemAddress aOnSrv;
	for ( size_t i = 0; i < L_srv.ids.size(); ++i )
		if ( L_srv.ids[i] == ID_NEW_INCOMING_CONNECTION ) aOnSrv = L_srv.addrs[i];
	CHECK( aOnSrv.binaryAddress != 0 && aOnSrv != UNASSIGNED_SYSTEM_ADDRESS, "client addr nonzero + not UNASSIGNED" );
	SystemAddress serverOnA;
	for ( size_t i = 0; i < L_A.ids.size(); ++i )
		if ( L_A.ids[i] == ID_CONNECTION_REQUEST_ACCEPTED ) serverOnA = L_A.addrs[i];
	CHECK( serverOnA.binaryAddress != 0 && serverOnA != UNASSIGNED_SYSTEM_ADDRESS,
	       "client A retained its accepted server address" );

	// ---------- 2. RPC client->server: bytes, bit count, zero-pad, sender ----------
	WirePayload pay; memset( &pay, 0, sizeof( pay ) );
	pay.a = 0x11223344; pay.b = 0x55; strcpy( pay.name, "merc" );
	clA->RPC( "srvPING", (const char*)&pay, (unsigned int)sizeof( pay ) * 8, HIGH_PRIORITY, RELIABLE, 0,
	          UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 );
	CHECK( PumpUntil( { &L_srv, &L_A }, [&] { return g_capSrv.count >= 1; } ), "RPC reached server handler" );
	CHECK( g_capSrv.bits == sizeof( pay ) * 8, "numberOfBitsOfData exact" );
	CHECK( BytesEqual( g_capSrv.bytes, &pay, sizeof( pay ) ), "payload byte-exact" );
	CHECK( g_capSrv.bytes.size() >= sizeof( pay ) + 2 &&
	       g_capSrv.bytes[sizeof( pay )] == 0 && g_capSrv.bytes[sizeof( pay ) + 1] == 0,
	       "input zero-padded past payload" );
	CHECK( g_capSrv.sender == aOnSrv, "RPCParameters::sender == accept-time address" );
	CHECK( !clA->RPC( "srvPING", nullptr, 8, HIGH_PRIORITY, RELIABLE, 0,
	                  UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 ),
	       "RPC rejects nonempty null payload" );
	CHECK( !clA->RPC( "srvPING", (const char*)&pay, 7, HIGH_PRIORITY, RELIABLE, 0,
	                  UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 ),
	       "RPC rejects non-byte-aligned payload" );
	CHECK( !clA->RPC( "srvPING", (const char*)&pay, 0xffffffffu, HIGH_PRIORITY, RELIABLE, 0,
	                  UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 ),
	       "RPC rejects overflowing/oversized bit length" );
	CHECK( !clA->RPC( "srvPING", (const char*)&pay, 0xfffffff8u, HIGH_PRIORITY, RELIABLE, 0,
	                  UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 ),
	       "RPC rejects byte-aligned body above the full-frame cap" );

	// relay went back out broadcast-except-sender; A must NOT get its own echo
	SDL_Delay( 100 );
	PumpUntil( { &L_srv, &L_A }, [] { return false; }, 150 );
	CHECK( g_capA.count == 0, "broadcast-except-sender excludes the sender" );

	// ---------- 3. second client: relay reaches the OTHER client ----------
	RakPeerInterface* clB = RakNetworkFactory::GetRakPeerInterface();
	CHECK( clB->Startup( 1, 30, &sd0, 1 ), "client B Startup" );
	clB->RegisterAsRemoteProcedureCall( "clPONG", clPONG_B );
	clB->Connect( "127.0.0.1", g_port, 0, 0 );
	PeerLog L_B{ clB };
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return L_B.Got( ID_CONNECTION_REQUEST_ACCEPTED ); } ), "client B connected" );
	SystemAddress serverOnB;
	for ( size_t i = 0; i < L_B.ids.size(); ++i )
		if ( L_B.ids[i] == ID_CONNECTION_REQUEST_ACCEPTED ) serverOnB = L_B.addrs[i];
	CHECK( serverOnB.binaryAddress != 0 && serverOnB != UNASSIGNED_SYSTEM_ADDRESS,
	       "client B retained its accepted server address" );

	g_capSrv = Captured(); g_capA = Captured(); g_capB = Captured();
	clA->RPC( "srvPING", (const char*)&pay, (unsigned int)sizeof( pay ) * 8, HIGH_PRIORITY, RELIABLE, 0,
	          UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 );
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return g_capB.count >= 1; } ), "relay delivered to client B" );
	CHECK( g_capA.count == 0, "relay still excludes sender A" );
	CHECK( BytesEqual( g_capB.bytes, &pay, sizeof( pay ) ), "relayed payload byte-exact" );

	// ---------- 4. targeted send (broadcast=false) ----------
	g_capA = Captured(); g_capB = Captured();
	srv->RPC( "clPONG", (const char*)&pay, (unsigned int)sizeof( pay ) * 8, HIGH_PRIORITY, RELIABLE, 0,
	          aOnSrv, false, 0, UNASSIGNED_NETWORK_ID, 0 );
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return g_capA.count >= 1; } ), "targeted RPC reached A" );
	SDL_Delay( 50 );
	PumpUntil( { &L_srv, &L_A, &L_B }, [] { return false; }, 100 );
	CHECK( g_capB.count == 0, "targeted RPC did not reach B" );

	// ---------- 5. unknown RPC name: dropped silently ----------
	clA->RPC( "noSuchHandler", (const char*)&pay, (unsigned int)sizeof( pay ) * 8, HIGH_PRIORITY, RELIABLE, 0,
	          UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 );
	size_t srvPackets = L_srv.ids.size();
	SDL_Delay( 50 );
	PumpUntil( { &L_srv, &L_A, &L_B }, [] { return false; }, 100 );
	CHECK( L_srv.ids.size() == srvPackets, "unknown RPC produced no user packet" );

	// ---------- 6. server full: third client refused ----------
	RakPeerInterface* clC = RakNetworkFactory::GetRakPeerInterface();
	CHECK( clC->Startup( 1, 30, &sd0, 1 ), "client C Startup" );
	clC->Connect( "127.0.0.1", g_port, 0, 0 );
	PeerLog L_C{ clC };
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B, &L_C }, [&] { return L_C.Got( ID_NO_FREE_INCOMING_CONNECTIONS ); } ),
	       "third client got ID_NO_FREE_INCOMING_CONNECTIONS" );
	clC->Shutdown( 0 );
	RakNetworkFactory::DestroyRakPeerInterface( clC );

	// ---------- 7. big payload (256 KB) framed across many reads ----------
	{
		std::vector<char> big( 256 * 1024 );
		for ( size_t i = 0; i < big.size(); ++i ) big[i] = (char)( i * 31 + 7 );
		g_capSrv = Captured(); g_relayPeer = nullptr;   // no relay for this one
		clA->RPC( "srvPING", big.data(), (unsigned int)big.size() * 8, HIGH_PRIORITY, RELIABLE, 0,
		          UNASSIGNED_SYSTEM_ADDRESS, true, 0, UNASSIGNED_NETWORK_ID, 0 );
		CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return g_capSrv.count >= 1; }, 10000 ), "256KB RPC arrived" );
		CHECK( g_capSrv.bits == big.size() * 8 && BytesEqual( g_capSrv.bytes, big.data(), big.size() ),
		       "256KB payload byte-exact" );
	}

	// ---------- 8. file transfer: 2 files, progress + bytes + completion ----------
	{
		FileListTransfer fltS, fltR;
		srv->AttachPlugin( &fltS );
		srv->AttachPlugin( &fltS );          // wrapper re-attaches on retry: must be idempotent
		clA->AttachPlugin( &fltR );
		clA->SetSplitMessageProgressInterval( 1 );

		FtCap cap;
		unsigned short setID = fltR.SetupReceive( &cap, false, serverOnA );

		std::vector<char> f1( 200 * 1024 );
		for ( size_t i = 0; i < f1.size(); ++i ) f1[i] = (char)( i ^ 0x5A );
		const char* f2 = "tiny file";
		FileList fl;
		fl.AddFile( "Data/Maps/big.dat", f1.data(), (unsigned)f1.size(), (unsigned)f1.size(), FileListNodeContext( 0, 0 ), false );
		fl.AddFile( "Data/tiny.txt", f2, 9, 9, FileListNodeContext( 0, 0 ), false );
		fltS.Send( &fl, srv, aOnSrv, setID, MEDIUM_PRIORITY, 0, false, 0, 5000 );

		CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return cap.complete >= 1; }, 10000 ), "file set completed" );
		CHECK( cap.files == 2, "both files delivered" );
		CHECK( cap.progress >= (int)( f1.size() / 5000 ), "per-chunk progress callbacks fired" );
		CHECK( cap.lastName == "Data/tiny.txt" && BytesEqual( cap.lastData, f2, 9 ), "file order + bytes exact" );
		// empty file set (host sync dir empty) must still complete -- the
		// joining client otherwise hangs forever on the download screen
		FtCap capEmpty;
		unsigned short setE = fltR.SetupReceive( &capEmpty, false, serverOnA );
		FileList flEmpty;
		fltS.Send( &flEmpty, srv, aOnSrv, setE, MEDIUM_PRIORITY, 0, false, 0, 5000 );
		CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return capEmpty.complete >= 1; } ), "empty file set completes immediately" );
		CHECK( capEmpty.files == 0, "empty set delivered zero files" );

		// A zero-length file is distinct from an empty set: it still reports one
		// progress part and one OnFile callback before set completion.
		FtCap capZero;
		unsigned short setZ = fltR.SetupReceive( &capZero, false, serverOnA );
		FileList flZero;
		flZero.AddFile( "Data/empty.dat", nullptr, 0, 0, FileListNodeContext( 0, 0 ), false );
		fltS.Send( &flZero, srv, aOnSrv, setZ, MEDIUM_PRIORITY, 0, false, 0, 5000 );
		CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] { return capZero.complete >= 1; } ),
		       "zero-length file set completes" );
		CHECK( capZero.files == 1 && capZero.progress == 1 && capZero.lastData.empty(),
		       "zero-length file delivers exactly one zero-chunk file" );

		srv->DetachPlugin( &fltS );
		clA->DetachPlugin( &fltR );
	}
	{
		FileListTransfer registrations;
		FtCap cap;
		std::vector<unsigned short> ids;
		for ( unsigned int i = 0; i < 64; ++i )
			ids.push_back( registrations.SetupReceive( &cap, false, UNASSIGNED_SYSTEM_ADDRESS ) );
		bool unique = true;
		for ( size_t i = 0; i < ids.size(); ++i )
			for ( size_t j = i + 1; j < ids.size(); ++j )
				if ( ids[i] == ids[j] || ids[i] == 0xffffu ) unique = false;
		CHECK( unique, "bounded receive registrations use unique live set IDs" );
		CHECK( registrations.SetupReceive( &cap, false, UNASSIGNED_SYSTEM_ADDRESS ) == 0xffffu,
		       "65th pending receive registration is rejected" );
	}
	{
		// RakPeer::Shutdown clears plugin attachment, while the shipped wrapper
		// persists and reattaches the FileListTransfer object on its next session.
		RakPeerInterface* lifecyclePeer = RakNetworkFactory::GetRakPeerInterface();
		SocketDescriptor lifecycleSocket;
		FileListTransfer registrations;
		FtCap cap;
		CHECK( lifecyclePeer->Startup( 1, 30, &lifecycleSocket, 1 ),
		       "receiver lifecycle peer starts" );
		lifecyclePeer->AttachPlugin( &registrations );
		for ( unsigned int i = 0; i < 64; ++i )
			registrations.SetupReceive( &cap, false, UNASSIGNED_SYSTEM_ADDRESS );
		lifecyclePeer->Shutdown( 0 );
		CHECK( lifecyclePeer->Startup( 1, 30, &lifecycleSocket, 1 ),
		       "receiver lifecycle peer restarts" );
		lifecyclePeer->AttachPlugin( &registrations );
		bool reset = true;
		for ( unsigned int i = 0; i < 64; ++i )
			if ( registrations.SetupReceive( &cap, false, UNASSIGNED_SYSTEM_ADDRESS ) == 0xffffu )
				reset = false;
		CHECK( reset && registrations.SetupReceive( &cap, false, UNASSIGNED_SYSTEM_ADDRESS ) == 0xffffu,
		       "Shutdown retires all pending receiver registrations before reattach" );
		lifecyclePeer->DetachPlugin( &registrations );
		lifecyclePeer->Shutdown( 0 );
		RakNetworkFactory::DestroyRakPeerInterface( lifecyclePeer );
	}

	// ---------- 9. graceful disconnect ----------
	// The real peers run in separate processes and pump concurrently. In this
	// single-threaded loopback test, a blocking Shutdown(B) prevents the server
	// from reading B's BYE until after B has torn down its socket, which can turn
	// a graceful notification into an OS-dependent connection-lost event. Start
	// the same graceful close non-blockingly and pump the receiver first.
	size_t before = L_srv.ids.size();
	clB->CloseConnection( serverOnB, true );
	CHECK( PumpUntil( { &L_srv, &L_A, &L_B }, [&] {
		for ( size_t i = before; i < L_srv.ids.size(); ++i )
			if ( L_srv.ids[i] == ID_DISCONNECTION_NOTIFICATION ) return true;
		return false;
	} ), "server saw ID_DISCONNECTION_NOTIFICATION for B" );
	clB->Shutdown( 0 );
	RakNetworkFactory::DestroyRakPeerInterface( clB );

	// ---------- 10. a second peer cannot inject into A's receive set ----------
	RakPeerInterface* clD = RakNetworkFactory::GetRakPeerInterface();
	CHECK( clD->Startup( 1, 30, &sd0, 1 ), "client D Startup" );
	CHECK( clD->Connect( "127.0.0.1", g_port, 0, 0 ), "client D Connect initiated" );
	PeerLog L_D{ clD };
	size_t beforeDOnSrv = L_srv.ids.size();
	CHECK( PumpUntil( { &L_srv, &L_A, &L_D }, [&] {
		bool serverAccepted = false;
		for ( size_t i = beforeDOnSrv; i < L_srv.ids.size(); ++i )
			if ( L_srv.ids[i] == ID_NEW_INCOMING_CONNECTION ) serverAccepted = true;
		return serverAccepted && L_D.Got( ID_CONNECTION_REQUEST_ACCEPTED );
	} ),
	       "client D connected for sender-binding test" );
	SystemAddress serverOnD;
	for ( size_t i = 0; i < L_D.ids.size(); ++i )
		if ( L_D.ids[i] == ID_CONNECTION_REQUEST_ACCEPTED ) serverOnD = L_D.addrs[i];
	SystemAddress dOnSrv;
	for ( size_t i = beforeDOnSrv; i < L_srv.ids.size(); ++i )
		if ( L_srv.ids[i] == ID_NEW_INCOMING_CONNECTION ) dOnSrv = L_srv.addrs[i];
	{
		FileListTransfer receiveA, sendD;
		srv->AttachPlugin( &receiveA );
		clD->AttachPlugin( &sendD );
		FtCap cap;
		unsigned short setID = receiveA.SetupReceive( &cap, false, aOnSrv );
		const char rogueByte = 'x';
		FileList rogue;
		rogue.AddFile( "rogue.dat", &rogueByte, 1, 1, FileListNodeContext( 0, 0 ), false );
		size_t beforeLoss = L_srv.ids.size();
		sendD.Send( &rogue, clD, serverOnD, setID, MEDIUM_PRIORITY, 0, false, 0, 1024 );
		CHECK( PumpUntil( { &L_srv, &L_A, &L_D }, [&] { return LostSince( L_srv, beforeLoss, dOnSrv ); } ),
		       "wrong sender file frame disconnects injecting peer" );
		CHECK( cap.files == 0 && cap.progress == 0 && cap.complete == 0,
		       "wrong sender cannot advance or complete A's transfer" );
		clD->DetachPlugin( &sendD );
		srv->DetachPlugin( &receiveA );
	}
	clD->Shutdown( 0 );
	RakNetworkFactory::DestroyRakPeerInterface( clD );

	// ---------- 11. CloseConnection kick: client sees the drop ----------
	size_t beforeA = L_A.ids.size();
	srv->CloseConnection( aOnSrv, true );
	CHECK( PumpUntil( { &L_A }, [&] {
		for ( size_t i = beforeA; i < L_A.ids.size(); ++i )
			if ( L_A.ids[i] == ID_DISCONNECTION_NOTIFICATION || L_A.ids[i] == ID_CONNECTION_LOST ) return true;
		return false;
	} ), "kicked client A notified" );

	clA->Shutdown( 0 );
	RakNetworkFactory::DestroyRakPeerInterface( clA );

	// ---------- 12. adversarial raw-wire file/control frames ----------
	// These bypass FileListTransfer::Send so malformed lengths and state changes
	// reach the receiver exactly as a hostile TCP peer could encode them.
	auto RunBadFile = [&]( const char* label, auto buildBodies, int expectedFiles )
	{
		RawConn raw;
		bool connected = ConnectRaw( srv, L_srv, raw );
		CHECK( connected, "raw adversarial client connected" );
		if ( !connected ) return;
		FileListTransfer receiver;
		srv->AttachPlugin( &receiver );
		FtCap cap;
		unsigned short setID = receiver.SetupReceive( &cap, false, raw.onServer );
		std::vector<std::vector<unsigned char> > bodies = buildBodies( setID );
		size_t beforeLoss = L_srv.ids.size();
		bool wrote = true;
		for ( const std::vector<unsigned char>& body : bodies )
			wrote = SendRaw( raw, WireFrame( 4, body ) ) && wrote;
		CHECK( wrote, "raw adversarial frames queued" );
		CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ), label );
		CHECK( cap.files == expectedFiles && cap.complete == 0,
		       "rejected file sequence cannot complete or duplicate a file callback" );
		srv->DetachPlugin( &receiver );
		NET_DestroyStreamSocket( raw.sock );
	};

	RunBadFile( "oversized file declaration is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 32u * 1024u * 1024u + 1u, "huge", 32u * 1024u * 1024u + 1u,
			          0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "wrapped/truncated chunk length is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 1, "chunk", 1, 0, std::vector<unsigned char>{ 'x' }, 0xfffffffeu ) };
	}, 0 );
	RunBadFile( "zero-progress chunk for nonempty file is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 1, "stalled", 1, 0, std::vector<unsigned char>() ) };
	}, 0 );
	RunBadFile( "fileIndex outside setCount is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 1, 1, 1, "index", 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "setCount above the file cap is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 4097, 1, "count-cap", 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "setTotal above the byte cap is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 2, 512u * 1024u * 1024u + 1u, "total-cap", 1, 0,
			          std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "non-sequential first fileIndex is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 1, 2, 2, "index", 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "oversized non-terminating file name is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 1, std::string( 512, 'n' ), 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "truncated claimed file name is rejected", []( unsigned short id ) {
		std::vector<unsigned char> body = FileBody( id, 0, 1, 0, "", 0, 0,
		                                                 std::vector<unsigned char>() );
		body[14] = 0xff; body[15] = 0x01;   // claim 511 bytes, provide none
		return std::vector<std::vector<unsigned char> >{ body };
	}, 0 );
	RunBadFile( "embedded-NUL file name alias is rejected", []( unsigned short id ) {
		std::string name( "ab", 2 ); name.push_back( '\0' ); name += "cd";
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 1, name, 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "declared set total must match completed file lengths", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 2, "total", 1, 0, std::vector<unsigned char>{ 'x' } ) };
	}, 0 );
	RunBadFile( "overlapping/duplicate chunk offset is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 3, "overlap", 3, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 1, 3, "overlap", 3, 0, std::vector<unsigned char>{ 'a' } ) };
	}, 0 );
	RunBadFile( "chunk offset hole is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 3, "hole", 3, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 1, 3, "hole", 3, 2, std::vector<unsigned char>{ 'c' } ) };
	}, 0 );
	RunBadFile( "active-file name mutation is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 1, 2, "before", 2, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 1, 2, "after", 2, 1, std::vector<unsigned char>{ 'b' } ) };
	}, 0 );
	RunBadFile( "setCount mutation is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 2, 4, "count", 2, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 3, 4, "count", 2, 1, std::vector<unsigned char>{ 'b' } ) };
	}, 0 );
	RunBadFile( "setTotal mutation is rejected", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 2, 4, "total", 2, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 2, 5, "total", 2, 1, std::vector<unsigned char>{ 'b' } ) };
	}, 0 );
	RunBadFile( "completed fileIndex cannot be replayed", []( unsigned short id ) {
		return std::vector<std::vector<unsigned char> >{
			FileBody( id, 0, 2, 2, "first", 1, 0, std::vector<unsigned char>{ 'a' } ),
			FileBody( id, 0, 2, 2, "first", 1, 0, std::vector<unsigned char>{ 'a' } ) };
	}, 1 );

	// Valid zero-length files remain legal, but completion retires the set before
	// callbacks so a wire replay cannot deliver it twice.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw zero-file client connected" );
		if ( raw.sock )
		{
			FileListTransfer receiver;
			srv->AttachPlugin( &receiver );
			FtCap cap;
			unsigned short setID = receiver.SetupReceive( &cap, false, raw.onServer );
			std::vector<unsigned char> frame = WireFrame( 4,
				FileBody( setID, 0, 1, 0, "zero", 0, 0, std::vector<unsigned char>() ) );
			CHECK( SendRaw( raw, frame ), "raw zero-file frame queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return cap.complete == 1; } ),
			       "raw zero-length file completes" );
			CHECK( cap.files == 1 && cap.progress == 1, "raw zero-length file callback semantics preserved" );
			size_t beforeReplay = L_srv.ids.size();
			CHECK( SendRaw( raw, frame ), "raw completed-set replay queued" );
			PumpUntil( { &L_srv }, [] { return false; }, 100 );
			CHECK( cap.files == 1 && cap.complete == 1 && !LostSince( L_srv, beforeReplay, raw.onServer ),
			       "completed set replay is inert" );
			srv->DetachPlugin( &receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Retiring each completed registration lets the 16-bit allocator wrap without
	// ever overwriting a live entry; 0xffff remains the explicit failure sentinel.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw set-ID-wrap client connected" );
		if ( raw.sock )
		{
			FileListTransfer receiver;
			srv->AttachPlugin( &receiver );
			FtCap cap;
			unsigned int allocated = 0;
			bool idsValid = true;
			while ( allocated < 65535u )
			{
				unsigned int batch = 65535u - allocated;
				if ( batch > 64u ) batch = 64u;
				std::vector<unsigned char> stream;
				for ( unsigned int i = 0; i < batch; ++i )
				{
					unsigned short id = receiver.SetupReceive( &cap, false, raw.onServer );
					if ( id == 0xffffu ) idsValid = false;
					std::vector<unsigned char> frame = WireFrame( 4,
						FileBody( id, 0, 0, 0, "", 0, 0, std::vector<unsigned char>() ) );
					stream.insert( stream.end(), frame.begin(), frame.end() );
				}
				allocated += batch;
				if ( !SendRaw( raw, stream ) ||
				     !PumpUntil( { &L_srv }, [&] { return cap.complete == (int)allocated; } ) )
				{
					idsValid = false;
					break;
				}
			}
			CHECK( idsValid, "set IDs remain usable through the full non-sentinel range" );
			CHECK( receiver.SetupReceive( &cap, false, raw.onServer ) == 0,
			       "set-ID allocator skips 0xffff and wraps to a free zero" );
			srv->DetachPlugin( &receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Callbacks are synchronous and legacy handlers may poll Receive() themselves.
	// Nested Receive must never recurse the socket pump: doing so can deliver the
	// second file before the first OnFile and can sweep the outer parser's Conn.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw callback-reentry client connected" );
		if ( raw.sock )
		{
			FileListTransfer receiver;
			srv->AttachPlugin( &receiver );
			ReentrantFtCap cap;
			cap.peer = srv;
			unsigned short setID = receiver.SetupReceive( &cap, false, raw.onServer );
			std::vector<unsigned char> stream = WireFrame( 4,
				FileBody( setID, 0, 2, 2, "first", 1, 0,
				          std::vector<unsigned char>{ 'a' } ) );
			std::vector<unsigned char> second = WireFrame( 4,
				FileBody( setID, 1, 2, 2, "second", 1, 0,
				          std::vector<unsigned char>{ 'b' } ) );
			stream.insert( stream.end(), second.begin(), second.end() );
			std::vector<unsigned char> bye = WireFrame( 2, std::vector<unsigned char>() );
			stream.insert( stream.end(), bye.begin(), bye.end() );
			CHECK( SendRaw( raw, stream ), "reentrant callback stream queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return cap.events.size() == 5; } ),
			       "reentrant Receive callback stream completes" );
			const std::vector<std::string> expected{
				"progress0", "file0", "progress1", "file1", "complete" };
			CHECK( cap.events == expected && cap.nestedReceives == 2,
			       "nested Receive preserves file callback order and one outer pump" );
			srv->DetachPlugin( &receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// CloseConnection requested from a partial-file callback is flag-only until
	// the outer parser unwinds. The callback's borrowed metadata and file buffer
	// must remain valid for the complete callback invocation.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw callback-close client connected" );
		if ( raw.sock )
		{
			FileListTransfer receiver;
			srv->AttachPlugin( &receiver );
			CloseConnectionFtCap cap;
			cap.peer = srv;
			cap.sender = raw.onServer;
			unsigned short setID = receiver.SetupReceive( &cap, false, raw.onServer );
			std::vector<unsigned char> frame = WireFrame( 4,
				FileBody( setID, 0, 1, 2, "close-during-callback", 2, 0,
				          std::vector<unsigned char>{ 'a' } ) );
			CHECK( SendRaw( raw, frame ), "callback-close partial frame queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return cap.progress == 1; } ),
			       "CloseConnection callback returns through the outer parser" );
			CHECK( cap.borrowedDataSurvived,
			       "CloseConnection preserves borrowed file data through callback return" );
			srv->DetachPlugin( &receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Detaching the persistent FileListTransfer from its own callback follows the
	// same borrowed-storage rule, then retires the partial registration before a
	// later reattach.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw callback-detach client connected" );
		if ( raw.sock )
		{
			FileListTransfer receiver;
			srv->AttachPlugin( &receiver );
			DetachPluginFtCap cap;
			cap.peer = srv;
			cap.plugin = &receiver;
			unsigned short setID = receiver.SetupReceive( &cap, false, raw.onServer );
			std::vector<unsigned char> stream = WireFrame( 4,
				FileBody( setID, 0, 1, 2, "detach-during-callback", 2, 0,
				          std::vector<unsigned char>{ 'b' } ) );
			std::vector<unsigned char> finalFrame = WireFrame( 4,
				FileBody( setID, 0, 1, 2, "detach-during-callback", 2, 1,
				          std::vector<unsigned char>{ 'c' } ) );
			stream.insert( stream.end(), finalFrame.begin(), finalFrame.end() );
			CHECK( SendRaw( raw, stream ), "callback-detach buffered frames queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return cap.progress == 1; } ),
			       "DetachPlugin callback returns through the outer parser" );
			CHECK( cap.borrowedDataSurvived,
			       "DetachPlugin preserves borrowed file data through callback return" );
			PumpUntil( { &L_srv }, [] { return false; }, 100 );
			CHECK( cap.progress == 1,
			       "deferred detach stops buffered file callbacks after the detach request" );
			srv->AttachPlugin( &receiver );
			CHECK( receiver.SetupReceive( &cap, false, raw.onServer ) != 0xffffu,
			       "deferred detach retires the partial registration before reattach" );
			srv->DetachPlugin( &receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Nine simultaneous partial sets exceed the per-plugin active-set bound.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw active-set-cap client connected" );
		if ( raw.sock )
		{
			FileListTransfer receiver;
			srv->AttachPlugin( &receiver );
			FtCap caps[9];
			std::vector<unsigned char> stream;
			for ( unsigned int i = 0; i < 9; ++i )
			{
				unsigned short id = receiver.SetupReceive( &caps[i], false, raw.onServer );
				std::vector<unsigned char> frame = WireFrame( 4,
					FileBody( id, 0, 1, 2, "active", 2, 0, std::vector<unsigned char>{ 'a' } ) );
				stream.insert( stream.end(), frame.begin(), frame.end() );
			}
			size_t beforeLoss = L_srv.ids.size();
			CHECK( SendRaw( raw, stream ), "nine partial sets queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ),
			       "ninth active transfer set is rejected" );
			bool noneComplete = true;
			for ( const FtCap& cap : caps ) if ( cap.files || cap.complete ) noneComplete = false;
			CHECK( noneComplete, "active-set cap cannot synthesize completion" );
			srv->DetachPlugin( &receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Declared totals are reserved separately from the much smaller live file
	// buffers, preventing many cheap partial sets from claiming unbounded state.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw aggregate-reservation client connected" );
		if ( raw.sock )
		{
			FileListTransfer receiver;
			srv->AttachPlugin( &receiver );
			FtCap caps[2];
			unsigned short a = receiver.SetupReceive( &caps[0], false, raw.onServer );
			unsigned short b = receiver.SetupReceive( &caps[1], false, raw.onServer );
			std::vector<unsigned char> stream = WireFrame( 4,
				FileBody( a, 0, 2, 300u * 1024u * 1024u, "reserve-a", 2, 0,
				          std::vector<unsigned char>{ 'a' } ) );
			std::vector<unsigned char> second = WireFrame( 4,
				FileBody( b, 0, 2, 300u * 1024u * 1024u, "reserve-b", 2, 0,
				          std::vector<unsigned char>{ 'b' } ) );
			stream.insert( stream.end(), second.begin(), second.end() );
			size_t beforeLoss = L_srv.ids.size();
			CHECK( SendRaw( raw, stream ), "aggregate reservation frames queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ),
			       "aggregate declared-set cap is enforced" );
			srv->DetachPlugin( &receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// The live buffer sum has its own hard cap. This intentionally allocates the
	// exact 32MiB boundary once; the next one-byte file must be rejected pre-alloc.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw aggregate-buffer client connected" );
		if ( raw.sock )
		{
			FileListTransfer receiver;
			srv->AttachPlugin( &receiver );
			FtCap caps[2];
			unsigned short a = receiver.SetupReceive( &caps[0], false, raw.onServer );
			unsigned short b = receiver.SetupReceive( &caps[1], false, raw.onServer );
			std::vector<unsigned char> first = WireFrame( 4,
				FileBody( a, 0, 2, 32u * 1024u * 1024u + 1u, "buffer-a", 32u * 1024u * 1024u,
				          0, std::vector<unsigned char>{ 'a' } ) );
			CHECK( SendRaw( raw, first ), "32MiB boundary declaration queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return caps[0].progress == 1; } ),
			       "32MiB live-buffer boundary accepted" );
			std::vector<unsigned char> second = WireFrame( 4,
				FileBody( b, 0, 1, 1, "buffer-b", 1, 0, std::vector<unsigned char>{ 'b' } ) );
			size_t beforeLoss = L_srv.ids.size();
			CHECK( SendRaw( raw, second ), "over-cap buffer declaration queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ),
			       "aggregate live-buffer cap is enforced before allocation" );
			srv->DetachPlugin( &receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// The shipped client/server detach persistent FileListTransfer plugins before
	// Shutdown and attach them again on reconnect.  A partial old-session set must
	// release its handler, live buffer, and declared-byte reservation at detach;
	// otherwise this valid new-session reservation is rejected as an aggregate
	// overflow even though the old transport is no longer reachable.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw detach-reset client connected" );
		if ( raw.sock )
		{
			FileListTransfer receiver;
			srv->AttachPlugin( &receiver );
			FtCap oldCap;
			unsigned short oldID = receiver.SetupReceive( &oldCap, false, raw.onServer );
			std::vector<unsigned char> oldFrame = WireFrame( 4,
				FileBody( oldID, 0, 10, 300u * 1024u * 1024u, "old-session", 2, 0,
				          std::vector<unsigned char>{ 'a' } ) );
			CHECK( SendRaw( raw, oldFrame ), "old-session partial set queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return oldCap.progress == 1; } ),
			       "old-session partial set reserves capacity" );

			srv->DetachPlugin( &receiver );
			srv->AttachPlugin( &receiver );
			FtCap newCap;
			unsigned short newID = receiver.SetupReceive( &newCap, false, raw.onServer );
			std::vector<unsigned char> newFrame = WireFrame( 4,
				FileBody( newID, 0, 10, 300u * 1024u * 1024u, "new-session", 2, 0,
				          std::vector<unsigned char>{ 'b' } ) );
			size_t beforeLoss = L_srv.ids.size();
			CHECK( SendRaw( raw, newFrame ), "new-session partial set queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return newCap.progress == 1; } ),
			       "detach retires stale receiver accounting before reattach" );
			CHECK( !LostSince( L_srv, beforeLoss, raw.onServer ) && oldCap.progress == 1,
			       "reattach accepts new reservation without reviving old handler" );
			srv->DetachPlugin( &receiver );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	auto RunBadHeader = [&]( const char* label, unsigned char type, unsigned int bodyLen )
	{
		RawConn raw;
		bool connected = ConnectRaw( srv, L_srv, raw );
		CHECK( connected, "raw control-header client connected" );
		if ( !connected ) return;
		std::vector<unsigned char> header;
		WireU32( header, bodyLen );
		header.push_back( type );
		size_t beforeLoss = L_srv.ids.size();
		CHECK( SendRaw( raw, header ), "malformed control header queued without body" );
		CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ), label );
		NET_DestroyStreamSocket( raw.sock );
	};
	RunBadHeader( "BYE requires an exact empty body", 2, 1 );
	RunBadHeader( "FULL requires an exact empty body", 3, 1 );
	RunBadHeader( "PING requires an exact empty body", 5, 1 );
	RunBadHeader( "unknown frame type is rejected from its header", 99, 0 );
	RunBadHeader( "undersized RPC body is rejected from its header", 1, 0 );
	RunBadHeader( "oversized RPC body is rejected from its header", 1, 1024u * 1024u + 1u );
	RunBadHeader( "undersized file body is rejected from its header", 4, 27 );
	RunBadHeader( "oversized file frame is rejected from its header", 4, 1024u * 1024u + 1u );

	// Reliable/TCP RPC frames cannot be silently discarded when a peer exceeds
	// its work budget: that would leave the two simulations on different event
	// streams. Four fast 300KiB RPCs exceed the 1MiB burst; the first three fit,
	// while the fourth must close the peer instead of disappearing.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw RPC-budget client connected" );
		if ( raw.sock )
		{
			const std::string rpcName = "srvPING";
			std::vector<unsigned char> body;
			body.reserve( 1 + rpcName.size() + 300u * 1024u );
			body.push_back( (unsigned char)rpcName.size() );
			body.insert( body.end(), rpcName.begin(), rpcName.end() );
			body.resize( 1 + rpcName.size() + 300u * 1024u, 0x5a );
			const std::vector<unsigned char> frame = WireFrame( 1, body );
			g_capSrv = Captured();
			bool firstThree = true;
			for ( int expected = 1; expected <= 3; ++expected )
			{
				firstThree = SendRaw( raw, frame ) && firstThree;
				firstThree = PumpUntil( { &L_srv }, [&] { return g_capSrv.count == expected; } ) && firstThree;
			}
			CHECK( firstThree, "first three in-budget reliable RPCs dispatch" );
			size_t beforeLoss = L_srv.ids.size();
			CHECK( SendRaw( raw, frame ), "over-budget reliable RPC queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return LostSince( L_srv, beforeLoss, raw.onServer ); } ),
			       "over-budget reliable RPC disconnects instead of silently dropping state" );
			CHECK( g_capSrv.count == 3,
			       "only in-budget RPCs are dispatched before disconnect" );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	// Shutdown requested by a callback is deferred until the active parser and
	// all callbacks for its completed file have unwound. This is intentionally
	// last because successful deferred shutdown stops the server peer.
	{
		RawConn raw;
		CHECK( ConnectRaw( srv, L_srv, raw ), "raw callback-shutdown client connected" );
		if ( raw.sock )
		{
			FileListTransfer receiver;
			srv->AttachPlugin( &receiver );
			ShutdownFtCap cap;
			cap.peer = srv;
			unsigned short setID = receiver.SetupReceive( &cap, false, raw.onServer );
			std::vector<unsigned char> frame = WireFrame( 4,
				FileBody( setID, 0, 1, 1, "shutdown", 1, 0,
				          std::vector<unsigned char>{ 'x' } ) );
			CHECK( SendRaw( raw, frame ), "callback-shutdown frame queued" );
			CHECK( PumpUntil( { &L_srv }, [&] { return cap.complete == 1; } ),
			       "callback-requested shutdown unwinds safely" );
			CHECK( cap.progress == 1 && cap.files == 1 && cap.complete == 1,
			       "deferred shutdown preserves the current completed-file callbacks" );
			CHECK( srv->Receive() == nullptr,
			       "deferred shutdown completes before outer Receive returns" );
			NET_DestroyStreamSocket( raw.sock );
		}
	}

	srv->Shutdown( 0 );
	RakNetworkFactory::DestroyRakPeerInterface( srv );
	{
		RakPeerInterface* invalidBind = RakNetworkFactory::GetRakPeerInterface();
		SocketDescriptor invalidSocket( g_port, "not a valid bind address" );
		CHECK( !invalidBind->Startup( 1, 30, &invalidSocket, 1 ),
		       "invalid explicit bind address fails instead of exposing all interfaces" );
		RakNetworkFactory::DestroyRakPeerInterface( invalidBind );
	}

	printf( g_failures ? "\n%d FAILURE(S)\n" : "\nALL TESTS PASSED\n", g_failures );
	SDL_Quit();
	return g_failures == 0 ? 0 : 1;
}
