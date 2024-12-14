#include "MapConvert.h"

class CMapLinedef
{
public:
	SHORT m_vFrom;
	SHORT m_vTo;
	SHORT m_Flags;
	SHORT m_Types;
	SHORT m_Tag;
	SHORT m_sRight;
	SHORT m_sLeft;
	BYTE m_Special;
	BYTE m_Args[5];

public:
	CMapLinedef (CMapLinedef* lpLinedef);
	CMapLinedef (LPLINEDEF lpLinedef);
	CMapLinedef ();
	~CMapLinedef ();

	VOID Reset (VOID);

	VOID operator= (CMapLinedef& Linedef);

	LPLINEDEF GetMapLinedef (LPLINEDEF lpLinedef);
};
