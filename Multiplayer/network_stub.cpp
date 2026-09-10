#include "network_stub.h"

#include <cstring>

bool IsMultiplayerAvailable()
{
	return false;
}

SocketDescriptor::SocketDescriptor()
	: port(0), isPS3LobbySocket(false)
{
	hostAddress[0] = '\0';
}

SocketDescriptor::SocketDescriptor(unsigned short newPort, const char* newHostAddress, bool lobbySocket)
	: port(newPort), isPS3LobbySocket(lobbySocket)
{
	hostAddress[0] = '\0';
	if (newHostAddress)
	{
		std::strncpy(hostAddress, newHostAddress, sizeof(hostAddress) - 1);
		hostAddress[sizeof(hostAddress) - 1] = '\0';
	}
}

bool SystemAddress::operator==(const SystemAddress& right) const
{
	return binaryAddress == right.binaryAddress && port == right.port;
}

bool SystemAddress::operator!=(const SystemAddress& right) const
{
	return !(*this == right);
}

bool SystemAddress::operator>(const SystemAddress& right) const
{
	return binaryAddress > right.binaryAddress ||
		(binaryAddress == right.binaryAddress && port > right.port);
}

bool SystemAddress::operator<(const SystemAddress& right) const
{
	return binaryAddress < right.binaryAddress ||
		(binaryAddress == right.binaryAddress && port < right.port);
}

bool RakPeerInterface::Startup(unsigned short, int, SocketDescriptor*, unsigned int)
{
	return false;
}

bool RakPeerInterface::Connect(const char*, unsigned short, const char*, int, unsigned int)
{
	return false;
}

void RakPeerInterface::Shutdown(unsigned int, unsigned char)
{
}

void RakPeerInterface::SetMaximumIncomingConnections(unsigned short)
{
}

void RakPeerInterface::SetOccasionalPing(bool)
{
}

void RakPeerInterface::SetTimeoutTime(RakNetTime, SystemAddress)
{
}

Packet* RakPeerInterface::Receive()
{
	return 0;
}

void RakPeerInterface::DeallocatePacket(Packet*)
{
}

void RakPeerInterface::RegisterAsRemoteProcedureCall(const char*, void (*)(RPCParameters*))
{
}

bool RakPeerInterface::RPC(const char*, const char*, BitSize_t, PacketPriority, PacketReliability,
	char, SystemAddress, bool, RakNetTime*, NetworkID, RakNet::BitStream*)
{
	return false;
}

void RakPeerInterface::CloseConnection(SystemAddress, bool, unsigned char)
{
}

void RakPeerInterface::AttachPlugin(PluginInterface*)
{
}

void RakPeerInterface::DetachPlugin(PluginInterface*)
{
}

void RakPeerInterface::SetSplitMessageProgressInterval(int)
{
}

RakPeerInterface* RakNetworkFactory::GetRakPeerInterface()
{
	static RakPeerInterface unavailablePeer;
	return &unavailablePeer;
}

void RakNetworkFactory::DestroyRakPeerInterface(RakPeerInterface*)
{
}

unsigned short FileListTransfer::SetupReceive(FileListTransferCBInterface*, bool, SystemAddress)
{
	return 65535;
}

void FileListTransfer::Send(FileList*, RakPeerInterface*, SystemAddress, unsigned short,
	PacketPriority, char, bool, IncrementalReadInterface*, unsigned int)
{
}

void FileListTransfer::SetCallback(FileListProgress*)
{
}

void FileList::Clear()
{
}

void FileList::AddFile(const char*, const char*, unsigned int, unsigned int, FileListNodeContext, bool)
{
}

unsigned int IncrementalReadInterface::GetFilePart(char*, unsigned int, unsigned int, void*, FileListNodeContext)
{
	return 0;
}
