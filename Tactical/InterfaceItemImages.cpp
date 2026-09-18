#include "InterfaceItemImages.h"

#include "DEBUG.H"
#include "vobject.h"
#include "Utilities.h"
#include "GameSettings.h"
#include "fileio/FileServices.h"
#include "fileio/StoreRouter.h"
#include "UtfConversion.h"

#include <sstream>

#include <vfs/Core/vfs_string.h>

extern void WriteMessageToFile( const STR16 pString );

/******************************************************************************/

bool					g_bUsePngItemImages = false;
// old item image handles
UINT32					guiGUNSM;
UINT32					guiPITEMS[MAX_PITEMS];

// new item image handles
MDItemVideoObjects		g_oGUNSM;
MDItemVideoObjects		g_oPITEMS[MAX_PITEMS];

/******************************************************************************/

MDItemVideoObjects::MDItemVideoObjects()
{}

UINT32 MDItemVideoObjects::getVObjectForItem(UINT32 key)
{
	std::map<UINT32,UINT32>::iterator it = m_mapVObjects.find(key);
	if(it != m_mapVObjects.end())
	{
		return it->second;
	}
	SGP_THROW(_BS(L"Item key not registered : ") << key << _BS::wget);	
}

void MDItemVideoObjects::registerItem(UINT32 key, const std::string& fileName)
{
	std::map<UINT32,UINT32>::iterator it = m_mapVObjects.find(key);
	if(it != m_mapVObjects.end())
	{
		SGP_THROW(_BS(L"Item image already registered : ") << key << _BS::wget);
	}
	// LOAD INTERFACE GUN PICTURES
	UINT32 uiVObject;
	VOBJECT_DESC VObjectDesc;
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP(const_cast<STR>(fileName.c_str()), VObjectDesc.ImageFile);
	if(! AddVideoObject( &VObjectDesc, &uiVObject ))
	{
		SGP_THROW(_BS(L"Could not add video object for file \"") <<
			vfs::String(fileName) << L"\"" << _BS::wget);
	}
	m_mapVObjects.insert(std::make_pair(key,uiVObject));
}

bool MDItemVideoObjects::registerItemsFromFilePattern(std::string_view filePattern)
{
	const std::string pattern = ja2::fileio::StoreRouter::normalizeLogicalPath(filePattern);
	const std::size_t slash = pattern.find_last_of('/');
	const std::string directory = slash == std::string::npos ? std::string() : pattern.substr(0, slash + 1);
	const std::vector<ja2::fileio::DirectoryEntry> entries =
		ja2::fileio::storeRouter().list(pattern);
	if(entries.empty())
	{
		return false;
	}
	for(const ja2::fileio::DirectoryEntry& entry : entries)
	{
		int item = 0;
		std::istringstream name(entry.name);
		if(!(name >> item))
		{
			CHAR16 error[512];
			const ja2::text::Utf16String name = ja2::text::utf8ToUtf16ReplacingInvalid(entry.name);
			swprintf(error, JA2_TEXT("Could not extract item number from file \"%s\""), name.c_str());
			WriteMessageToFile(error);
			continue;
		}
		const std::string fileName = directory + entry.name;
		try
		{
			this->registerItem(item, fileName);
		}
		catch(std::exception& ex)
		{
			SGP_RETHROW( _BS(L"Registering item from file \"") <<
				vfs::String(fileName) << L"\" failed" << _BS::wget, ex );
		}
	}
	return true;
}


void MDItemVideoObjects::unRegisterAllItems()
{
	std::map<UINT32,UINT32>::iterator it = m_mapVObjects.begin();
	for(; it != m_mapVObjects.end(); ++it)
	{
		// later
	}
}

/******************************************************************************/

bool RegisterItemImages()
{
	VOBJECT_DESC	VObjectDesc;

	//// LOAD INTERFACE GUN PICTURES
	if(!g_bUsePngItemImages)
	{
		VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
		FilenameForBPP("INTERFACE\\mdguns.sti", VObjectDesc.ImageFile);
		if( !AddVideoObject( &VObjectDesc, &guiGUNSM ) )
			AssertMsg(0, "Missing INTERFACE\\mdguns.sti" );
	}
	else if(!g_oGUNSM.registerItemsFromFilePattern("INTERFACE/mdguns/*.png"))
	{
		return false;
	}

	for (UINT8 ubLoop = 0; ubLoop < gGameExternalOptions.ubNumPItems; ubLoop++)
	{
		// LOAD INTERFACE ITEM PICTURES
		if(!g_bUsePngItemImages)
		{
			VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
			FilenameForBPP(String("INTERFACE\\mdp%ditems.sti",ubLoop+1), VObjectDesc.ImageFile);
			if( !AddVideoObject( &VObjectDesc, &guiPITEMS[ubLoop] ) )
				AssertMsg(0, String("Missing INTERFACE\\mdp%ditems.sti",ubLoop+1) );
		}
		else if(!g_oPITEMS[ubLoop].registerItemsFromFilePattern(String("INTERFACE/MDP%dITEMS/*.png",ubLoop+1)))
		{
			return false;
		}

	}

	return true;
}

