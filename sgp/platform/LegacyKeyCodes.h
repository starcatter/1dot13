#pragma once

#include <cstdint>

// JA2 configuration files use Win32 virtual-key names. These stable byte
// values are a file-format contract, not a reason for the parser to include
// the Windows SDK.
namespace LegacyKeyCode
{
inline constexpr std::uint8_t kLBUTTON = 0x01, kRBUTTON = 0x02, kCANCEL = 0x03;
inline constexpr std::uint8_t kMBUTTON = 0x04, kXBUTTON1 = 0x05, kXBUTTON2 = 0x06;
inline constexpr std::uint8_t kBACK = 0x08, kTAB = 0x09, kCLEAR = 0x0c, kRETURN = 0x0d;
inline constexpr std::uint8_t kSHIFT = 0x10, kCONTROL = 0x11, kMENU = 0x12;
inline constexpr std::uint8_t kPAUSE = 0x13, kCAPITAL = 0x14, kKANA = 0x15;
inline constexpr std::uint8_t kHANGEUL = kKANA, kHANGUL = kKANA, kJUNJA = 0x17;
inline constexpr std::uint8_t kFINAL = 0x18, kHANJA = 0x19, kKANJI = kHANJA;
inline constexpr std::uint8_t kESCAPE = 0x1b, kCONVERT = 0x1c, kNONCONVERT = 0x1d;
inline constexpr std::uint8_t kACCEPT = 0x1e, kMODECHANGE = 0x1f, kSPACE = 0x20;
inline constexpr std::uint8_t kPRIOR = 0x21, kNEXT = 0x22, kEND = 0x23, kHOME = 0x24;
inline constexpr std::uint8_t kLEFT = 0x25, kUP = 0x26, kRIGHT = 0x27, kDOWN = 0x28;
inline constexpr std::uint8_t kSELECT = 0x29, kPRINT = 0x2a, kEXECUTE = 0x2b;
inline constexpr std::uint8_t kSNAPSHOT = 0x2c, kINSERT = 0x2d, kDELETE_KEY = 0x2e;
inline constexpr std::uint8_t kHELP = 0x2f, kLWIN = 0x5b, kRWIN = 0x5c, kAPPS = 0x5d;
inline constexpr std::uint8_t kSLEEP = 0x5f;
inline constexpr std::uint8_t kNUMPAD0 = 0x60, kNUMPAD1 = 0x61, kNUMPAD2 = 0x62;
inline constexpr std::uint8_t kNUMPAD3 = 0x63, kNUMPAD4 = 0x64, kNUMPAD5 = 0x65;
inline constexpr std::uint8_t kNUMPAD6 = 0x66, kNUMPAD7 = 0x67, kNUMPAD8 = 0x68;
inline constexpr std::uint8_t kNUMPAD9 = 0x69, kMULTIPLY = 0x6a, kADD = 0x6b;
inline constexpr std::uint8_t kSEPARATOR = 0x6c, kSUBTRACT = 0x6d;
inline constexpr std::uint8_t kDECIMAL = 0x6e, kDIVIDE = 0x6f;
inline constexpr std::uint8_t kF1 = 0x70, kF2 = 0x71, kF3 = 0x72, kF4 = 0x73;
inline constexpr std::uint8_t kF5 = 0x74, kF6 = 0x75, kF7 = 0x76, kF8 = 0x77;
inline constexpr std::uint8_t kF9 = 0x78, kF10 = 0x79, kF11 = 0x7a, kF12 = 0x7b;
inline constexpr std::uint8_t kF13 = 0x7c, kF14 = 0x7d, kF15 = 0x7e, kF16 = 0x7f;
inline constexpr std::uint8_t kF17 = 0x80, kF18 = 0x81, kF19 = 0x82, kF20 = 0x83;
inline constexpr std::uint8_t kF21 = 0x84, kF22 = 0x85, kF23 = 0x86, kF24 = 0x87;
inline constexpr std::uint8_t kNUMLOCK = 0x90, kSCROLL = 0x91;
inline constexpr std::uint8_t kOEM_NEC_EQUAL = 0x92, kOEM_FJ_JISHO = 0x92;
inline constexpr std::uint8_t kOEM_FJ_MASSHOU = 0x93, kOEM_FJ_TOUROKU = 0x94;
inline constexpr std::uint8_t kOEM_FJ_LOYA = 0x95, kOEM_FJ_ROYA = 0x96;
inline constexpr std::uint8_t kLSHIFT = 0xa0, kRSHIFT = 0xa1;
inline constexpr std::uint8_t kLCONTROL = 0xa2, kRCONTROL = 0xa3;
inline constexpr std::uint8_t kLMENU = 0xa4, kRMENU = 0xa5;
inline constexpr std::uint8_t kBROWSER_BACK = 0xa6, kBROWSER_FORWARD = 0xa7;
inline constexpr std::uint8_t kBROWSER_REFRESH = 0xa8, kBROWSER_STOP = 0xa9;
inline constexpr std::uint8_t kBROWSER_SEARCH = 0xaa, kBROWSER_FAVORITES = 0xab;
inline constexpr std::uint8_t kBROWSER_HOME = 0xac, kVOLUME_MUTE = 0xad;
inline constexpr std::uint8_t kVOLUME_DOWN = 0xae, kVOLUME_UP = 0xaf;
inline constexpr std::uint8_t kMEDIA_NEXT_TRACK = 0xb0, kMEDIA_PREV_TRACK = 0xb1;
inline constexpr std::uint8_t kMEDIA_STOP = 0xb2, kMEDIA_PLAY_PAUSE = 0xb3;
inline constexpr std::uint8_t kLAUNCH_MAIL = 0xb4, kLAUNCH_MEDIA_SELECT = 0xb5;
inline constexpr std::uint8_t kLAUNCH_APP1 = 0xb6, kLAUNCH_APP2 = 0xb7;
inline constexpr std::uint8_t kOEM_1 = 0xba, kOEM_PLUS = 0xbb, kOEM_COMMA = 0xbc;
inline constexpr std::uint8_t kOEM_MINUS = 0xbd, kOEM_PERIOD = 0xbe, kOEM_2 = 0xbf;
inline constexpr std::uint8_t kOEM_3 = 0xc0, kOEM_4 = 0xdb, kOEM_5 = 0xdc;
inline constexpr std::uint8_t kOEM_6 = 0xdd, kOEM_7 = 0xde, kOEM_8 = 0xdf;
inline constexpr std::uint8_t kOEM_AX = 0xe1, kOEM_102 = 0xe2, kICO_HELP = 0xe3;
inline constexpr std::uint8_t kICO_00 = 0xe4, kPROCESSKEY = 0xe5, kICO_CLEAR = 0xe6;
inline constexpr std::uint8_t kPACKET = 0xe7, kOEM_RESET = 0xe9, kOEM_JUMP = 0xea;
inline constexpr std::uint8_t kOEM_PA1 = 0xeb, kOEM_PA2 = 0xec, kOEM_PA3 = 0xed;
inline constexpr std::uint8_t kOEM_WSCTRL = 0xee, kOEM_CUSEL = 0xef, kOEM_ATTN = 0xf0;
inline constexpr std::uint8_t kOEM_FINISH = 0xf1, kOEM_COPY = 0xf2, kOEM_AUTO = 0xf3;
inline constexpr std::uint8_t kOEM_ENLW = 0xf4, kOEM_BACKTAB = 0xf5, kATTN = 0xf6;
inline constexpr std::uint8_t kCRSEL = 0xf7, kEXSEL = 0xf8, kEREOF = 0xf9;
inline constexpr std::uint8_t kPLAY = 0xfa, kZOOM = 0xfb, kNONAME = 0xfc;
inline constexpr std::uint8_t kPA1 = 0xfd, kOEM_CLEAR = 0xfe;
}
