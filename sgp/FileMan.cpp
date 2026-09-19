//**************************************************************************
//
// Filename :	FileMan.c
//
//	Purpose :	function definitions for the memory manager
//
// Modification history :
//
//		24sep96:HJH		->creation
//	08Apr97:ARM	->Assign return value from Push() calls back to HStack
//					 handle, because it may possibly do a MemRealloc()
//		29Dec97:Kris Morness 
//									->Added functionality for setting file attributes which
//									allows for read-only attribute overriding
//									->Also added a simple function that clears all file attributes
//										to normal.
//
//		5 Feb 98:Dave French->extensive modification to support libraries
//
//**************************************************************************

//**************************************************************************
//
//				Includes
//
//**************************************************************************
#include "FileMan.h"

#include "DEBUG.H"
#include "fileio/FileIO.h"
#include "fileio/FileServices.h"
#include "fileio/PlatformPaths.h"
#include "fileio/StoreRouter.h"
#ifdef _WIN32
#include "sgp_logger.h"
#endif

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using namespace std;

struct SOperation
{
	enum EOperation
	{
		UNKNOWN, READ, WRITE,
	};
	EOperation op;
	SOperation() : op(UNKNOWN) {};
};

namespace
{
	void LogFileManagerError(const char* message)
	{
#ifdef _WIN32
		SGP_ERROR(message);
#else
		std::fprintf(stderr, "FileMan: %s\n", message);
#endif
	}

	constexpr UINT32 FILE_HANDLE_INDEX_MASK = 0xffff;
	constexpr UINT32 FILE_HANDLE_GENERATION_SHIFT = 16;

	struct FileHandleEntry
	{
		std::unique_ptr<ja2::fileio::File> ownedFile;
		ja2::fileio::File* borrowedFile = nullptr;
		SOperation::EOperation operation = SOperation::UNKNOWN;
		UINT16 generation = 1;

		ja2::fileio::File* file() const
		{
			return ownedFile ? ownedFile.get() : borrowedFile;
		}
	};

	std::vector<FileHandleEntry> gFileHandles;
	std::recursive_mutex gFileHandlesMutex;

	HWFILE RegisterFileHandle(std::unique_ptr<ja2::fileio::File> file,
		SOperation::EOperation operation)
	{
		if (!file)
		{
			return 0;
		}
		for (size_t index = 0; index < gFileHandles.size(); ++index)
		{
			FileHandleEntry& entry = gFileHandles[index];
			if (entry.file() == nullptr)
			{
				entry.ownedFile = std::move(file);
				entry.operation = operation;
				return (static_cast<UINT32>(entry.generation) << FILE_HANDLE_GENERATION_SHIFT) |
					static_cast<UINT32>(index + 1);
			}
		}

		if (gFileHandles.size() >= FILE_HANDLE_INDEX_MASK)
		{
			return 0;
		}

		gFileHandles.emplace_back();
		FileHandleEntry& entry = gFileHandles.back();
		entry.ownedFile = std::move(file);
		entry.operation = operation;
		return (static_cast<UINT32>(entry.generation) << FILE_HANDLE_GENERATION_SHIFT) |
			static_cast<UINT32>(gFileHandles.size());
	}

	HWFILE RegisterBorrowedFile(ja2::fileio::File& file, SOperation::EOperation operation)
	{
		for (size_t index = 0; index < gFileHandles.size(); ++index)
		{
			FileHandleEntry& entry = gFileHandles[index];
			if (entry.file() == nullptr)
			{
				entry.borrowedFile = &file;
				entry.operation = operation;
				return (static_cast<UINT32>(entry.generation) << FILE_HANDLE_GENERATION_SHIFT) |
					static_cast<UINT32>(index + 1);
			}
		}

		if (gFileHandles.size() >= FILE_HANDLE_INDEX_MASK)
		{
			return 0;
		}

		gFileHandles.emplace_back();
		FileHandleEntry& entry = gFileHandles.back();
		entry.borrowedFile = &file;
		entry.operation = operation;
		return (static_cast<UINT32>(entry.generation) << FILE_HANDLE_GENERATION_SHIFT) |
			static_cast<UINT32>(gFileHandles.size());
	}

	FileHandleEntry* GetFileHandleEntry(HWFILE handle)
	{
		const UINT32 encodedIndex = handle & FILE_HANDLE_INDEX_MASK;
		if (encodedIndex == 0)
		{
			return nullptr;
		}

		const size_t index = encodedIndex - 1;
		if (index >= gFileHandles.size())
		{
			return nullptr;
		}

		FileHandleEntry& entry = gFileHandles[index];
		const UINT16 generation = static_cast<UINT16>(handle >> FILE_HANDLE_GENERATION_SHIFT);
		if (entry.file() == nullptr || entry.generation != generation)
		{
			return nullptr;
		}
		return &entry;
	}

	void ReleaseFileHandleEntry(FileHandleEntry& entry)
	{
		entry.ownedFile.reset();
		entry.borrowedFile = nullptr;
		entry.operation = SOperation::UNKNOWN;
		if (++entry.generation == 0)
		{
			entry.generation = 1;
		}
	}

	struct FileSearchEntry
	{
		std::vector<ja2::fileio::DirectoryEntry> entries;
		size_t next = 0;
		UINT16 generation = 1;
		bool active = false;
	};

	std::vector<std::unique_ptr<FileSearchEntry>> gFileSearches;

	INT32 RegisterFileSearch(std::vector<ja2::fileio::DirectoryEntry> entries)
	{
		for (size_t index = 0; index < gFileSearches.size(); ++index)
		{
			FileSearchEntry& entry = *gFileSearches[index];
			if (!entry.active)
			{
				entry.entries = std::move(entries);
				entry.next = 1;
				entry.active = true;
				return static_cast<INT32>(
					(static_cast<UINT32>(entry.generation) << FILE_HANDLE_GENERATION_SHIFT) |
					static_cast<UINT32>(index + 1));
			}
		}

		if (gFileSearches.size() >= FILE_HANDLE_INDEX_MASK)
		{
			return 0;
		}

		gFileSearches.emplace_back(std::make_unique<FileSearchEntry>());
		FileSearchEntry& entry = *gFileSearches.back();
		entry.entries = std::move(entries);
		entry.next = 1;
		entry.active = true;
		return static_cast<INT32>(
			(static_cast<UINT32>(entry.generation) << FILE_HANDLE_GENERATION_SHIFT) |
			static_cast<UINT32>(gFileSearches.size()));
	}

	FileSearchEntry* GetFileSearchEntry(INT32 handle)
	{
		const UINT32 unsignedHandle = static_cast<UINT32>(handle);
		const UINT32 encodedIndex = unsignedHandle & FILE_HANDLE_INDEX_MASK;
		if (encodedIndex == 0)
		{
			return nullptr;
		}

		const size_t index = encodedIndex - 1;
		if (index >= gFileSearches.size())
		{
			return nullptr;
		}

		FileSearchEntry& entry = *gFileSearches[index];
		const UINT16 generation = static_cast<UINT16>(unsignedHandle >> FILE_HANDLE_GENERATION_SHIFT);
		if (!entry.active || entry.generation != generation)
		{
			return nullptr;
		}
		return &entry;
	}

	void ReleaseFileSearchEntry(FileSearchEntry& entry)
	{
		entry.entries.clear();
		entry.next = 0;
		entry.active = false;
		if (++entry.generation == 0)
		{
			entry.generation = 1;
		}
	}

	std::string LeafName(const std::string& name)
	{
		const size_t separator = name.find_last_of("/\\");
		return separator == std::string::npos ? name : name.substr(separator + 1);
	}

	void FillGetFileStruct(GETFILESTRUCT& output, const ja2::fileio::DirectoryEntry& entry)
	{
		const std::string name = LeafName(entry.name);
		const size_t maximumSize = sizeof(output.zFileName) - 1;
		const size_t size = (std::min)(name.size(), maximumSize);
		memcpy(output.zFileName, name.data(), size);
		output.zFileName[size] = 0;
		output.uiFileSize = static_cast<UINT32>((std::min)(entry.metadata.size,
			static_cast<std::uint64_t>((std::numeric_limits<UINT32>::max)())));
		output.uiFileAttribs = entry.metadata.directory ? FILE_IS_DIRECTORY :
			(entry.metadata.readOnly ? FILE_IS_READONLY : FILE_IS_NORMAL);
	}

	void NormalizeDirectoryEntryNames(std::vector<ja2::fileio::DirectoryEntry>& entries)
	{
		for (ja2::fileio::DirectoryEntry& entry : entries)
		{
			entry.name = LeafName(entry.name);
		}
	}
}

//**************************************************************************
//
//				Defines
//
//**************************************************************************

#define CHECKF(exp)	if (!(exp)) { return(FALSE); }
#define CHECKV(exp)	if (!(exp)) { return; }
#define CHECKN(exp)	if (!(exp)) { return(NULL); }
#define CHECKBI(exp) if (!(exp)) { return(-1); }

//**************************************************************************
//
//				Variables
//
//**************************************************************************

//**************************************************************************
//
//				Function Prototypes
//
//**************************************************************************

//**************************************************************************
//
//				Functions
//
//**************************************************************************

//**************************************************************************
//
// FileSystemInit
//
//		Starts up the file system.
//
// Parameter List :
// Return Value :
// Modification history :
//
//		24sep96:HJH		->creation
//
//**************************************************************************
BOOLEAN	InitializeFileManager(	STR strIndexFilename )
{
	(void)strIndexFilename;
	RegisterDebugTopic( TOPIC_FILE_MANAGER, "File Manager" );
	return( TRUE );
}



//**************************************************************************
//
// FileSystemShutdown
//
//		Shuts down the file system.
//
// Parameter List :
// Return Value :
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

void ShutdownFileManager( void )
{
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	for (FileHandleEntry& entry : gFileHandles)
	{
		if (entry.file() != nullptr)
		{
			ReleaseFileHandleEntry(entry);
		}
	}
	for (const std::unique_ptr<FileSearchEntry>& search : gFileSearches)
	{
		if (search->active)
		{
			ReleaseFileSearchEntry(*search);
		}
	}
	UnRegisterDebugTopic( TOPIC_FILE_MANAGER, "File Manager" );
}


//**************************************************************************
//
// FileExists
//
//		Checks if a file exists.
//
// Parameter List :
//
//		STR	->name of file to check existence of
//
// Return Value :
//
//		BOOLEAN	->TRUE if it exists
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//	Oct 2005: Snap - Rewrote, made to check data catalogues
//
//**************************************************************************
BOOLEAN	FileExists( STR strFilename )
{
	if (strFilename == nullptr)
	{
		return FALSE;
	}
	try
	{
		return ja2::fileio::fileServicesInitialized() &&
			ja2::fileio::storeRouter().exists(strFilename) ? TRUE : FALSE;
	}
	catch (...)
	{
		return FALSE;
	}
}

//**************************************************************************
//
// FileExistsNoDB
//
//		Checks if a file exists, but doesn't check the database files.
//
// Parameter List :
//
//		STR	->name of file to check existence of
//
// Return Value :
//
//		BOOLEAN	->TRUE if it exists
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//	Oct 2005: Snap - Rewrote, made to check data catalogues
//
//**************************************************************************
extern BOOLEAN	FileExistsNoDB( STR strFilename )
{
	return FileExists(strFilename);
}

//**************************************************************************
//
// FileDelete
//
//		Deletes a file.
//
// Parameter List :
//
//		STR	->name of file to delete
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//**************************************************************************	
BOOLEAN	FileDelete( STR strFilename )
{
	if (strFilename == nullptr)
	{
		return FALSE;
	}
	try
	{
		if (!ja2::fileio::fileServicesInitialized()) return FALSE;
		ja2::fileio::storeRouter().remove(strFilename);
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
}

//**************************************************************************
//
// FileOpen
//
//		Opens a file.
//
// Parameter List :
//
//		STR	->filename
//		UIN32		->access - read or write, or both
//		BOOLEAN	->delete on close
//
// Return Value :
//
//		HWFILE	->handle of opened file
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//	Oct 2005: Snap - modified to work with the custom Data directory
//
//**************************************************************************
HWFILE FileOpen( STR strFilename, UINT32 uiOptions, BOOLEAN fDeleteOnClose, STR strProfilename )//dnl ch81 021213
{
	(void)fDeleteOnClose;
	if (strFilename == nullptr || !ja2::fileio::fileServicesInitialized())
	{
		return 0;
	}
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	try
	{
		if(uiOptions & FILE_ACCESS_WRITE)
		{
			return RegisterFileHandle(ja2::fileio::storeRouter().openWrite(strFilename),
				SOperation::WRITE);
		}
		else if(uiOptions & FILE_ACCESS_READ)
		{
			if(strProfilename && strProfilename[0])
			{
				return RegisterFileHandle(
					ja2::fileio::storeRouter().resourceStore().openFromProfile(
						strFilename, strProfilename), SOperation::READ);
			}
			return RegisterFileHandle(ja2::fileio::storeRouter().openRead(strFilename),
				SOperation::READ);
		}
	}
	// sometimes a file is supposed to opened that does not exist (not tested with FileExists())
	// this operation can fail with an exception that the calling code doesn't catch
	// instead we catch it (any exception, not just CBasicException) here and return 0
	catch(const std::exception& ex) { LogFileManagerError(ex.what()); }
	catch(...)
	{
		LogFileManagerError("Caught undefined exception");
	}
	return 0;
}

BorrowedFileHandle::BorrowedFileHandle(ja2::fileio::File& file, UINT32 uiOptions) : handle_(0)
{
	const SOperation::EOperation operation = (uiOptions & FILE_ACCESS_WRITE) ?
		SOperation::WRITE : ((uiOptions & FILE_ACCESS_READ) ? SOperation::READ : SOperation::UNKNOWN);
	if (operation == SOperation::UNKNOWN)
	{
		return;
	}
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	handle_ = RegisterBorrowedFile(file, operation);
}

BorrowedFileHandle::~BorrowedFileHandle()
{
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileHandleEntry* entry = GetFileHandleEntry(handle_);
	if (entry != nullptr && entry->borrowedFile != nullptr)
	{
		ReleaseFileHandleEntry(*entry);
	}
}

//**************************************************************************
//
// FileClose
//
//
// Parameter List :
//
//		HWFILE hFile	->handle to file to close
//
// Return Value :
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************
void FileClose( HWFILE hFile )
{
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileHandleEntry* entry = GetFileHandleEntry(hFile);
	if(entry != nullptr)
	{
		ReleaseFileHandleEntry(*entry);
	}
}

//**************************************************************************
//
// FileRead
//
//		To read a file.
//
// Parameter List :
//
//		HWFILE		->handle to file to read from
//		void	*	->source buffer
//		UINT32	->num bytes to read
//		UINT32	->num bytes read
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//		08Dec97:ARM		->return FALSE if bytes to read != bytes read
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

#ifdef JA2TESTVERSION
	extern UINT32 uiTotalFileReadTime;
	extern UINT32 uiTotalFileReadCalls;
	#include "Timer Control.h"

class TimeCounter
{
public:
	TimeCounter() : start_time(GetJA2Clock()) {}
	~TimeCounter()
	{
		uiTotalFileReadTime += GetJA2Clock() - start_time;
		uiTotalFileReadCalls++;
	}
private:
	UINT32 start_time;
};

#endif

BOOLEAN FileRead( HWFILE hFile, PTR pDest, UINT32 uiBytesToRead, UINT32 *puiBytesRead )
{
#ifdef JA2TESTVERSION
	TimeCounter timer;
#endif
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileHandleEntry* entry = GetFileHandleEntry(hFile);
	if(entry != nullptr && entry->operation == SOperation::READ)
	{
		UINT32 uiBytesRead = 0;
		try
		{
			uiBytesRead = static_cast<UINT32>(entry->file()->read(pDest, uiBytesToRead));
		}
			catch (...)
			{
				if (pDest && uiBytesRead < uiBytesToRead)
					memset(static_cast<UINT8*>(pDest) + uiBytesRead, 0, uiBytesToRead - uiBytesRead);
				if (puiBytesRead) *puiBytesRead = uiBytesRead;
				return FALSE;
			}
			if (puiBytesRead) *puiBytesRead = uiBytesRead;
			if (pDest && uiBytesRead < uiBytesToRead)
				memset(static_cast<UINT8*>(pDest) + uiBytesRead, 0, uiBytesToRead - uiBytesRead);
			return uiBytesRead == uiBytesToRead ? TRUE : FALSE;
		}
		if (pDest) memset(pDest, 0, uiBytesToRead);
		if (puiBytesRead) *puiBytesRead = 0;
	return FALSE;
}

BOOLEAN FileReadLine( HWFILE hFile, std::string* pDest )
{
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileHandleEntry* entry = GetFileHandleEntry(hFile);
	if (entry == nullptr || entry->operation != SOperation::READ || pDest == nullptr)
	{
		return FALSE;
	}
	try
	{
		ja2::fileio::File& file = *entry->file();
		if (file.position() >= file.size())
		{
			return FALSE;
		}

		pDest->clear();
		if (file.position() == 0 && file.size() >= 3)
		{
			unsigned char bom[3];
			if (file.read(bom, sizeof(bom)) != sizeof(bom) ||
				bom[0] != 0xef || bom[1] != 0xbb || bom[2] != 0xbf)
			{
				file.seek(0, ja2::fileio::SeekOrigin::begin);
			}
		}

		char character;
		while (file.read(&character, 1) == 1)
		{
			if (character == '\0' || character == '\n')
			{
				break;
			}
			if (character == '\r')
			{
				if (file.position() < file.size())
				{
					char next;
					if (file.read(&next, 1) == 1 && next != '\n' && next != '\0')
						file.seek(-1, ja2::fileio::SeekOrigin::current);
				}
				break;
			}
			pDest->push_back(character);
		}
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
}

//**************************************************************************
//
// FileWrite
//
//		To write a file.
//
// Parameter List :
//
//		HWFILE		->handle to file to write to
//		void	*	->destination buffer
//		UINT32	->num bytes to write
//		UINT32	->num bytes written
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//		08Dec97:ARM		->return FALSE if dwNumBytesToWrite != dwNumBytesWritten
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

BOOLEAN FileWrite( HWFILE hFile, const void* pDest, UINT32 uiBytesToWrite, UINT32 *puiBytesWritten )
{
	if(uiBytesToWrite == 0)//dnl ch38 110909
	{
		if (puiBytesWritten)
		{
			*puiBytesWritten = 0;
		}
		return(TRUE);
	}
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileHandleEntry* entry = GetFileHandleEntry(hFile);
	if(entry != nullptr && entry->operation == SOperation::WRITE)
	{
		try
		{
			entry->file()->writeExact(pDest, uiBytesToWrite);
			if (puiBytesWritten) *puiBytesWritten = uiBytesToWrite;
			return TRUE;
		}
		catch (...)
		{
			if (puiBytesWritten) *puiBytesWritten = 0;
			return FALSE;
		}
	}
	if (puiBytesWritten) *puiBytesWritten = 0;
	return FALSE;
}

//**************************************************************************
//
// FileLoad
//
//		To open, read, and close a file.
//
// Parameter List :
//
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//		08Dec97:ARM		->return FALSE if bytes to read != bytes read (CHECKF is inappropriate?)
//
//**************************************************************************

BOOLEAN FileLoad( STR strFilename, PTR pDest, UINT32 uiBytesToRead, UINT32 *puiBytesRead )
{
	if (puiBytesRead) *puiBytesRead = 0;
	if (strFilename == nullptr)
	{
		return FALSE;
	}
	try
	{
		if (!ja2::fileio::fileServicesInitialized()) return FALSE;
		std::unique_ptr<ja2::fileio::File> file = ja2::fileio::storeRouter().openRead(strFilename);
		const UINT32 count = static_cast<UINT32>(file->read(pDest, uiBytesToRead));
		if (puiBytesRead) *puiBytesRead = count;
		return count == uiBytesToRead ? TRUE : FALSE;
	}
	catch (...)
	{
		return FALSE;
	}
}

//**************************************************************************
//
// FilePrintf
//
//		To printf to a file.
//
// Parameter List :
//
//		HWFILE	->handle to file to seek in
//		...		->arguments, 1st of which should be a string
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************
#ifndef DIM
# define DIM(x) (sizeof(x)/sizeof(x[0]))	/* made StringLen Save, Sergeant_Kolja, 2007-06-10 */
#endif


BOOLEAN JA2_CDECL FilePrintf( HWFILE hFile, STR8	strFormatted, ... )
{
	CHAR8		strToSend[160]; /* itemdescription of item 0 will NOT fit if only 80 Chars per Line!, Sergeant_Kolja, 2007-06-10 */
	va_list	argptr;
	BOOLEAN fRetVal = FALSE;

	va_start(argptr, strFormatted);
	vsnprintf( strToSend, DIM(strToSend), strFormatted, argptr ); /* made StringLen Save, Sergeant_Kolja, 2007-06-10 */
	strToSend[ DIM(strToSend)-1 ] = 0;
	va_end(argptr);
	
	fRetVal = FileWrite( hFile, strToSend, strlen(strToSend), NULL );
	return( fRetVal );
}

//**************************************************************************
//
// FileSeek
//
//		To seek to a position in a file.
//
// Parameter List :
//
//		HWFILE	->handle to file to seek in
//		UINT32	->distance to seek
//		UINT8		->how to seek
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

BOOLEAN FileSeek( HWFILE hFile, UINT32 uiDistance, UINT8 uiHow )
{
	INT32 iDistance = (INT32)uiDistance;

	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileHandleEntry* entry = GetFileHandleEntry(hFile);
	if(entry != nullptr)
	{
		ja2::fileio::SeekOrigin origin;
		if ( uiHow == FILE_SEEK_FROM_START )
		{
			origin = ja2::fileio::SeekOrigin::begin;
		}
		else if ( uiHow == FILE_SEEK_FROM_END )
		{
			origin = ja2::fileio::SeekOrigin::end;
			if( iDistance > 0 )
			{
				iDistance = -(iDistance);
			}
		}
		else
		{
			origin = ja2::fileio::SeekOrigin::current;
		}

		try
		{
			entry->file()->seek(iDistance, origin);
			return TRUE;
		}
		catch (...)
		{
			return FALSE;
		}
	}
	return FALSE;
}

//**************************************************************************
//
// FileGetPos
//
//		To get the current position in a file.
//
// Parameter List :
//
//		HWFILE	->handle to file
//
// Return Value :
//
//		INT32		->current offset in file if successful
//					->-1 if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

INT32 FileGetPos( HWFILE hFile )
{
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileHandleEntry* entry = GetFileHandleEntry(hFile);
	if(entry != nullptr)
	{
		try
		{
			const std::uint64_t position = entry->file()->position();
			if (position > static_cast<std::uint64_t>((std::numeric_limits<INT32>::max)()))
			{
				return BAD_INDEX;
			}
			return static_cast<INT32>(position);
		}
		catch (...)
		{
			return BAD_INDEX;
		}
	}

	return BAD_INDEX;
}

//**************************************************************************
//
// FileGetSize
//
//		To get the current file size.
//
// Parameter List :
//
//		HWFILE	->handle to file
//
// Return Value :
//
//		INT32		->file size in file if successful
//					->0 if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

UINT32 FileGetSize( HWFILE hFile )
{
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileHandleEntry* entry = GetFileHandleEntry(hFile);
	if(entry != nullptr)
	{
		try
		{
			const std::uint64_t size = entry->file()->size();
			return static_cast<UINT32>((std::min)(size,
				static_cast<std::uint64_t>((std::numeric_limits<UINT32>::max)())));
		}
		catch (...)
		{
			return 0;
		}
	}
	return 0;
}


BOOLEAN SetFileManCurrentDirectory( STR pcDirectory )
{
	return pcDirectory != nullptr && ja2::fileio::setCurrentDirectory(pcDirectory) ? TRUE : FALSE;
}


BOOLEAN GetFileManCurrentDirectory( STRING512 pcDirectory )
{
	try
	{
		const std::string directory = ja2::fileio::currentDirectory();
		strncpy(pcDirectory, directory.c_str(), 511);
		pcDirectory[511] = 0;
	}
	catch(const std::exception& ex)
	{
		LogFileManagerError(ex.what());
		return FALSE;
	}
	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Removes ALL FILES in the specified directory (and all subdirectories with their files if fRecursive is TRUE)
// Use EraseDirectory() to simply delete directory contents without deleting the directory itself
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN RemoveFileManDirectory( STRING512 pcDirectory, BOOLEAN fRecursive )
{
	try
	{
		if (!ja2::fileio::fileServicesInitialized()) return FALSE;
		ja2::fileio::storeRouter().clearDirectory(pcDirectory, fRecursive != FALSE);
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Removes ALL FILES in the specified directory but leaves the directory alone.	Does not affect any subdirectories!
// Use RemoveFilemanDirectory() to also delete the directory itself, or to recursively delete subdirectories.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN EraseDirectory( STRING512 pcDirectory)
{
	return RemoveFileManDirectory(pcDirectory, FALSE);
}


BOOLEAN GetExecutableDirectory( STRING512 pcDirectory )
{
	try
	{
		const std::string directory = ja2::fileio::executableDirectory();
		strncpy(pcDirectory, directory.c_str(), 511);
		pcDirectory[511] = 0;
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
}

BOOLEAN GetFileFirst( CHAR8 * pSpec, GETFILESTRUCT *pGFStruct )
{
	CHECKF( pSpec != NULL );
	CHECKF( pGFStruct != NULL );
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);

	try
	{
		if (!ja2::fileio::fileServicesInitialized()) return FALSE;
		std::vector<ja2::fileio::DirectoryEntry> entries =
			ja2::fileio::storeRouter().list(pSpec);
		NormalizeDirectoryEntryNames(entries);
		if (entries.empty())
		{
			pGFStruct->iFindHandle = 0;
			return FALSE;
		}
		FillGetFileStruct(*pGFStruct, entries.front());
		pGFStruct->iFindHandle = RegisterFileSearch(std::move(entries));
		if (pGFStruct->iFindHandle == 0)
		{
			return FALSE;
		}
		return TRUE;
	}
	catch (...)
	{
		pGFStruct->iFindHandle = 0;
		return FALSE;
	}
}

BOOLEAN GetFileNext( GETFILESTRUCT *pGFStruct )
{
	CHECKF( pGFStruct != NULL );
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileSearchEntry* search = GetFileSearchEntry(pGFStruct->iFindHandle);
	if (search == nullptr)
	{
		return FALSE;
	}

	if(search->next < search->entries.size())
	{
		FillGetFileStruct(*pGFStruct, search->entries[search->next++]);
		return TRUE;
	}
	return FALSE;
}

void GetFileClose( GETFILESTRUCT *pGFStruct )
{
	if (pGFStruct == NULL)
	{
		return;
	}

	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileSearchEntry* search = GetFileSearchEntry(pGFStruct->iFindHandle);
	if (search != nullptr)
	{
		ReleaseFileSearchEntry(*search);
	}
	pGFStruct->iFindHandle = 0;
}


//returns true if at end of file, else false
BOOLEAN	FileCheckEndOfFile( HWFILE hFile )
{
	std::lock_guard<std::recursive_mutex> lock(gFileHandlesMutex);
	FileHandleEntry* entry = GetFileHandleEntry(hFile);

	if(entry != nullptr)
	{
		try
		{
			return entry->file()->position() >= entry->file()->size() ? TRUE : FALSE;
		}
		catch (...)
		{
			return FALSE;
		}
	}
	return FALSE;
}


UINT32 FileSize(STR strFilename)
{
	if (strFilename == nullptr)
	{
		return 0;
	}
	try
	{
		if (!ja2::fileio::fileServicesInitialized()) return 0;
		const std::uint64_t size = ja2::fileio::storeRouter().metadata(strFilename).size;
		return static_cast<UINT32>((std::min)(size,
			static_cast<std::uint64_t>((std::numeric_limits<UINT32>::max)())));
	}
	catch (...)
	{
		return 0;
	}
}


// Flugente: simple wrapper to check whether an audio file in mp3/ogg/wav format exists
BOOLEAN	SoundFileExists( STR strFilename, STR zFoundFilename )
{
	sprintf( zFoundFilename, "%s.mp3", strFilename );
	if ( !FileExists( zFoundFilename ) )
	{
		sprintf( zFoundFilename, "%s.ogg", strFilename );
		if ( !FileExists( zFoundFilename ) )
		{
			sprintf( zFoundFilename, "%s.wav", strFilename );
		}
	}

	return FileExists( zFoundFilename );
}
