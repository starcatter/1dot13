// netshim: RakNet 3.401 RakPeerInterface compat, implemented over SDL3_net TCP
// stream sockets (netshim.cpp). Exactly the 14 methods the JA2 MP wrapper calls.
// Semantics preserved from RakNet 3.x:
//  - Receive() performs the socket I/O AND synchronously dispatches registered
//    RPC handlers in-place; RPC packets are never returned to the caller.
//  - RPC broadcast=true + UNASSIGNED_SYSTEM_ADDRESS  -> everyone
//    RPC broadcast=true + addr                       -> everyone EXCEPT addr
//    RPC broadcast=false + addr                      -> only addr
//  - Handler input buffers are zero-padded (wrapper atoi/wcscpy's wire data).
//  - AttachPlugin is idempotent (the wrapper re-attaches on every retry).
#pragma once
#include "RakNetTypes.h"

struct NetShimPeerState;

class RakPeerInterface
{
public:
	RakPeerInterface();
	~RakPeerInterface();

	bool Startup( unsigned short maxConnections, int threadSleepTimer, SocketDescriptor* socketDescriptors, unsigned socketDescriptorCount );
	bool Connect( const char* host, unsigned short remotePort, const char* passwordData, int passwordDataLength );
	void Shutdown( unsigned int blockDuration, unsigned char orderingChannel = 0 );

	Packet* Receive( void );
	void DeallocatePacket( Packet* packet );

	bool RegisterAsRemoteProcedureCall( const char* uniqueID, void ( *functionPointer )( RPCParameters* rpcParms ) );
	bool RPC( const char* uniqueID, const char* data, BitSize_t bitLength,
	          PacketPriority priority, PacketReliability reliability, char orderingChannel,
	          SystemAddress systemAddress, bool broadcast, RakNetTime* includedTimestamp,
	          NetworkID networkID, RakNet::BitStream* replyFromTarget );

	void SetMaximumIncomingConnections( unsigned short numberAllowed );
	void SetOccasionalPing( bool doPing );
	void SetTimeoutTime( RakNetTime timeMS, const SystemAddress target );
	void CloseConnection( const SystemAddress target, bool sendDisconnectionNotification, unsigned char orderingChannel = 0 );

	void AttachPlugin( PluginInterface* plugin );
	void DetachPlugin( PluginInterface* plugin );
	void SetSplitMessageProgressInterval( int interval );

	NetShimPeerState* state;   // shim internals (netshim.cpp)
};
