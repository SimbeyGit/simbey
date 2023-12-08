#pragma once

#include "..\..\..\Shared\Library\Util\RString.h"
#include "..\..\..\Shared\Library\Util\StreamCopy.h"

interface __declspec(uuid("03CAACF8-4C42-464c-901E-C5C0AA8F94F3")) IRStringArray : IUnknown
{
	virtual sysint STDMETHODCALLTYPE Length (VOID) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetRString (sysint nString, __out RSTRING* prstrString) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetStringW (sysint nString, __out_ecount(cchMaxString) PWSTR pwzString, INT cchMaxString, INT* pcchString) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetStringA (sysint nString, __out_ecount(cchMaxString) PSTR pszString, INT cchMaxString, INT* pcchString) = 0;

	virtual HRESULT STDMETHODCALLTYPE AddRString (RSTRING rstrString) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddStringW (PCWSTR pcwzString, INT cchString) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddStringA (PCSTR pcszString, INT cchString, INT nCodePage) = 0;

	virtual HRESULT STDMETHODCALLTYPE InsertAt (RSTRING rstrString, sysint nInsert) = 0;
	virtual HRESULT STDMETHODCALLTYPE Replace (sysint nString, RSTRING rstrString) = 0;
	virtual HRESULT STDMETHODCALLTYPE Swap (sysint nStringA, sysint nStringB) = 0;

	virtual RSTRING* STDMETHODCALLTYPE GetArray (__out sysint* pcStrings) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveString (sysint nString, __out_opt RSTRING* prstrString) = 0;
	virtual VOID STDMETHODCALLTYPE RemoveAll (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE Compact (VOID) = 0;

	virtual HRESULT STDMETHODCALLTYPE IndexOf (RSTRING rstrFind, __out sysint* pidxFound, sysint idxStart = 0) = 0;
	virtual HRESULT STDMETHODCALLTYPE Clone (__deref_out IRStringArray** ppClone) = 0;
	virtual HRESULT STDMETHODCALLTYPE Compare (IRStringArray* pOther, __out INT* pnCompare) = 0;
};

interface __declspec(uuid("F0F4ACB4-A594-46b2-B7E2-FB8E3A88D0E0")) ITempFileSet : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetTempFile (PCWSTR pcwzHint, __out RSTRING* prstrTempFile) = 0;
	virtual HRESULT STDMETHODCALLTYPE DuplicateFile (PCWSTR pcwzSource, __out RSTRING* prstrTempFile) = 0;
	virtual HRESULT STDMETHODCALLTYPE DuplicateFileWithHint (PCWSTR pcwzSource, PCWSTR pcwzHint, __out RSTRING* prstrTempFile) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetTempFileFromStream (ISequentialStream* pstmSource, ULONG cbStream, PCWSTR pcwzHint, __out RSTRING* prstrTempFile) = 0;
};

interface __declspec(uuid("BDC4A2B7-78BC-4616-BC0C-B53368D5725D")) ITempFileManager : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE ClearFolder (__out_opt INT* pcFilesDeleted) = 0;
	virtual HRESULT STDMETHODCALLTYPE CreateFileSet (__deref_out ITempFileSet** ppSet) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFilePathFromName (PCWSTR pcwzName, __out RSTRING* prstrPath) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetBasePath (__deref_out RSTRING* prstrPath) = 0;
};

HRESULT WINAPI ScPrintVFExA (UINT nCodePage, __out_ecount(cchMaxOutput) PSTR pszOutput, INT cchMaxOutput, INT* pcchOutput, PCSTR pcszFormat, va_list vArgs);
HRESULT WINAPI ScPrintVFExW (UINT nCodePage, __out_ecount(cchMaxOutput) PWSTR pwzOutput, INT cchMaxOutput, INT* pcchOutput, PCWSTR pcwzFormat, va_list vArgs);
HRESULT WINAPI ScPrintVFA (__out_ecount(cchMaxOutput) PSTR pszOutput, INT cchMaxOutput, INT* pcchOutput, PCSTR pcszFormat, va_list vArgs);
HRESULT WINAPI ScPrintVFW (__out_ecount(cchMaxOutput) PWSTR pwzOutput, INT cchMaxOutput, INT* pcchOutput, PCWSTR pcwzFormat, va_list vArgs);
HRESULT WINAPI ScStreamVFExA (UINT nCodePage, ISequentialStream* pStream, PCSTR pcszFormat, va_list vArgs);
HRESULT WINAPI ScStreamVFExW (UINT nCodePage, ISequentialStream* pStream, PCWSTR pcwzFormat, va_list vArgs);
HRESULT WINAPI ScStreamVFA (ISequentialStream* pStream, PCSTR pcszFormat, va_list vArgs);
HRESULT WINAPI ScStreamVFW (ISequentialStream* pStream, PCWSTR pcwzFormat, va_list vArgs);

HRESULT WINAPI ScEncodeQuotedA (PCSTR pcszText, ISequentialStream* pStream, __out_opt DWORD* pcbStream);
HRESULT WINAPI ScEncodeBase64A (const BYTE* pcbData, INT cbData, ISequentialStream* pStream, __out_opt DWORD* pcbStream);
HRESULT WINAPI ScEncodeBase64LinesA (const BYTE* pcbData, INT cbData, ISequentialStream* pStream, __out_opt DWORD* pcbStream);
HRESULT WINAPI ScEncodeBase64UrlA (const BYTE* pcbData, INT cbData, ISequentialStream* pStream, __out_opt DWORD* pcbStream);
HRESULT WINAPI ScEncodeHexA (const BYTE* pcbData, INT cbData, ISequentialStream* pStream, __out_opt DWORD* pcbStream);
HRESULT WINAPI ScDecodeQuotedA (PCSTR pcszData, INT cchData, ISequentialStream* pStream, __out_opt DWORD* pcbStream);
HRESULT WINAPI ScDecodeBase64A (PCSTR pcszData, INT cchData, ISequentialStream* pStream, __out_opt DWORD* pcbStream);
HRESULT WINAPI ScDecodeBase64UrlA (PCSTR pcszData, INT cchData, ISequentialStream* pStream, __out_opt DWORD* pcbStream);
HRESULT WINAPI ScDecodeHexA (PCSTR pcszData, INT cchData, ISequentialStream* pStream, __out_opt DWORD* pcbStream);

HRESULT WINAPI ScCopyDoubleA (PSTR pszOutput, INT cchMaxOutput, DOUBLE fValue, INT cCopy, CHAR chFormat, __out INT* pcchCopied);
HRESULT WINAPI ScCopyDoubleW (PWSTR pwzOutput, INT cchMaxOutput, DOUBLE fValue, INT cCopy, WCHAR wchFormat, __out INT* pcchCopied);

HRESULT WINAPI ScFloatToStringA (FLOAT fValue, PSTR pszBuffer, INT cchMaxBuffer, INT cMaxPlaces, __out_opt INT* pcchWritten);
HRESULT WINAPI ScFloatToStringW (FLOAT fValue, PWSTR pwzBuffer, INT cchMaxBuffer, INT cMaxPlaces, __out_opt INT* pcchWritten);
HRESULT WINAPI ScDoubleToStringA (DOUBLE dblValue, PSTR pszBuffer, INT cchMaxBuffer, INT cMaxPlaces, __out_opt INT* pcchWritten);
HRESULT WINAPI ScDoubleToStringW (DOUBLE dblValue, PWSTR pwzBuffer, INT cchMaxBuffer, INT cMaxPlaces, __out_opt INT* pcchWritten);

HRESULT WINAPI ScCopyAnsiToStreamW (UINT nCodePage, ISequentialStream* pStream, PCSTR pcszString, INT cchString, INT cchCopy);
HRESULT WINAPI ScCopyWideToStreamA (UINT nCodePage, ISequentialStream* pStream, PCWSTR pcwzString, INT cchString, INT cchCopy);

HRESULT WINAPI ScCreateRStringArray (__deref_out IRStringArray** ppArray);
HRESULT WINAPI ScCreateTempFileManager (__in_opt PCWSTR pcwzBaseFolder, __in_opt PCWSTR pcwzSubFolder, BOOL fCreateRandomFolder, __deref_out ITempFileManager** ppTempFileManager);

HRESULT WINAPI ScReadContentTypeA (RSTRING rstrFileW, __out RSTRING* prstrContentTypeA);
HRESULT WINAPI ScReadContentTypeW (RSTRING rstrFileW, __out RSTRING* prstrContentTypeW);

HRESULT WINAPI ScCopyStream (ISequentialStream* pstmDest, ISequentialStream* pstmSrc, ULONGLONG ullCopy, __out_opt ULONGLONG* pullCopied, __in_opt Stream::CopyBuffer* pBuffer, __in_opt Stream::ICopyCallback* pCallback);

HRESULT WINAPI ScRegisterServer (HMODULE hModule, const IID& iidClass, PCWSTR pcwzProgID, PCWSTR pcwzModuleDescription);
HRESULT WINAPI ScUnregisterServer (const IID& iidClass, PCWSTR pcwzProgID);
HRESULT WINAPI ScAddImplementedCategories (const IID& iidClass, __in_ecount(cCategories) const GUID* pcrgCategories, INT cCategories);
