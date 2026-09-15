#include "transfer_rules.h"
#include "DEBUG.H"
#include "fileio/FileIO.h"
#include "fileio/FileServices.h"
#include "fileio/StoreRouter.h"
#include <vfs/Tools/vfs_parser_tools.h>
#include <vfs/Tools/vfs_tools.h>

CTransferRules::CTransferRules()
: m_eDefaultAction(ACCEPT)
{};


bool CTransferRules::initFromTxtFile(vfs::Path const& sPath)
{
	std::unique_ptr<ja2::fileio::File> file;
	try
	{
		file = ja2::fileio::storeRouter().openRead(sPath.to_string());
	}
	catch(const ja2::fileio::Error& error)
	{
		if(error.code() == ja2::fileio::ErrorCode::notFound) return false;
		throw;
	}
	std::string sBuffer;
	vfs::UInt32 line_counter = 0;
	while(file->position() < file->size())
	{
		sBuffer.clear();
		char character = 0;
		while(file->read(&character, 1) == 1)
		{
			if(character == '\n' || character == '\0') break;
			if(character != '\r') sBuffer.push_back(character);
		}
		line_counter++;
			// very simple parsing : key = value
			if(!sBuffer.empty())
			{
				// remove leading white spaces
				::size_t iStart = sBuffer.find_first_not_of(" \t",0);
				if(iStart == std::string::npos) continue;
				char first = sBuffer.at(iStart);
				switch(first)
				{
				case '!':
				case ';':
				case '#':
					// comment -> do nothing
					break;
				default:
					::size_t iEnd = sBuffer.find_first_of(" \t", iStart);
					if(iEnd != std::string::npos)
					{
						SRule rule;
						std::string action = sBuffer.substr(iStart, iEnd - iStart);
						if( vfs::StrCmp::Equal(action, "deny") )
						{
							rule.action = CTransferRules::DENY;
						}
						else if( vfs::StrCmp::Equal(action, "accept") )
						{
							rule.action = CTransferRules::ACCEPT;
						}
						else
						{
							std::wstring trybuffer = L"Invalid UTF-8 character in string";
							// The VFS macro tests the constant log argument as a pointer.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4127)
#endif
							VFS_IGNOREEXCEPTION( trybuffer = vfs::String(sBuffer).c_wcs(), "" ); /* just make sure we don't break off when string conversion fails */
#ifdef _MSC_VER
#pragma warning(pop)
#endif
							std::wstringstream wss;
							wss << L"Unknown action in file \"" << sPath.c_wcs()
								<< L", line " << line_counter << " : " << vfs::String(sBuffer).c_wcs();
							SGP_THROW(wss.str().c_str());
						}
						try
						{
							rule.pattern = vfs::Path(vfs::trimString(sBuffer, iEnd, sBuffer.length()));
						}
						catch(vfs::Exception& ex)
						{
							std::wstringstream wss;
							wss << L"Could not convert string, invalid utf8 encoding in file \"" << sPath.c_wcs()
								<< L"\", line "  << line_counter;
							SGP_RETHROW(wss.str().c_str(), ex);
						}
						m_listRules.push_back(rule);
					}
					break;
				}; // end switch
			} // end if (empty)
		} // end while(!eof)
	return true;
}

void CTransferRules::setDefaultAction(CTransferRules::EAction act)
{
	m_eDefaultAction = act;
}

CTransferRules::EAction	CTransferRules::getDefaultAction()
{
	return m_eDefaultAction;
}

CTransferRules::EAction CTransferRules::applyRule(vfs::String const& sStr)
{
	tPatternList::iterator sit = m_listRules.begin();
	for(; sit != m_listRules.end(); ++sit)
	{
		if(matchPattern(sit->pattern(), sStr))
		{
			return sit->action;
		}
	}
	return m_eDefaultAction;
}

