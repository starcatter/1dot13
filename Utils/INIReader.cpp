#include "INIReader.h"
#include "FileMan.h"
#include "DEBUG.H"
#include "Font Control.h"
#include "message.h"
#include "fileio/BfVfsResourceStore.h"
#include "fileio/FileIO.h"
#include "fileio/FileServices.h"
#include "fileio/StoreRouter.h"
#include <stdio.h>
#include <string.h>
#include <sstream>

// Kaiden: INI reading function definitions:

#include <vfs/Tools/vfs_tools.h>

std::set<std::string> CIniReader::m_merge_files;
std::stack<std::string> iniErrorMessages;

namespace
{
bool loadIni(vfs::PropertyContainer& properties, ja2::fileio::File& file)
{
	std::string section;
	std::string line;
	while(file.position() < file.size())
	{
		line.clear();
		char character = 0;
		while(file.read(&character, 1) == 1)
		{
			if(character == '\n' || character == '\0') break;
			if(character != '\r') line.push_back(character);
		}
		if(line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xef &&
			static_cast<unsigned char>(line[1]) == 0xbb &&
			static_cast<unsigned char>(line[2]) == 0xbf)
		{
			line.erase(0, 3);
		}
		const std::size_t start = line.find_first_not_of(" \t");
		if(start == std::string::npos || line[start] == '!' || line[start] == ';' || line[start] == '#')
			continue;
		if(line[start] == '[')
		{
			const std::size_t close = line.find(']', start + 1);
			if(close != std::string::npos && close > start + 1)
				section = vfs::trimString(vfs::String(line.substr(start + 1,
					close - start - 1)), 0, close - start - 1).utf8();
			continue;
		}
		if(section.empty()) continue;

		std::size_t separator = line.find_first_of("+=", start);
		if(separator == std::string::npos) continue;
		const bool append = line[separator] == '+' && separator + 1 < line.size() &&
			line[separator + 1] == '=';
		const std::string key = vfs::trimString(vfs::String(line.substr(start,
			separator - start)), 0, separator - start).utf8();
		if(key.empty()) continue;
		if(append) ++separator;
		const std::string value = vfs::trimString(vfs::String(line.substr(separator + 1)),
			0, line.size() - separator - 1).utf8();
		vfs::String finalValue(value);
		if(append)
		{
			finalValue = properties.getStringProperty(section, key, L"");
			if(!finalValue.empty()) finalValue += L", ";
			finalValue += vfs::String(value);
		}
		properties.setStringProperty(section, key, finalValue);
	}
	return true;
}

bool loadLogicalIni(vfs::PropertyContainer& properties, std::string_view name)
{
	try
	{
		std::unique_ptr<ja2::fileio::File> file = ja2::fileio::storeRouter().openRead(name);
		return loadIni(properties, *file);
	}
	catch(const ja2::fileio::Error& error)
	{
		if(error.code() == ja2::fileio::ErrorCode::notFound) return false;
		throw;
	}
}

std::string overrideName(std::string_view name)
{
	std::string result = ja2::fileio::StoreRouter::normalizeLogicalPath(name);
	const std::size_t slash = result.find_last_of('/');
	const std::size_t dot = result.find_last_of('.');
	if(dot != std::string::npos && (slash == std::string::npos || dot > slash)) result.resize(dot);
	result += ".Override";
	return result;
}
}

template<typename ValueType>
void PushErrorMessage(std::string const& filename,
					  std::string const& section,
					  std::string const& key, 
					  ValueType value, ValueType used_value,
					  ValueType minVal, ValueType maxVal)
{
	std::stringstream errMessage;
	errMessage << "The value [" << section << "][" <<  key << "] = \"" << value << "\" "
		<< "in file [" << filename << "] "
		<< "is outside the valid range [" << minVal << " , " << maxVal << "].  "
		<< used_value << " will be used.";
	iniErrorMessages.push(errMessage.str());
}

void CIniReader::RegisterFileForMerging(std::string_view filename)
{
	m_merge_files.insert(ja2::fileio::StoreRouter::normalizeLogicalPath(filename));
}

CIniReader::CIniReader(const CHAR8*	szFileName)
{
	memset(m_szFileName,0,sizeof(m_szFileName));
	CIniReader_File_Found = FALSE;
	strncpy(m_szFileName,szFileName, std::min<int>(strlen(szFileName), sizeof(m_szFileName)-1));
	const std::string logicalName = ja2::fileio::StoreRouter::normalizeLogicalPath(szFileName);
	if(!ja2::fileio::fileServicesInitialized())
	{
		CIniReader_File_Found = m_oProps.initFromIniFile(vfs::Path(szFileName)) ? TRUE : FALSE;
	}
	else if(m_merge_files.find(logicalName) == m_merge_files.end())
	{
		CIniReader_File_Found = loadLogicalIni(m_oProps, logicalName) ? TRUE : FALSE;
	}
	else
	{
		std::vector<ja2::fileio::ResourceVersion> versions =
			ja2::fileio::resourceStore().openAll(logicalName);
		for(auto it = versions.rbegin(); it != versions.rend(); ++it)
		{
			loadIni(m_oProps, *it->file);
			CIniReader_File_Found = TRUE;
		}
	}
	if(ja2::fileio::fileServicesInitialized())
	{
		(void)loadLogicalIni(m_oProps, overrideName(logicalName));
	}
}

CIniReader::CIniReader(const CHAR8*	szFileName, BOOLEAN Force_Custom_Data_Path)
{
	memset(m_szFileName,0,sizeof(m_szFileName));
	CIniReader_File_Found = FALSE;
	// ary-05/05/2009 : force custom data path for potential non existing file -or- force default data path
	//       : Also, flag file detection to allow functions to determine course of action for case of file [not found/is found].
	strncpy(m_szFileName,szFileName, std::min<int>(strlen(szFileName), sizeof(m_szFileName)-1));
	const std::string logicalName = ja2::fileio::StoreRouter::normalizeLogicalPath(szFileName);
	if(!ja2::fileio::fileServicesInitialized())
	{
		CIniReader_File_Found = m_oProps.initFromIniFile(vfs::Path(szFileName));
	}
	else if(m_merge_files.find(logicalName) == m_merge_files.end())
	{
		CIniReader_File_Found = loadLogicalIni(m_oProps, logicalName) ? TRUE : FALSE;
	}
	else
	{
		std::vector<ja2::fileio::ResourceVersion> versions =
			ja2::fileio::resourceStore().openAll(logicalName);
		CIniReader_File_Found = TRUE;
		for(auto it = versions.rbegin(); it != versions.rend(); ++it)
		{
			CIniReader_File_Found = loadIni(m_oProps, *it->file) && CIniReader_File_Found ? TRUE : FALSE;
		}
	}
}

void CIniReader::Clear()
{
	memset(m_szFileName, 0, sizeof(m_szFileName));
	m_oProps.clearContainer();
}


int CIniReader::ReadInteger(const CHAR8*	szSection, const CHAR8*	szKey, int iDefaultValue)
{
	return (int)(m_oProps.getIntProperty(szSection, szKey, iDefaultValue));
}


int CIniReader::ReadInteger(const CHAR8* szSection, const CHAR8* szKey, int defaultValue, int minValue, int maxValue)
{
	int iniValueReadFromFile = (int)(m_oProps.getIntProperty(szSection, szKey, defaultValue));
	//AssertGE(iniValueReadFromFile, minValue);
	//AssertLE(iniValueReadFromFile, maxValue);
	if (iniValueReadFromFile < minValue)
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, minValue, minValue, maxValue);
		return minValue;
	} 
	else if (iniValueReadFromFile > maxValue)
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, maxValue, minValue, maxValue);
		return maxValue;
	}
	return iniValueReadFromFile;
}



//float CIniReader::ReadDouble(const STR8	szSection, const STR8	szKey, float fltDefaultValue)
//{
// char szResult[255];
// char szDefault[255];
// float fltResult;
// sprintf(szDefault, "%f",fltDefaultValue);
// GetPrivateProfileString(szSection,	szKey, szDefault, szResult, 255, m_szFileName);
// fltResult = (float) atof(szResult);
// return fltResult;
//}

double CIniReader::ReadDouble(const CHAR8* szSection, const CHAR8* szKey, double defaultValue, double minValue, double maxValue)
{
	double iniValueReadFromFile;
	iniValueReadFromFile = m_oProps.getFloatProperty(szSection, szKey, defaultValue);
	//AssertGE(iniValueReadFromFile, minValue);
	//AssertLE(iniValueReadFromFile, maxValue);
	if (iniValueReadFromFile < minValue)
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey,iniValueReadFromFile, minValue, minValue, maxValue);
		return minValue;
	}
	else if (iniValueReadFromFile > maxValue)
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, maxValue, minValue, maxValue);
		return maxValue;
	}
	return iniValueReadFromFile;
}

FLOAT CIniReader::ReadFloat(const CHAR8* szSection, const CHAR8* szKey, FLOAT defaultValue, FLOAT minValue, FLOAT maxValue)
{
	FLOAT iniValueReadFromFile;
	iniValueReadFromFile = (FLOAT) m_oProps.getFloatProperty(szSection, szKey, (float)defaultValue);

	//AssertGE(iniValueReadFromFile, minValue);
	//AssertLE(iniValueReadFromFile, maxValue);

	if (iniValueReadFromFile < minValue) 
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, minValue, minValue, maxValue);
		return minValue;
	}
	else if (iniValueReadFromFile > maxValue)
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, maxValue, minValue, maxValue);
		return maxValue;
	}
	return iniValueReadFromFile;
}

void CIniReader::ReadFloatArray(const CHAR8* szSection, const CHAR8* szKey, std::vector<FLOAT>& vec)
{
	// read the array as a string
	STRING512 textBuffer;
	ReadString(szSection, szKey, "", textBuffer, _countof(textBuffer));

	std::string str, token;
	std::string delim = ",";
	size_t offset = 0, prevOffset = 0;

	// sanitise input
	vec.clear();
	str = textBuffer;
	str.erase(std::remove(str.begin(), str.end(), ' '), str.end());

	// split into array
	try
	{
		do
		{
			offset = str.find(delim, prevOffset);
			token = str.substr(prevOffset, offset - prevOffset);
			prevOffset = offset + delim.length();

			vec.push_back(std::stof(token.c_str()));
		} while (offset != std::string::npos);
	}
	catch (...)
	{
		std::stringstream errMessage;
		errMessage << "There was an error reading array [" << szSection << "][" << szKey << "] in file [" << m_szFileName << "]. Defaulting to [0].";
		iniErrorMessages.push(errMessage.str());

		vec.push_back(0);
	}
}

void CIniReader::ReadINT32Array(const CHAR8* szSection, const CHAR8* szKey, std::vector<INT32>& vec)
{
	// read the array as a string
	STRING512 textBuffer;
	ReadString(szSection, szKey, "", textBuffer, _countof(textBuffer));

	std::string str, token;
	std::string delim = ",";
	size_t offset = 0, prevOffset = 0;

	// sanitise input
	vec.clear();
	str = textBuffer;
	str.erase(std::remove(str.begin(), str.end(), ' '), str.end());

	// split into array
	try
	{
		do
		{
			offset = str.find(delim, prevOffset);
			token = str.substr(prevOffset, offset - prevOffset);
			prevOffset = offset + delim.length();

			vec.push_back(std::stoi(token.c_str()));
		} while (offset != std::string::npos);
	}
	catch (...)
	{
		std::stringstream errMessage;
		errMessage << "There was an error reading array [" << szSection << "][" << szKey << "] in file [" << m_szFileName << "]. Defaulting to [0].";
		iniErrorMessages.push(errMessage.str());

		vec.push_back(0);
	}
}

BOOLEAN CIniReader::ReadBoolean(const CHAR8* szSection, const CHAR8* szKey, bool defaultValue, bool bolDisplayError)
{
	vfs::String str = m_oProps.getStringProperty(szSection, szKey, L"");
	if( vfs::StrCmp::Equal(str, L"true") )
	{
		return TRUE;
	}
	else if( vfs::StrCmp::Equal(str, L"false") )
	{
		return FALSE;
	}
	std::string szResult = str.utf8();
	char szDefault[255];
	sprintf(szDefault, "%s", defaultValue? "TRUE" : "FALSE");

	if(bolDisplayError){
		std::stringstream errMessage;
		errMessage << "The value [" << szSection << "][" << szKey << "] = \"" << szResult << "\" "
			<< "in file [" << this->m_szFileName << "] is neither TRUE nor FALSE.  The value " << szDefault << " will be used.";
		iniErrorMessages.push(errMessage.str());
	}
	return defaultValue;
}

// ary-05/15/2009 : snippet on how to use CIniReader::ReadString
//	const  STR8 test_ini_string = new char[255];
//	memset(test_ini_string, 0x00, 255);
//	iniReader.ReadString("JA2 Game Settings" , "TEST_STRING" , "default string" , test_ini_string , 255 );

void CIniReader::ReadString(const CHAR8* szSection, const CHAR8* szKey, const CHAR8* szDefaultValue, STR8 input_buffer, size_t buffer_size)
{
	std::string s = m_oProps.getStringProperty(szSection, szKey, szDefaultValue).utf8();
	int len = std::min<unsigned int>(s.length(),buffer_size-1);
	strncpy(input_buffer, s.c_str(), len);
	input_buffer[len] = 0;
}

// WANNE - MP: Old version, currently used by Multiplayer
STR8	CIniReader::ReadString(const CHAR8*	szSection, const CHAR8*	szKey, const CHAR8*	szDefaultValue)
{
	// >>>>> Memory Leak <<<<<
	STR8	szResult = new char[255];
	memset(szResult, 0x00, 255);
	std::string s = m_oProps.getStringProperty(szSection, szKey, szDefaultValue).utf8();
	strncpy(szResult, s.c_str(), std::min<int>(s.length(),254));
	return szResult;
}

UINT8  CIniReader::ReadUINT8(const CHAR8* szSection, const CHAR8* szKey, UINT8  defaultValue, UINT8  minValue, UINT8  maxValue)
{
	UINT8 iniValueReadFromFile;


	iniValueReadFromFile = (UINT8) this->ReadUINT( szSection,  szKey, (UINT32) defaultValue, (UINT32) minValue, (UINT32) maxValue);

	return iniValueReadFromFile;

}

UINT16 CIniReader::ReadUINT16(const CHAR8* szSection, const CHAR8* szKey, UINT16 defaultValue, UINT16 minValue, UINT16 maxValue)
{
	UINT16 iniValueReadFromFile;

	iniValueReadFromFile = (UINT16) this->ReadUINT( szSection,  szKey, (UINT32) defaultValue, (UINT32) minValue, (UINT32) maxValue);

	return iniValueReadFromFile;

}

UINT32 CIniReader::ReadUINT32(const CHAR8* szSection, const CHAR8* szKey, UINT32 defaultValue, UINT32 minValue, UINT32 maxValue)
{
	UINT32 iniValueReadFromFile;

	iniValueReadFromFile = (UINT32) this->ReadUINT( szSection,  szKey, (UINT32) defaultValue, (UINT32) minValue, (UINT32) maxValue);

	return iniValueReadFromFile;

}

UINT32 CIniReader::ReadUINT(const CHAR8* szSection, const CHAR8* szKey, UINT32 defaultValue, UINT32 minValue, UINT32 maxValue )
{ 
	UINT32 iniValueReadFromFile;
	iniValueReadFromFile = (UINT32) m_oProps.getUIntProperty(szSection, szKey, defaultValue);
	//AssertGE(iniValueReadFromFile, minValue);
	//AssertLE(iniValueReadFromFile, maxValue);

	if (iniValueReadFromFile < minValue) 
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, minValue, minValue, maxValue);
		iniValueReadFromFile = minValue;
	} 
	else if (iniValueReadFromFile > maxValue) 
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, maxValue, minValue, maxValue);
		iniValueReadFromFile = maxValue;
	}

	return iniValueReadFromFile;
}
