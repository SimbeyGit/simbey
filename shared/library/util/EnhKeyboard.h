#pragma once

struct CHAR_PAIR
{
	BYTE LeadChar;
	BYTE SecondChar;
};

struct ENH_KEY_VAL
{
	USHORT ScanCode;
	CHAR_PAIR RegChars;
	CHAR_PAIR ShiftChars;
	CHAR_PAIR CtrlChars;
	CHAR_PAIR AltChars;
};

struct NORMAL_KEY_VAL
{
	CHAR_PAIR RegChars;
	CHAR_PAIR ShiftChars;
	CHAR_PAIR CtrlChars;
	CHAR_PAIR AltChars;
};

const CHAR_PAIR* GetExtendedKeycode (const KEY_EVENT_RECORD* pcker);
HRESULT HrKBHit (HANDLE hConsole, __out_opt KEY_EVENT_RECORD* pker);
