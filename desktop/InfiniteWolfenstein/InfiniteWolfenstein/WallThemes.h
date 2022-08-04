#pragma once

#include "TemplateData.h"

class CWallTextures;

class CWallTheme
{
public:
	sysint* m_pidxWalls;
	sysint m_cWalls;

	sysint* m_pidxDecorations;
	sysint m_cDecorations;

public:
	CWallTheme ();
	~CWallTheme ();
};

class CWallThemeNamespace
{
private:
	TRStrMap<CWallTheme*> m_mapWallThemes;

public:
	CWallThemeNamespace (RSTRING rstrNamespaceW);
	~CWallThemeNamespace ();

	HRESULT Add (CWallTextures* pWalls, RSTRING rstrNameW, IJSONObject* pItem);
	HRESULT Find (RSTRING rstrNameW, __deref_out CWallTheme** ppItem);

private:
	HRESULT LoadTextures (CWallTextures* pWalls, IJSONObject* pItem, PCWSTR pcwzList, __out sysint* pcTextures, __deref_out sysint** ppidxTextures);
};

typedef TTemplateData<CWallThemeNamespace, CWallTextures, CWallTheme> CWallThemes;
