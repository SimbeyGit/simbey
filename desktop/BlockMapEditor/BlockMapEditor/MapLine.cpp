#include <windows.h>
#include "MapLine.h"

CMapLine::CMapLine (CMapLine* lpLine)
{
	*this = *lpLine;
}

CMapLine::CMapLine (LPMAPLINE lpLine)
{
	CopyMemory(&m_vFrom,&lpLine->vFrom,sizeof(VERTEX));
	CopyMemory(&m_vTo,&lpLine->vTo,sizeof(VERTEX));
	m_Flags = lpLine->Flags;
	m_Types = 0;
	m_Tag = 0;
	CopyMemory(&m_sRight,&lpLine->sRight,sizeof(SIDEDEF));
	CopyMemory(&m_sLeft,&lpLine->sLeft,sizeof(SIDEDEF));
	m_Special = lpLine->Special;
	CopyMemory(m_Args,lpLine->Args,sizeof(m_Args));
}

CMapLine::CMapLine ()
{
	Reset();
}

CMapLine::~CMapLine ()
{
}

VOID CMapLine::Reset (VOID)
{
	ZeroMemory(&m_vFrom,sizeof(VERTEX));
	ZeroMemory(&m_vTo,sizeof(VERTEX));
	m_Flags = 0;
	m_Types = 0;
	m_Tag = 0;
	ZeroMemory(&m_sRight,sizeof(SIDEDEF));
	ZeroMemory(&m_sLeft,sizeof(SIDEDEF));
	m_Special = 0;
	ZeroMemory(m_Args,sizeof(m_Args));
}

VOID CMapLine::operator= (CMapLine& Line)
{
	CopyMemory(&m_vFrom,&Line.m_vFrom,sizeof(VERTEX));
	CopyMemory(&m_vTo,&Line.m_vTo,sizeof(VERTEX));
	m_Flags = Line.m_Flags;
	m_Types = Line.m_Types;
	m_Tag = Line.m_Tag;
	CopyMemory(&m_sRight,&Line.m_sRight,sizeof(SIDEDEF));
	CopyMemory(&m_sLeft,&Line.m_sLeft,sizeof(SIDEDEF));
	m_Special = Line.m_Special;
	CopyMemory(m_Args,Line.m_Args,sizeof(m_Args));
}

LPMAPLINE CMapLine::GetMapLine (LPMAPLINE lpLine)
{
	CopyMemory(&lpLine->vFrom,&m_vFrom,sizeof(VERTEX));
	CopyMemory(&lpLine->vTo,&m_vTo,sizeof(VERTEX));
	lpLine->Flags = m_Flags;
	CopyMemory(&lpLine->sRight,&m_sRight,sizeof(SIDEDEF));
	CopyMemory(&lpLine->sLeft,&m_sLeft,sizeof(SIDEDEF));
	lpLine->Special = m_Special;
	CopyMemory(lpLine->Args,m_Args,sizeof(m_Args));
	return lpLine;
}
