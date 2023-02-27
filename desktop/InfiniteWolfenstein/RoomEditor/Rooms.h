#pragma once

#include <SIFRibbon.h>
#include "Library\Core\Array.h"
#include "Published\JSON.h"

#define	COLOR_TUNNEL_START		RGB(0, 255, 0)
#define	COLOR_CONNECTOR			RGB(255, 0, 255)
#define	COLOR_ANCHOR			RGB(8, 12, 80)
#define	COLOR_FLOOR				RGB(224, 224, 224)
#define	COLOR_WALL				RGB(64, 64, 64)
#define	COLOR_DECORATION		RGB(128, 80, 80)
#define	COLOR_DOOR				RGB(255, 255, 0)
#define	COLOR_ELEVATOR_DOOR		RGB(172, 8, 64)
#define	COLOR_GOLD_LOCKED		RGB(224, 180, 32)
#define	COLOR_SILVER_LOCKED		RGB(0, 255, 255)
#define	COLOR_RAILING			RGB(160, 88, 20)
#define	COLOR_SWITCH			RGB(60, 120, 120)
#define	COLOR_SPLIT_DOOR		RGB(192, 192, 0)

interface IGrapher;

struct PAINTABLE
{
	INT x, z;
	COLORREF cr;
	IJSONObject* pEntity;
};

class CRoom
{
public:
	RSTRING m_rstrName;
	TArray<PAINTABLE> m_aCells;
	IJSONArray* m_pCells;
	bool m_fEnableRotation;

public:
	CRoom ();
	~CRoom ();

	HRESULT Initialize (RSTRING rstrName, IJSONArray* pPalette, IJSONArray* pData);
	HRESULT RemoveCell (INT x, INT z);
	HRESULT AddCell (INT x, INT z, IJSONObject* pType);
	HRESULT SetEntity (INT x, INT z, IJSONObject* pEntity);
	HRESULT RemoveEntity (INT x, INT z);
	HRESULT Serialize (__deref_out IJSONValue** ppvRoom);
	HRESULT GetCellData (INT x, INT z, __deref_out IJSONObject** ppCell, __deref_out IJSONObject** ppEntity);

private:
	HRESULT FindCell (INT x, INT z, __out sysint* pidxCell);
	HRESULT CopyFromPalette (IJSONArray* pPalette, IJSONObject* pData);
	HRESULT ReformatToPalette (RSTRING rstrTypeName, IJSONObject* pData, IJSONArray* pPalette);
};

class CRooms
{
public:
	TArray<CRoom*> m_aRooms;
	CRoom* m_pSelected;

	HBRUSH m_hbrTunnelStart;
	HBRUSH m_hbrConnector;
	HBRUSH m_hbrAnchor;
	HBRUSH m_hbrFloor;
	HBRUSH m_hbrWall;
	HBRUSH m_hbrDecoration;
	HBRUSH m_hbrDoor;
	HBRUSH m_hbrElevatorDoor;
	HBRUSH m_hbrGoldLocked;
	HBRUSH m_hbrSilverLocked;
	HBRUSH m_hbrSplitDoor;
	HBRUSH m_hbrRailing;
	HBRUSH m_hbrSwitch;

public:
	CRooms ();
	~CRooms ();

	HRESULT Initialize (VOID);
	HRESULT AddRoom (__out sysint* pidxRoom);
	HRESULT AddRoom (RSTRING rstrName, IJSONObject* pRoomDef, __out_opt sysint* pidxRoom);
	HRESULT DeleteRoom (sysint idxRoom);
	HRESULT SelectRoom (sysint idxRoom);
	HRESULT GetRoomIndex (__out sysint* pidxRoom);
	HRESULT UpdateSelectionData (CSIFRibbon* pRibbon, REFPROPERTYKEY key, const PROPVARIANT* currentValue, PROPVARIANT* newValue);
	BOOL HasSelection (VOID) { return m_pSelected ? TRUE : FALSE; }
	BOOL Paint (IGrapher* pGrapher);
	HRESULT SetCellData (FLOAT rx, FLOAT rz, IJSONObject* pType);
	HRESULT GetCellData (INT x, INT z, __deref_out IJSONObject** ppCell, __deref_out IJSONObject** ppEntity);
	HRESULT Load (PCWSTR pcwzFile);
	HRESULT Save (PCWSTR pcwzFile);
};
