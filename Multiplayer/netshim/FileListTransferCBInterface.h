// netshim: RakNet 3.401 file-transfer callback compat. Live wrapper uses:
// OnFile{fileName,fileData,finalDataLength}, OnFileProgress(.,partCount,partTotal,
// partLength,firstDataChunk), OnDownloadComplete. Other fields exist so the
// commented-out debug lines in client.cpp still compile if revived.
#pragma once

class FileListTransferCBInterface
{
public:
	struct OnFileStruct
	{
		unsigned       fileIndex;
		unsigned short setID;
		unsigned       setCount;
		unsigned       setTotalCompressedTransmissionLength;
		unsigned       setTotalFinalLength;
		unsigned       compressedTransmissionLength;
		unsigned       finalDataLength;
		char           fileName[512];
		char*          fileData;
		OnFileStruct() : fileIndex( 0 ), setID( 0 ), setCount( 0 ),
			setTotalCompressedTransmissionLength( 0 ), setTotalFinalLength( 0 ),
			compressedTransmissionLength( 0 ), finalDataLength( 0 ), fileData( 0 ) { fileName[0] = 0; }
	};

	virtual ~FileListTransferCBInterface() {}
	// Return value kept for API compat; the shim always retains ownership of
	// fileData and frees it after the call.
	virtual bool OnFile( OnFileStruct* onFileStruct ) = 0;
	virtual void OnFileProgress( OnFileStruct*, unsigned int, unsigned int, unsigned int, char* ) {}
	virtual bool OnDownloadComplete( void ) { return false; }
};
