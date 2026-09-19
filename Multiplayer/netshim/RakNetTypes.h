// netshim: RakNet 3.401 API-compat types, implemented over SDL3_net (netshim.cpp).
// Only the surface the JA2 MP wrapper (client.cpp/server.cpp) actually uses.
#pragma once

typedef unsigned int  BitSize_t;
typedef unsigned int  RakNetTime;   // 4 bytes, matching RakNet 3.401 (not unsigned long!)

enum PacketPriority { SYSTEM_PRIORITY = 0, HIGH_PRIORITY, MEDIUM_PRIORITY, LOW_PRIORITY, NUMBER_OF_PRIORITIES };
enum PacketReliability { UNRELIABLE = 0, UNRELIABLE_SEQUENCED, RELIABLE, RELIABLE_ORDERED, RELIABLE_SEQUENCED };

struct SystemAddress
{
	// Kept binary-compatible in spirit with RakNet 3.x: the server's client-slot
	// logic compares ONLY these two fields (f_rec_num) and uses binaryAddress==0
	// as the empty-slot sentinel. The shim guarantees real connections always get
	// a nonzero binaryAddress and a unique (binaryAddress, port) pair.
	unsigned int   binaryAddress;
	unsigned short port;

	SystemAddress() : binaryAddress( 0 ), port( 0 ) {}
	SystemAddress( unsigned int addr, unsigned short p ) : binaryAddress( addr ), port( p ) {}
	bool operator==( const SystemAddress& o ) const { return binaryAddress == o.binaryAddress && port == o.port; }
	bool operator!=( const SystemAddress& o ) const { return !( *this == o ); }
	bool operator<( const SystemAddress& o ) const
	{ return binaryAddress < o.binaryAddress || ( binaryAddress == o.binaryAddress && port < o.port ); }
};
inline const SystemAddress UNASSIGNED_SYSTEM_ADDRESS( 0xFFFFFFFFu, 0xFFFFu );

struct NetworkID
{
	bool operator==( const NetworkID& ) const { return true; }
};
inline const NetworkID UNASSIGNED_NETWORK_ID;

struct SocketDescriptor
{
	SocketDescriptor() : port( 0 ) { hostAddress[0] = 0; }
	SocketDescriptor( unsigned short _port, const char* _hostAddress ) : port( _port )
	{
		hostAddress[0] = 0;
		if ( _hostAddress )
		{
			unsigned int i = 0;
			for ( ; _hostAddress[i] && i < sizeof( hostAddress ) - 1; ++i ) hostAddress[i] = _hostAddress[i];
			hostAddress[i] = 0;
		}
	}
	unsigned short port;
	char hostAddress[32];
};

struct Packet
{
	SystemAddress  systemAddress;
	unsigned int   length;     // bytes
	BitSize_t      bitSize;
	unsigned char* data;       // data[0] is the ID_* message identifier
};

class RakPeerInterface;
namespace RakNet { class BitStream; }

struct RPCParameters
{
	unsigned char*    input;               // payload bytes, zero-padded past the end
	BitSize_t         numberOfBitsOfData;
	SystemAddress     sender;
	RakPeerInterface* recipient;           // never read by the wrapper
	RakNet::BitStream* replyToSender;      // never read by the wrapper
};

// PluginInterface: minimal base so AttachPlugin/DetachPlugin work; FileListTransfer
// identifies itself via NetShimPluginId (avoids an RTTI dependency).
class PluginInterface
{
public:
	virtual ~PluginInterface() {}
	virtual int NetShimPluginId() const { return 0; }
};

#define REGISTER_STATIC_RPC( networkObject, functionName ) \
	( networkObject )->RegisterAsRemoteProcedureCall( ( #functionName ), ( functionName ) )
