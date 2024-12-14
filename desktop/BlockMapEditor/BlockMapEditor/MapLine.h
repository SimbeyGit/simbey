#pragma once

#include "MapConvert.h"

class CMapLine
{
public:
	VERTEX m_vFrom;
	VERTEX m_vTo;
	SHORT m_Flags;
	SHORT m_Types;
	SHORT m_Tag;
	SIDEDEF m_sRight;
	SIDEDEF m_sLeft;
	BYTE m_Special;
	BYTE m_Args[5];

public:
	CMapLine (CMapLine* lpLine);
	CMapLine (LPMAPLINE lpLine);
	CMapLine ();
	~CMapLine ();

	VOID Reset (VOID);

	VOID operator= (CMapLine& Line);

	LPMAPLINE GetMapLine (LPMAPLINE lpLine);
};
