// netshim: RakNet 3.401 API-compat header. The wrapper only uses BitStream as an
// always-NULL RPC argument type; the two statics appear in commented-out code.
#pragma once
namespace RakNet
{
	class BitStream
	{
	public:
		static bool DoEndianSwap( void );
		static void ReverseBytesInPlace( unsigned char* data, unsigned int length );
	};
}
