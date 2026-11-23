#pragma once
#include "Runtime.h"

namespace QF
{
	inline BOOL ValidTime(const SYSTEMTIME& value)
	{
		static const WORD days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
		if(value.wYear < 1601 || value.wYear > 9999 || value.wMonth < 1 || value.wMonth > 12)
		{
			return FALSE;
		}
		UINT maximum = days[value.wMonth - 1];
		if(value.wMonth == 2 && !(value.wYear % 4) && (value.wYear % 100 || !(value.wYear % 400)))
		{
			maximum++;
		}
		return value.wDay >= 1 && value.wDay <= maximum && value.wHour < 24 && value.wMinute < 60 && value.wSecond < 60 && value.wMilliseconds < 1000;
	}
	inline HRESULT ParseTime(RSTRING text, BOOL dateOnly, SYSTEMTIME* value)
	{
		HRESULT hr = S_OK;
		static const UINT offsets[] = {0,5,8,11,14,17,20};
		static const UINT widths[] = {4,2,2,2,2,2,3};
		WORD fields[7] = {0};
		ZeroMemory(value,sizeof(*value));
		INT length = RStrLen(text);
		CheckIf(!text || !RStrIsWide(text) || (dateOnly ? length != 10 : length != 10 && length != 19 && length != 23),E_INVALIDARG);
		PCWSTR input = RStrToWide(text);
		CheckIf(input[4] != L'-' || input[7] != L'-',E_INVALIDARG);
		CheckIf(length > 10 && (input[10] != L'T' || input[13] != L':' || input[16] != L':'),E_INVALIDARG);
		CheckIf(length == 23 && input[19] != L'.',E_INVALIDARG);
		UINT count = length == 10 ? 3 : length == 19 ? 6 : 7;
		for(UINT n = 0; n < count; n++)
		{
			for(UINT digit = 0; digit < widths[n]; digit++)
			{
				WCHAR ch = input[offsets[n] + digit];
				CheckIf(ch < L'0' || ch > L'9',E_INVALIDARG);
				fields[n] = static_cast<WORD>(fields[n] * 10 + ch - L'0');
			}
		}
		value->wYear = fields[0];
		value->wMonth = fields[1];
		value->wDay = fields[2];
		value->wHour = fields[3];
		value->wMinute = fields[4];
		value->wSecond = fields[5];
		value->wMilliseconds = fields[6];
		CheckIf(!ValidTime(*value),E_INVALIDARG);
		FILETIME file;
		CheckIf(!SystemTimeToFileTime(value,&file) || !FileTimeToSystemTime(&file,value),E_INVALIDARG);
	Cleanup:
		return hr;
	}
	inline HRESULT TimeJSON(const SYSTEMTIME& value, BOOL dateOnly, IJSONValue** json)
	{
		HRESULT hr;
		CRString text;
		CheckIf(!ValidTime(value),E_INVALIDARG);
		Check(RStrFormatW(&text,L"%.4u-%.2u-%.2uT%.2u:%.2u:%.2u.%.3u",
			value.wYear,value.wMonth,value.wDay,value.wHour,value.wMinute,value.wSecond,value.wMilliseconds));
		Check(JSONCreateStringW(text.c_wstr(),dateOnly ? 10 : 23,json));
	Cleanup:
		return hr;
	}
	inline UINT DayNumber(const SYSTEMTIME& value)
	{
		return value.wYear * 10000 + value.wMonth * 100 + value.wDay;
	}
}
