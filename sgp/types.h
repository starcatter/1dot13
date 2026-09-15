#ifndef __TYPES_
#define __TYPES_

#ifndef _SIRTECH_TYPES_
#define _SIRTECH_TYPES_

	#ifdef	RELEASE_WITH_DEBUG_INFO

		//For JA2 Release with debug info build, disable these warnigs messages
		#pragma warning( disable : 4201 4214 4057 4100 4514 4115 4711 4244 )

	#endif




// build defines header....
	



#include <cstdint>
#include <wchar.h>			// for wide-character strings
#include "FileHandle.h"

// *** SIR-TECH TYPE DEFINITIONS ***

// These two types are defined by VC6 and were causing redefinition
// problems, but JA2 is compiled with VC5

// HEY WIZARDRY DUDES, JA2 ISN'T THE ONLY PROGRAM WE COMPILE! :-)

typedef std::uint32_t		UINT32;
typedef std::int64_t		INT64;		// WANNE - BMP: Used for Big Maps
typedef std::int32_t		INT32;
typedef std::uint64_t		UINT64;
//typedef unsigned long long	UINT128;  //Madd:  Doing away with this redundant type

// integers
typedef std::uint8_t	UINT8;
typedef std::int8_t		INT8;
typedef std::uint16_t	UINT16;
typedef std::int16_t	INT16;
// floats
typedef float			FLOAT;
typedef double			DOUBLE;
// strings
typedef char			CHAR8;
#ifdef _WIN32
typedef wchar_t			CHAR16;
#else
// Engine text and serialized strings use Windows-width UTF-16 code units.
// Do not use Linux wchar_t here: it is normally 32 bits wide.
typedef char16_t		CHAR16;
#endif
typedef CHAR8 * 		STR;
typedef CHAR8 *			STR8;
typedef CHAR16 *		STR16;
// flags (individual bits used)
typedef std::uint8_t	FLAGS8;
typedef std::uint16_t	FLAGS16;
typedef std::uint32_t	FLAGS32;
typedef UINT64			FLAGS64;
// other
typedef std::uint8_t	BOOLEAN;
typedef void *			PTR;
typedef std::uint16_t	HNDL;
typedef std::uint8_t	BYTE;
typedef CHAR8			STRING512[512];

static_assert(sizeof(INT8) == 1 && sizeof(UINT8) == 1, "8-bit engine types changed width");
static_assert(sizeof(INT16) == 2 && sizeof(UINT16) == 2, "16-bit engine types changed width");
static_assert(sizeof(INT32) == 4 && sizeof(UINT32) == 4, "32-bit engine types changed width");
static_assert(sizeof(INT64) == 8 && sizeof(UINT64) == 8, "64-bit engine types changed width");
static_assert(sizeof(FLAGS32) == 4, "FLAGS32 must remain serialized as 32 bits");
static_assert(sizeof(BOOLEAN) == 1, "BOOLEAN must remain an 8-bit engine value");
static_assert(sizeof(CHAR16) == 2, "CHAR16 must remain a UTF-16 code unit");

#define SGPFILENAME_LEN 100
typedef CHAR8 SGPFILENAME[SGPFILENAME_LEN];	

// *** SIR-TECH TYPE DEFINITIONS ***

#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define BAD_INDEX -1

#define NULL_HANDLE 65535

#define PI 3.1415926

#define ST_EPSILON 0.00001	// define a sir-tech epsilon value

#ifndef NULL
#define NULL 0
#endif

typedef struct
{
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;

} SGPRectangle;

typedef struct
{ 
	INT32 iLeft;
	INT32 iTop;
	INT32 iRight;
	INT32 iBottom;

} SGPRect;

typedef struct
{
	INT32 	iX;
	INT32	iY;

} SGPPoint;

typedef struct
{
	INT32 Min;
	INT32 Max;

} SGPRange;


typedef FLOAT	 VECTOR2[2];		// 2d vector (2x1 matrix)
typedef FLOAT	 VECTOR3[3];		// 3d vector (3x1 matrix)
typedef FLOAT	 VECTOR4[4];		// 4d vector (4x1 matrix)

typedef INT32		IVECTOR2[2];		// 2d vector (2x1 matrix)
typedef INT32		IVECTOR3[3];		// 3d vector (3x1 matrix)
typedef INT32		IVECTOR4[4];		// 4d vector (4x1 matrix)

typedef VECTOR3	MATRIX3[3];		// 3x3 matrix
typedef VECTOR4	MATRIX4[4];		// 4x4 matrix

//typedef VECTOR3	ANGLE;			// angle return array //lal removed
typedef	VECTOR4	COLOR;			// rgba color array


#include <vfs/Aspects/vfs_settings.h>
#include <vfs/Core/vfs_string.h>

inline void convert_string(std::wstring const& str_in, std::string &str_out)
{
	if(vfs::Settings::getUseUnicode())
	{
		str_out = vfs::String::as_utf8(str_in);
	}
	else
	{
		vfs::String::narrow(str_in, str_out);
	}
}

inline void convert_string(std::string const& str_in, std::wstring &str_out)
{
	if(vfs::Settings::getUseUnicode())
	{
		vfs::String::as_utf16(str_in, str_out);
	}
	else
	{
		vfs::String::widen(str_in, str_out);
	}
}


#endif
