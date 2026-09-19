// netshim: RakNet 3.401 FileListTransfer/FileList compat over the netshim frame
// protocol (netshim.cpp). Used by the MP wrapper for host->client map/data sync.
#pragma once
#include <vector>
#include <string>
#include "RakNetTypes.h"
#include "FileListTransferCBInterface.h"

struct FileListNodeContext
{
	unsigned char op;
	unsigned int  fileId;
	FileListNodeContext() : op( 0 ), fileId( 0 ) {}
	FileListNodeContext( unsigned char _op, unsigned int _fileId ) : op( _op ), fileId( _fileId ) {}
};

class FileList
{
public:
	struct FileEntry
	{
		std::string       filename;
		std::vector<char> data;
		unsigned          fileLength;
	};
	void AddFile( const char* filename, const char* data, const unsigned dataLength, const unsigned fileLength, FileListNodeContext context, bool isAReference = false );
	void Clear( void );
	std::vector<FileEntry> files;
};

// Progress callback the server installs (OnFilePush per outgoing chunk).
class FileListProgress
{
public:
	virtual ~FileListProgress() {}
	virtual void OnFilePush( const char*, unsigned int, unsigned int, unsigned int, bool, SystemAddress ) {}
};

class IncrementalReadInterface;
class RakPeerInterface;
struct NetShimFltState;

class FileListTransfer : public PluginInterface
{
public:
	FileListTransfer();
	~FileListTransfer();
	int NetShimPluginId() const override { return 1; }

	// Receiver side: allocate a set id and remember the callback.
	unsigned short SetupReceive( FileListTransferCBInterface* handler, bool deleteHandler, SystemAddress allowedSender );
	// Sender side: stream the file list to recipient, tagged with setID.
	void Send( FileList* fileList, RakPeerInterface* rakPeer, SystemAddress recipient, unsigned short setID,
	           PacketPriority priority, char orderingChannel, bool compressData,
	           IncrementalReadInterface* incrementalReadInterface = 0, unsigned int chunkSize = 262144 );
	void SetCallback( FileListProgress* callback );

	NetShimFltState* state;   // shim internals (netshim.cpp)
};
