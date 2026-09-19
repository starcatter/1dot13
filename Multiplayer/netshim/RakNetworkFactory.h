// netshim: RakNet 3.401 API-compat header, implemented in netshim.cpp over SDL3_net.
#pragma once
class RakPeerInterface;
class RakNetworkFactory
{
public:
	static RakPeerInterface* GetRakPeerInterface( void );
	static void DestroyRakPeerInterface( RakPeerInterface* i );
};
