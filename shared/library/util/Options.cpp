#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "..\Core\MemoryStream.h"
#include "Formatting.h"
#include "Options.h"

COptions::COptions ()
{
}

COptions::~COptions ()
{
	Clear();
}

VOID COptions::Clear (VOID)
{
	for(sysint i = 0; i < m_aOptions.Length(); i++)
		RStrRelease(m_aOptions[i]);
	m_aOptions.Clear();
}

//
// There is no command line parsing available for Windows apps in CE, so we do this 
// ourselves.  There is a lot of attention here to quoted paths, but for now, we
// have no need to support this.  Passing a quoted path to some Win32 API functions
// results in an error.  Removing the quotes results in success.  So, for now, 
// FreestyleUpdate recommends not using quoted paths.
//
HRESULT COptions::Parse (PCWSTR pcwzOptions)
{
	HRESULT hr = S_FALSE;
	BOOL fQuoted = FALSE;
	PCWSTR pcwzStart = NULL;
	RSTRING rstrOption = NULL;

	for(;;)
	{
		if(NULL == pcwzStart)
		{
			if(!Formatting::IsSpace(*pcwzOptions))
			{
				if(L'"' == *pcwzOptions)
					fQuoted = TRUE;
				pcwzStart = pcwzOptions;
			}
		}
		else if(L'"' == *pcwzOptions && !fQuoted)
			fQuoted = TRUE;
		else if((!fQuoted && Formatting::IsSpace(*pcwzOptions)) || (fQuoted && L'"' == *pcwzOptions) || L'\0' == *pcwzOptions)
		{
			INT cchOption = static_cast<INT>(pcwzOptions - pcwzStart);

			if(fQuoted)
			{
				if(L'"' == *pcwzStart)
				{
					// For options that are entirely quoted, remove the quotes.
					pcwzStart++;
					cchOption--;
				}
				else if(L'"' == *pcwzOptions)
				{
					// For options that were quoted part way through, include the quotes.
					cchOption++;
				}
				fQuoted = FALSE;
			}

			Check(RStrCreateW(cchOption, pcwzStart, &rstrOption));
			Check(m_aOptions.Append(rstrOption));
			rstrOption = NULL;

			pcwzStart = NULL;
		}

		if(L'\0' == *pcwzOptions)
			break;
		pcwzOptions++;
	}

Cleanup:
	return hr;
}

BOOL COptions::FindParam (PCWSTR pcwzParam, __out_opt sysint* pnPosition)
{
	for(sysint i = 0; i < m_aOptions.Length(); i++)
	{
		RSTRING rstrArg = m_aOptions[i];
		PCWSTR pcwzArg = RStrToWide(rstrArg);

		if(L'-' == pcwzArg[0] || L'/' == pcwzArg[0])
		{
			if(0 == TStrCmpIAssert(pcwzArg + 1, pcwzParam))
			{
				if(pnPosition)
					*pnPosition = i;
				return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL COptions::FindAndRemoveParam (PCWSTR pcwzParam)
{
	sysint n;
	BOOL fSuccess = FindParam(pcwzParam, &n);
	if(fSuccess)
	{
		RStrRelease(m_aOptions[n]);
		SideAssertHr(m_aOptions.RemoveChecked(n, NULL));
	}
	return fSuccess;
}

BOOL COptions::GetParamValue (PCWSTR pcwzParam, __deref_out RSTRING* pprstrValue)
{
	sysint n;
	return GetParamValueInternal(pcwzParam, pprstrValue, &n);
}

BOOL COptions::GetAndRemoveParamValue (PCWSTR pcwzParam, __deref_out RSTRING* pprstrValue)
{
	sysint n;
	BOOL fSuccess = GetParamValueInternal(pcwzParam, pprstrValue, &n);
	if(fSuccess)
	{
		SideAssertHr(m_aOptions.RemoveChecked(n + 1, NULL));

		RStrRelease(m_aOptions[n]);
		SideAssertHr(m_aOptions.RemoveChecked(n, NULL));
	}
	return fSuccess;
}

HRESULT COptions::RemoveArg (INT nArg, __out RSTRING* prstrArg)
{
	HRESULT hr;

	CheckIf(NULL == prstrArg, E_INVALIDARG);
	Check(m_aOptions.RemoveChecked(nArg, prstrArg));

Cleanup:
	return hr;
}

HRESULT COptions::CollapseToString (__out RSTRING* prstrCommands)
{
	HRESULT hr;
	CMemoryStream stmCommands;

	for(sysint i = 0; i < m_aOptions.Length(); i++)
	{
		RSTRING rstrArg = m_aOptions[i];
		INT cchArg = RStrLen(rstrArg);
		BOOL fUseQuotes = FALSE;
		ULONG cb;

		for(INT n = 0; n < cchArg; n++)
		{
			if(Formatting::IsSpace(RStrToWide(rstrArg)[n]))
			{
				fUseQuotes = TRUE;
				break;
			}
		}

		if(0 < stmCommands.DataRemaining())
		{
			Check(stmCommands.TWrite(SLP(L" "), &cb));
		}

		if(fUseQuotes)
		{
			Check(stmCommands.TWrite(SLP(L"\""), &cb));
			Check(stmCommands.TWrite(RStrToWide(rstrArg), cchArg, &cb));
			Check(stmCommands.TWrite(SLP(L"\""), &cb));
		}
		else
		{
			Check(stmCommands.TWrite(RStrToWide(rstrArg), cchArg, &cb));
		}
	}

	Check(RStrCreateW(stmCommands.DataRemaining() / sizeof(WCHAR), stmCommands.TGetReadPtr<WCHAR>(), prstrCommands));

Cleanup:
	return hr;
}

BOOL COptions::GetParamValueInternal (PCWSTR pcwzParam, __deref_out RSTRING* pprstrValue, __out sysint* pnPosition)
{
	sysint i;
	if(FindParam(pcwzParam, &i))
	{
		if(i + 1 == m_aOptions.Length())
			return FALSE;
		RSTRING rstrArg = m_aOptions[i + 1];
		PCWSTR pcwzArg = RStrToWide(rstrArg);
		if(L'-' == pcwzArg[0] || L'/' == pcwzArg[0])
			return FALSE;
		RStrSet(*pprstrValue, rstrArg);
		*pnPosition = i;
		return TRUE;
	}
	return FALSE;
}
