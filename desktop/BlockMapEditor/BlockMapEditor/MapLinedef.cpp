#include <windows.h>
#include "MapLinedef.h"

CMapLinedef::CMapLinedef (CMapLinedef* lpLinedef)
{
	*this = *lpLinedef;
}

CMapLinedef::CMapLinedef (LPLINEDEF lpLinedef)
{
	m_vFrom = lpLinedef->vFrom;
	m_vTo = lpLinedef->vTo;
	m_Flags = lpLinedef->Flags;
	m_sRight = lpLinedef->sRight;
	m_sLeft = lpLinedef->sLeft;
	m_Special = lpLinedef->Special;
	CopyMemory(m_Args,lpLinedef->Args,sizeof(m_Args));
}

CMapLinedef::CMapLinedef ()
{
	Reset();
}

CMapLinedef::~CMapLinedef ()
{
}

VOID CMapLinedef::Reset (VOID)
{
	m_vFrom = 0;
	m_vTo = 0;
	m_Flags = 0;
	m_Types = 0;
	m_Tag = 0;
	m_sRight = 0;
	m_sLeft = 0;
	m_Special = 0;
	ZeroMemory(m_Args,sizeof(m_Args));
}

VOID CMapLinedef::operator= (CMapLinedef& Linedef)
{
	m_vFrom = Linedef.m_vFrom;
	m_vTo = Linedef.m_vTo;
	m_Flags = Linedef.m_Flags;
	m_Types = Linedef.m_Types;
	m_Tag = Linedef.m_Tag;
	m_sRight = Linedef.m_sRight;
	m_sLeft = Linedef.m_sLeft;
	m_Special = Linedef.m_Special;
	CopyMemory(m_Args,Linedef.m_Args,sizeof(m_Args));
}

LPLINEDEF CMapLinedef::GetMapLinedef (LPLINEDEF lpLinedef)
{
	lpLinedef->vFrom = m_vFrom;
	lpLinedef->vTo = m_vTo;
	lpLinedef->Flags = m_Flags;
	lpLinedef->sRight = m_sRight;
	lpLinedef->sLeft = m_sLeft;
	lpLinedef->Special = m_Special;
	CopyMemory(lpLinedef->Args,m_Args,sizeof(m_Args));
	return lpLinedef;
}
