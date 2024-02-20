#pragma once

#include <docobj.h>

interface ISimbeyInterchangeFile;

#define	VSIF_EXEC_OUT_ARG_INVALIDATE		1
#define	VSIF_EXEC_OUT_ARG_UPDATE_LAYERS		2
#define	VSIF_EXEC_OUT_ARG_UPDATE_RELAYOUT	4

interface __declspec(uuid("9E63671B-7A2D-4877-A02F-A7C375706CF1")) IVisualSIFMenu : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE AddCommand (IOleCommandTarget* pCmd, DWORD idCmd, PCWSTR pcwzLabel, INT cchLabel, BOOL fEnabled) = 0;
};

interface __declspec(uuid("16CB9902-776F-4239-81D3-764F41C53AE4")) IVisualSIFExtModule : IUnknown
{
	virtual VOID STDMETHODCALLTYPE Close (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddCanvasCommands (ISimbeyInterchangeFile* pSIF, IVisualSIFMenu* pMenu) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddLayerCommands (ISimbeyInterchangeFile* pSIF, DWORD idLayer, IVisualSIFMenu* pMenu) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetImportFilter (INT idxFilter, __out_ecount(cchMaxName) PWSTR pwzName, INT cchMaxName, __out_ecount(cchMaxFilter) PWSTR pwzFilter, INT cchMaxFilter) = 0;
	virtual HRESULT STDMETHODCALLTYPE ImportFile (ISimbeyInterchangeFile* pSIF, PCWSTR pcwzImportFile, PCWSTR pcwzExt) = 0;
};
