#include "KeyMap.h"
#include "input.h"
#include "platform/LegacyKeyCodes.h"
#include "Text.h"

#include <cstddef>

static Str8EnumLookupType gKeyTable[] = 
{
	{LegacyKeyCode::kLBUTTON, "LBUTTON"},
	{LegacyKeyCode::kRBUTTON, "RBUTTON"},
	{LegacyKeyCode::kCANCEL, "CANCEL"},
	{LegacyKeyCode::kMBUTTON, "MBUTTON"},
	{LegacyKeyCode::kXBUTTON1, "XBUTTON1"},
	{LegacyKeyCode::kXBUTTON2, "XBUTTON2"},
	{LegacyKeyCode::kBACK, "BACK"},
	{LegacyKeyCode::kTAB, "TAB"},
	{LegacyKeyCode::kCLEAR, "CLEAR"},
	{LegacyKeyCode::kRETURN, "RETURN"},
	{LegacyKeyCode::kSHIFT, "SHIFT"},
	{LegacyKeyCode::kCONTROL, "CONTROL"},
	{LegacyKeyCode::kMENU, "MENU"},
	{LegacyKeyCode::kPAUSE, "PAUSE"},
	{LegacyKeyCode::kCAPITAL, "CAPITAL"},
	{LegacyKeyCode::kKANA, "KANA"},
	{LegacyKeyCode::kHANGEUL, "HANGEUL"},
	{LegacyKeyCode::kHANGUL, "HANGUL"},
	{LegacyKeyCode::kJUNJA, "JUNJA"},
	{LegacyKeyCode::kFINAL, "FINAL"},
	{LegacyKeyCode::kHANJA, "HANJA"},
	{LegacyKeyCode::kKANJI, "KANJI"},
	{LegacyKeyCode::kESCAPE, "ESCAPE"},
	{LegacyKeyCode::kCONVERT, "CONVERT"},
	{LegacyKeyCode::kNONCONVERT, "NONCONVERT"},
	{LegacyKeyCode::kACCEPT, "ACCEPT"},
	{LegacyKeyCode::kMODECHANGE, "MODECHANGE"},
	{LegacyKeyCode::kSPACE, "SPACE"},
	{LegacyKeyCode::kPRIOR, "PRIOR"},
	{LegacyKeyCode::kNEXT, "NEXT"},
	{LegacyKeyCode::kEND, "END"},
	{LegacyKeyCode::kHOME, "HOME"},
	{LegacyKeyCode::kLEFT, "LEFT"},
	{LegacyKeyCode::kUP, "UP"},
	{LegacyKeyCode::kRIGHT, "RIGHT"},
	{LegacyKeyCode::kDOWN, "DOWN"},
	{LegacyKeyCode::kSELECT, "SELECT"},
	{LegacyKeyCode::kPRINT, "PRINT"},
	{LegacyKeyCode::kEXECUTE, "EXECUTE"},
	{LegacyKeyCode::kSNAPSHOT, "SNAPSHOT"},
	{LegacyKeyCode::kINSERT, "INSERT"},
	{LegacyKeyCode::kDELETE_KEY, "DELETE"},
	{LegacyKeyCode::kHELP, "HELP"},

	{ '0', "0"},
	{ '1', "1"},
	{ '2', "2"},
	{ '3', "3"},
	{ '4', "4"},
	{ '5', "5"},
	{ '6', "6"},
	{ '7', "7"},
	{ '8', "8"},
	{ '9', "9"},
	{ ':', ":"},
	{ ';', ";"},
	{ '<', "<"},
	{ '=', "="},
	{ '>', ">"},
	{ '?', "?"},
	{ 'A', "A"},
	{ 'B', "B"},
	{ 'C', "C"},
	{ 'D', "D"},
	{ 'E', "E"},
	{ 'F', "F"},
	{ 'G', "G"},
	{ 'H', "H"},
	{ 'I', "I"},
	{ 'J', "J"},
	{ 'K', "K"},
	{ 'L', "L"},
	{ 'M', "M"},
	{ 'N', "N"},
	{ 'O', "O"},
	{ 'P', "P"},
	{ 'Q', "Q"},
	{ 'R', "R"},
	{ 'S', "S"},
	{ 'T', "T"},
	{ 'U', "U"},
	{ 'V', "V"},
	{ 'W', "W"},
	{ 'X', "X"},
	{ 'Y', "Y"},
	{ 'Z', "Z"},

	{LegacyKeyCode::kLWIN, "LWIN"},
	{LegacyKeyCode::kRWIN, "RWIN"},
	{LegacyKeyCode::kAPPS, "APPS"},
	{LegacyKeyCode::kSLEEP, "SLEEP"},
	{LegacyKeyCode::kNUMPAD0, "NUMPAD0"},
	{LegacyKeyCode::kNUMPAD1, "NUMPAD1"},
	{LegacyKeyCode::kNUMPAD2, "NUMPAD2"},
	{LegacyKeyCode::kNUMPAD3, "NUMPAD3"},
	{LegacyKeyCode::kNUMPAD4, "NUMPAD4"},
	{LegacyKeyCode::kNUMPAD5, "NUMPAD5"},
	{LegacyKeyCode::kNUMPAD6, "NUMPAD6"},
	{LegacyKeyCode::kNUMPAD7, "NUMPAD7"},
	{LegacyKeyCode::kNUMPAD8, "NUMPAD8"},
	{LegacyKeyCode::kNUMPAD9, "NUMPAD9"},
	{LegacyKeyCode::kMULTIPLY, "MULTIPLY"},
	{LegacyKeyCode::kADD, "ADD"},
	{LegacyKeyCode::kSEPARATOR, "SEPARATOR"},
	{LegacyKeyCode::kSUBTRACT, "SUBTRACT"},
	{LegacyKeyCode::kDECIMAL, "DECIMAL"},
	{LegacyKeyCode::kDIVIDE, "DIVIDE"},
	{LegacyKeyCode::kF1, "F1"},
	{LegacyKeyCode::kF2, "F2"},
	{LegacyKeyCode::kF3, "F3"},
	{LegacyKeyCode::kF4, "F4"},
	{LegacyKeyCode::kF5, "F5"},
	{LegacyKeyCode::kF6, "F6"},
	{LegacyKeyCode::kF7, "F7"},
	{LegacyKeyCode::kF8, "F8"},
	{LegacyKeyCode::kF9, "F9"},
	{LegacyKeyCode::kF10, "F10"},
	{LegacyKeyCode::kF11, "F11"},
	{LegacyKeyCode::kF12, "F12"},
	{LegacyKeyCode::kF13, "F13"},
	{LegacyKeyCode::kF14, "F14"},
	{LegacyKeyCode::kF15, "F15"},
	{LegacyKeyCode::kF16, "F16"},
	{LegacyKeyCode::kF17, "F17"},
	{LegacyKeyCode::kF18, "F18"},
	{LegacyKeyCode::kF19, "F19"},
	{LegacyKeyCode::kF20, "F20"},
	{LegacyKeyCode::kF21, "F21"},
	{LegacyKeyCode::kF22, "F22"},
	{LegacyKeyCode::kF23, "F23"},
	{LegacyKeyCode::kF24, "F24"},
	{LegacyKeyCode::kNUMLOCK, "NUMLOCK"},
	{LegacyKeyCode::kSCROLL, "SCROLL"},
	{LegacyKeyCode::kOEM_NEC_EQUAL, "OEM_NEC_EQUAL"},
	{LegacyKeyCode::kOEM_FJ_JISHO, "OEM_FJ_JISHO"},
	{LegacyKeyCode::kOEM_FJ_MASSHOU, "OEM_FJ_MASSHOU"},
	{LegacyKeyCode::kOEM_FJ_TOUROKU, "OEM_FJ_TOUROKU"},
	{LegacyKeyCode::kOEM_FJ_LOYA, "OEM_FJ_LOYA"},
	{LegacyKeyCode::kOEM_FJ_ROYA, "OEM_FJ_ROYA"},
	{LegacyKeyCode::kLSHIFT, "LSHIFT"},
	{LegacyKeyCode::kRSHIFT, "RSHIFT"},
	{LegacyKeyCode::kLCONTROL, "LCONTROL"},
	{LegacyKeyCode::kRCONTROL, "RCONTROL"},
	{LegacyKeyCode::kLMENU, "LMENU"},
	{LegacyKeyCode::kRMENU, "RMENU"},

	{LegacyKeyCode::kBROWSER_BACK, "BROWSER_BACK"},
	{LegacyKeyCode::kBROWSER_FORWARD, "BROWSER_FORWARD"},
	{LegacyKeyCode::kBROWSER_REFRESH, "BROWSER_REFRESH"},
	{LegacyKeyCode::kBROWSER_STOP, "BROWSER_STOP"},
	{LegacyKeyCode::kBROWSER_SEARCH, "BROWSER_SEARCH"},
	{LegacyKeyCode::kBROWSER_FAVORITES, "BROWSER_FAVORITES"},
	{LegacyKeyCode::kBROWSER_HOME, "BROWSER_HOME"},

	{LegacyKeyCode::kVOLUME_MUTE, "VOLUME_MUTE"},
	{LegacyKeyCode::kVOLUME_DOWN, "VOLUME_DOWN"},
	{LegacyKeyCode::kVOLUME_UP, "VOLUME_UP"},
	{LegacyKeyCode::kMEDIA_NEXT_TRACK, "MEDIA_NEXT_TRACK"},
	{LegacyKeyCode::kMEDIA_PREV_TRACK, "MEDIA_PREV_TRACK"},
	{LegacyKeyCode::kMEDIA_STOP, "MEDIA_STOP"},
	{LegacyKeyCode::kMEDIA_PLAY_PAUSE, "MEDIA_PLAY_PAUSE"},
	{LegacyKeyCode::kLAUNCH_MAIL, "LAUNCH_MAIL"},
	{LegacyKeyCode::kLAUNCH_MEDIA_SELECT, "LAUNCH_MEDIA_SELECT"},
	{LegacyKeyCode::kLAUNCH_APP1, "LAUNCH_APP1"},
	{LegacyKeyCode::kLAUNCH_APP2, "LAUNCH_APP2"},

	{LegacyKeyCode::kOEM_1, "OEM_1"},
	{LegacyKeyCode::kOEM_PLUS, "OEM_PLUS"},
	{LegacyKeyCode::kOEM_COMMA, "OEM_COMMA"},
	{LegacyKeyCode::kOEM_MINUS, "OEM_MINUS"},
	{LegacyKeyCode::kOEM_PERIOD, "OEM_PERIOD"},
	{LegacyKeyCode::kOEM_2, "OEM_2"},
	{LegacyKeyCode::kOEM_3, "OEM_3"},
	{LegacyKeyCode::kOEM_4, "OEM_4"},
	{LegacyKeyCode::kOEM_5, "OEM_5"},
	{LegacyKeyCode::kOEM_6, "OEM_6"},
	{LegacyKeyCode::kOEM_7, "OEM_7"},
	{LegacyKeyCode::kOEM_8, "OEM_8"},
	{LegacyKeyCode::kOEM_AX, "OEM_AX"},
	{LegacyKeyCode::kOEM_102, "OEM_102"},
	{LegacyKeyCode::kICO_HELP, "ICO_HELP"},
	{LegacyKeyCode::kICO_00, "ICO_00"},
	{LegacyKeyCode::kPROCESSKEY, "PROCESSKEY"},
	{LegacyKeyCode::kICO_CLEAR, "ICO_CLEAR"},
	{LegacyKeyCode::kPACKET, "PACKET"},
	{LegacyKeyCode::kOEM_RESET, "OEM_RESET"},
	{LegacyKeyCode::kOEM_JUMP, "OEM_JUMP"},
	{LegacyKeyCode::kOEM_PA1, "OEM_PA1"},
	{LegacyKeyCode::kOEM_PA2, "OEM_PA2"},
	{LegacyKeyCode::kOEM_PA3, "OEM_PA3"},
	{LegacyKeyCode::kOEM_WSCTRL, "OEM_WSCTRL"},
	{LegacyKeyCode::kOEM_CUSEL, "OEM_CUSEL"},
	{LegacyKeyCode::kOEM_ATTN, "OEM_ATTN"},
	{LegacyKeyCode::kOEM_FINISH, "OEM_FINISH"},
	{LegacyKeyCode::kOEM_COPY, "OEM_COPY"},
	{LegacyKeyCode::kOEM_AUTO, "OEM_AUTO"},
	{LegacyKeyCode::kOEM_ENLW, "OEM_ENLW"},
	{LegacyKeyCode::kOEM_BACKTAB, "OEM_BACKTAB"},
	{LegacyKeyCode::kATTN, "ATTN"},
	{LegacyKeyCode::kCRSEL, "CRSEL"},
	{LegacyKeyCode::kEXSEL, "EXSEL"},
	{LegacyKeyCode::kEREOF, "EREOF"},
	{LegacyKeyCode::kPLAY, "PLAY"},
	{LegacyKeyCode::kZOOM, "ZOOM"},
	{LegacyKeyCode::kNONAME, "NONAME"},
	{LegacyKeyCode::kPA1, "PA1"},
	{LegacyKeyCode::kOEM_CLEAR, "OEM_CLEAR"},
	{0, NULL}
};

static inline STR Trim(STR &p) { 
	while(isspace(*p)) *p++ = 0; 
	CHAR8 *e = p + strlen(p) - 1;
	while (e > p && isspace(*e)) *e-- = 0;
	return p;
}

// Checks VK for each byte in the integer, all must be true to return TRUE.
extern BOOLEAN IsKeyPressed(int value)
{
	if (!value)
		return 0;

	BOOLEAN ok = 0;
	UINT8* ptr = (UINT8*)&value;
	int len = sizeof(int) / sizeof(UINT8);
	for (int i=0;i<len && ptr[i];++i)
	{
		if (IsPhysicalKeyPressed(ptr[i]))
			ok = 1;
		else
			return 0;
	}
	return ok;
}


extern int ParseKeyString(const STR value)
{
	STRING512 buffer;
	constexpr std::size_t bufferSize = sizeof(buffer) / sizeof(buffer[0]);
	strncpy(buffer, value, bufferSize);
	buffer[bufferSize - 1] = 0;
	int iresult = 0;
	std::size_t idx = 0;
	UINT8* ptr = (UINT8*)&iresult;
	const CHAR8* sDelims = "|+";
	for ( STR key = strtok(buffer, sDelims); key != NULL; key = strtok(NULL, sDelims) )
	{
		Trim(key);
		int ichr = StringToEnum(key, gKeyTable);
		if (ichr > 0 && ichr <= 0xFF && idx < sizeof(iresult))
			ptr[idx++] = (UINT8)ichr;
	}
	return iresult;
}
