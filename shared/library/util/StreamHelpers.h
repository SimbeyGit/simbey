#pragma once

class CMemoryStream;

namespace Stream
{
	HRESULT WINAPI CopyAnsiToStreamW (UINT nCodePage, ISequentialStream* pStream, PCSTR pcszString, INT cchString, INT cchCopy);
	HRESULT WINAPI CopyWideToStreamA (UINT nCodePage, ISequentialStream* pStream, PCWSTR pcwzString, INT cchString, INT cchCopy);

	template <typename T>
	HRESULT TPrintVFEx (UINT nCodePage, ISequentialStream* pStream, const T* pctzFormat, va_list vArgs)
	{
		// This template must not be used.  Either TPrintVFEx<CHAR> or TPrintVFEx<WCHAR> should be selected.
		Assert(false);
		return E_NOTIMPL;
	}

	template <>
	HRESULT TPrintVFEx<CHAR> (UINT nCodePage, ISequentialStream* pStream, PCSTR pctzFormat, va_list vArgs);

	template <>
	HRESULT TPrintVFEx<WCHAR> (UINT nCodePage, ISequentialStream* pStream, PCWSTR pctzFormat, va_list vArgs);

	template <typename T>
	HRESULT TPrintFEx (UINT nCodePage, ISequentialStream* pStream, const T* pctzFormat, ...)
	{
		HRESULT hr;

		va_list vArgs;
		va_start(vArgs, pctzFormat);
		hr = TPrintVFEx(nCodePage, pStream, pctzFormat, vArgs);
		va_end(vArgs);

		return hr;
	}

	template <typename T>
	HRESULT TPrintVF (ISequentialStream* pStream, const T* pctzFormat, va_list vArgs)
	{
		// This template must not be used.  Either TPrintVF<CHAR> or TPrintVF<WCHAR> should be selected.
		Assert(false);
		return E_NOTIMPL;
	}

	template <>
	HRESULT TPrintVF<CHAR> (ISequentialStream* pStream, PCSTR pctzFormat, va_list vArgs);

	template <>
	HRESULT TPrintVF<WCHAR> (ISequentialStream* pStream, PCWSTR pctzFormat, va_list vArgs);

	template <typename T>
	HRESULT TPrintF (ISequentialStream* pStream, const T* pctzFormat, ...)
	{
		HRESULT hr;

		va_list vArgs;
		va_start(vArgs, pctzFormat);
		hr = TPrintVF(pStream, pctzFormat, vArgs);
		va_end(vArgs);

		return hr;
	}

	template <typename T>
	HRESULT TWrite (__in ISequentialStream* pStream, __in_ecount(cElements) const T* pctv, INT cElements, ULONG* pcbWritten)
	{
		return pStream->Write(pctv, cElements * sizeof(T), pcbWritten);
	}

	HRESULT WriteWideToCodePage (CMemoryStream* pstmDest, UINT nCodePage, DWORD dwFlags, INT cchWide, PCWSTR pcwzWide);
}
