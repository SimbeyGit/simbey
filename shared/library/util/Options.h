#pragma once

#include "..\Core\Array.h"
#include "RString.h"

class COptions
{
private:
	TArray<RSTRING> m_aOptions;

public:
	COptions ();
	~COptions ();

	inline sysint Length (VOID) { return m_aOptions.Length(); }
	inline RSTRING& operator[] (sysint n) { return m_aOptions[n]; }

	VOID Clear (VOID);
	HRESULT Parse (PCWSTR pcwzOptions);
	BOOL FindParam (PCWSTR pcwzParam, __out_opt sysint* pnPosition = NULL);
	BOOL FindAndRemoveParam (PCWSTR pcwzParam);
	BOOL GetParamValue (PCWSTR pcwzParam, __deref_out RSTRING* pprstrValue);
	BOOL GetAndRemoveParamValue (PCWSTR pcwzParam, __deref_out RSTRING* pprstrValue);
	HRESULT RemoveArg (INT nArg, __out RSTRING* prstrArg);
	HRESULT CollapseToString (__out RSTRING* prstrCommands);

private:
	BOOL GetParamValueInternal (PCWSTR pcwzParam, __deref_out RSTRING* pprstrValue, __out sysint* pnPosition);
};
