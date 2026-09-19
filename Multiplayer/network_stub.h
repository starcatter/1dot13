#pragma once

#if defined(JA2_USE_SDL3_NET)

#include "netshim/BitStream.h"
#include "netshim/FileListTransfer.h"
#include "netshim/FileListTransferCBInterface.h"
#include "netshim/IncrementalReadInterface.h"
#include "netshim/MessageIdentifiers.h"
#include "netshim/RakNetTypes.h"
#include "netshim/RakNetworkFactory.h"
#include "netshim/RakPeerInterface.h"

#else

typedef unsigned int BitSize_t;
typedef unsigned int RakNetTime;
typedef unsigned short SystemIndex;

namespace RakNet
{
	class BitStream;
}

enum PacketPriority
{
	HIGH_PRIORITY = 1,
	MEDIUM_PRIORITY = 2,
};

enum PacketReliability
{
	RELIABLE = 2,
};

enum
{
	ID_CONNECTION_REQUEST_ACCEPTED = 14,
	ID_CONNECTION_ATTEMPT_FAILED = 15,
	ID_ALREADY_CONNECTED = 16,
	ID_NEW_INCOMING_CONNECTION = 17,
	ID_NO_FREE_INCOMING_CONNECTIONS = 18,
	ID_DISCONNECTION_NOTIFICATION = 19,
	ID_CONNECTION_LOST = 20,
	ID_MODIFIED_PACKET = 24,
	ID_TIMESTAMP = 25,
	ID_REMOTE_DISCONNECTION_NOTIFICATION = 28,
	ID_REMOTE_CONNECTION_LOST = 29,
	ID_REMOTE_NEW_INCOMING_CONNECTION = 30,
};

struct SocketDescriptor
{
	SocketDescriptor();
	SocketDescriptor(unsigned short port, const char* hostAddress, bool isLobbySocket = false);

	unsigned short port;
	char hostAddress[32];
	bool isPS3LobbySocket;
};

struct SystemAddress
{
	unsigned int binaryAddress;
	unsigned short port;

	bool operator==(const SystemAddress& right) const;
	bool operator!=(const SystemAddress& right) const;
	bool operator>(const SystemAddress& right) const;
	bool operator<(const SystemAddress& right) const;
};

const SystemAddress UNASSIGNED_SYSTEM_ADDRESS = { 0xFFFFFFFF, 0xFFFF };

struct NetworkID
{
	NetworkID() : localSystemAddress(65535) {}
	unsigned short localSystemAddress;
};

const NetworkID UNASSIGNED_NETWORK_ID;

struct Packet
{
	SystemIndex systemIndex;
	SystemAddress systemAddress;
	unsigned int length;
	BitSize_t bitSize;
	unsigned char* data;
	bool deleteData;
};

class RakPeerInterface;

struct RPCParameters
{
	unsigned char* input;
	BitSize_t numberOfBitsOfData;
	SystemAddress sender;
	RakPeerInterface* recipient;
	RakNetTime remoteTimestamp;
	char* functionName;
	RakNet::BitStream* replyToSender;
};

class PluginInterface
{
public:
	virtual ~PluginInterface() {}
};

class RakPeerInterface
{
public:
	bool Startup(unsigned short, int, SocketDescriptor*, unsigned int);
	bool Connect(const char*, unsigned short, const char*, int, unsigned int = 0);
	void Shutdown(unsigned int, unsigned char = 0);
	void SetMaximumIncomingConnections(unsigned short);
	void SetOccasionalPing(bool);
	void SetTimeoutTime(RakNetTime, SystemAddress);
	Packet* Receive();
	void DeallocatePacket(Packet*);
	void RegisterAsRemoteProcedureCall(const char*, void (*)(RPCParameters*));
	bool RPC(const char*, const char*, BitSize_t, PacketPriority, PacketReliability,
		char, SystemAddress, bool, RakNetTime*, NetworkID, RakNet::BitStream*);
	void CloseConnection(SystemAddress, bool, unsigned char = 0);
	void AttachPlugin(PluginInterface*);
	void DetachPlugin(PluginInterface*);
	void SetSplitMessageProgressInterval(int);
};

class RakNetworkFactory
{
public:
	static RakPeerInterface* GetRakPeerInterface();
	static void DestroyRakPeerInterface(RakPeerInterface*);
};

struct FileListNodeContext
{
	FileListNodeContext() : op(0), fileId(0) {}
	FileListNodeContext(unsigned char operation, unsigned int id) : op(operation), fileId(id) {}

	unsigned char op;
	unsigned int fileId;
};

class FileList;

class FileListProgress
{
public:
	virtual ~FileListProgress() {}
	virtual void OnFilePush(const char*, unsigned int, unsigned int, unsigned int, bool, SystemAddress) {}
};

class FileListTransferCBInterface
{
public:
	struct OnFileStruct
	{
		unsigned int fileIndex;
		char fileName[512];
		char* fileData;
		unsigned int compressedTransmissionLength;
		BitSize_t finalDataLength;
		unsigned short setID;
		unsigned int setCount;
		unsigned int setTotalCompressedTransmissionLength;
		unsigned int setTotalFinalLength;
		FileListNodeContext context;
	};

	virtual ~FileListTransferCBInterface() {}
	virtual bool OnFile(OnFileStruct*) = 0;
	virtual void OnFileProgress(OnFileStruct*, unsigned int, unsigned int, unsigned int, char*) {}
	virtual bool Update() { return true; }
	virtual bool OnDownloadComplete() { return false; }
	virtual void OnDereference() {}
};

class IncrementalReadInterface
{
public:
	virtual ~IncrementalReadInterface() {}
	virtual unsigned int GetFilePart(char*, unsigned int, unsigned int, void*, FileListNodeContext);
};

class FileList
{
public:
	void Clear();
	void AddFile(const char*, const char*, unsigned int, unsigned int, FileListNodeContext, bool = false);
};

class FileListTransfer : public PluginInterface
{
public:
	unsigned short SetupReceive(FileListTransferCBInterface*, bool, SystemAddress);
	void Send(FileList*, RakPeerInterface*, SystemAddress, unsigned short, PacketPriority,
		char, bool, IncrementalReadInterface* = 0, unsigned int = 8388608);
	void SetCallback(FileListProgress*);
};

#define REGISTER_STATIC_RPC(networkObject, functionName) \
	(networkObject)->RegisterAsRemoteProcedureCall((#functionName), (functionName))

#endif
