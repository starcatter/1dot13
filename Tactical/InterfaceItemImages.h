#ifndef _ITERFACEITEMIMAGES_H_
#define _ITERFACEITEMIMAGES_H_

#include "types.h"
#include <map>
#include <string>
#include <string_view>

class MDItemVideoObjects
{
public:
	MDItemVideoObjects();

	UINT32	getVObjectForItem(UINT32 key);
	void	registerItem(UINT32 key, const std::string& fileName);
	bool	registerItemsFromFilePattern(std::string_view filePattern);
	void	unRegisterAllItems();
private:
	std::map<UINT32,UINT32> m_mapVObjects;
};


extern bool					g_bUsePngItemImages;
const UINT8					MAX_PITEMS = 20;
// old item image handles
extern UINT32				guiGUNSM;
extern UINT32				guiPITEMS[MAX_PITEMS];

// new item image handles
extern MDItemVideoObjects	g_oGUNSM;
extern MDItemVideoObjects	g_oPITEMS[MAX_PITEMS];

bool RegisterItemImages();

#endif // _ITERFACEITEMIMAGES_H_
