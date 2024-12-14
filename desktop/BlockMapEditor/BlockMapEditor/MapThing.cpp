#include <windows.h>
#include "MapThing.h"

CMapThing::CMapThing (CMapThing* lpThing)
{
	*this = *lpThing;
}

CMapThing::CMapThing (LPTHING lpThing)
{
	m_ID = lpThing->ID;
	m_x = lpThing->x;
	m_y = lpThing->y;
	m_z = lpThing->z;
	m_Angle = lpThing->Angle;
	m_Type = lpThing->Type;
	m_Options = lpThing->Options;
	m_Special = lpThing->Special;
	CopyMemory(m_Args,lpThing->Args,sizeof(m_Args));
}

CMapThing::CMapThing ()
{
	Reset();
}

CMapThing::~CMapThing ()
{
}

VOID CMapThing::Reset (VOID)
{
	m_ID = 0;
	m_x = 0;
	m_y = 0;
	m_z = 0;
	m_Angle = 0;
	m_Type = 0;
	m_Options = 0;
	m_Special = 0;
	ZeroMemory(m_Args,sizeof(m_Args));
}

VOID CMapThing::operator= (CMapThing& Thing)
{
	m_ID = Thing.m_ID;
	m_x = Thing.m_x;
	m_y = Thing.m_y;
	m_z = Thing.m_z;
	m_Angle = Thing.m_Angle;
	m_Type = Thing.m_Type;
	m_Options = Thing.m_Options;
	m_Special = Thing.m_Special;
	CopyMemory(m_Args,Thing.m_Args,sizeof(m_Args));
}

LPTHING CMapThing::GetMapThing (LPTHING lpThing)
{
	lpThing->ID = m_ID;
	lpThing->x = m_x;
	lpThing->y = m_y;
	lpThing->z = m_z;
	lpThing->Angle = m_Angle;
	lpThing->Type = m_Type;
	lpThing->Options = m_Options;
	lpThing->Special = m_Special;
	CopyMemory(lpThing->Args,m_Args,sizeof(m_Args));
	return lpThing;
}
