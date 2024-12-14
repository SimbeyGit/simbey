#include "MapConvert.h"

class CMapThing
{
public:
	SHORT m_ID;
	SHORT m_x;
	SHORT m_y;
	SHORT m_z;
	SHORT m_Angle;
	SHORT m_Type;
	SHORT m_Options;
	BYTE m_Special;
	BYTE m_Args[5];

public:
	CMapThing (CMapThing* lpThing);
	CMapThing (LPTHING lpThing);
	CMapThing ();
	~CMapThing ();

	VOID Reset (VOID);

	VOID operator= (CMapThing& Thing);

	LPTHING GetMapThing (LPTHING lpThing);
};
