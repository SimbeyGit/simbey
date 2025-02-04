#pragma once

#include "Library\Core\Map.h"
#include "Library\Core\MemoryStream.h"
#include "Library\GraphCtrl.h"

interface IResolveItemPalette;

class CPaintItem;

#define	MAP_WIDTH		256
#define	MAP_HEIGHT		256

#define	CELL_SCALE		16.0f

struct MAP_BLOCK
{
	CPaintItem* pBlock;
	CPaintItem* pObject;
	SHORT nObjectFlags;		// 1 - Easy, 2 - Medium, 4 - Hard
};

class CBaseUndoRedo
{
public:
	virtual ~CBaseUndoRedo () {}

	virtual VOID Undo (class CBlockMap* pMap) = 0;
	virtual VOID Redo (class CBlockMap* pMap) = 0;
};

class CUndoRedoObjectFlags : public CBaseUndoRedo
{
public:
	INT m_idxCell;
	SHORT m_nOldObjectFlags;
	SHORT m_nNewObjectFlags;

public:
	CUndoRedoObjectFlags (INT idxCell, SHORT nOldFlags, SHORT nNewFlags) : m_idxCell(idxCell), m_nOldObjectFlags(nOldFlags), m_nNewObjectFlags(nNewFlags) {}

	virtual VOID Undo (class CBlockMap* pMap);
	virtual VOID Redo (class CBlockMap* pMap);
};

class CUndoRedoCellData : public CBaseUndoRedo
{
public:
	struct CELLDATA
	{
		CPaintItem* pOld;
		INT idxCell;
	};
	TArray<CELLDATA> m_aOld;
	CPaintItem* m_pNew;

public:
	CUndoRedoCellData (CPaintItem* pNew) : m_pNew(pNew) {}

	virtual VOID Undo (class CBlockMap* pMap);
	virtual VOID Redo (class CBlockMap* pMap);
};

class CBlockMap
{
private:
	INT m_nLighting;
	USHORT m_nHighestFloor;
	MAP_BLOCK* m_pMap;

	TArray<CBaseUndoRedo*> m_aChangeStack;
	sysint m_idxChangePtr;

public:
	CBlockMap ();
	~CBlockMap ();

	HRESULT Initialize (VOID);
	INT GetLighting (VOID) { return m_nLighting; }
	VOID SetLighting (INT nLighting) { m_nLighting = nLighting; }
	VOID Paint (IGrapher* pGraph);
	HRESULT Load (IResolveItemPalette* pResolve, PCWSTR pcwzFile, PWSTR pwzCeiling, PWSTR pwzFloor);
	HRESULT Save (PCWSTR pcwzFile, PCWSTR pcwzCeiling, PCWSTR pcwzFloor);
	USHORT GetHighestFloor (VOID);
	USHORT AddFloor (VOID);
	HRESULT SetCellData (FLOAT x, FLOAT z, CPaintItem* pType);
	HRESULT GetCellData (INT xCell, INT zCell, __deref_out CPaintItem** ppBlock, CPaintItem** ppObject);
	HRESULT GetObjectFlags (INT xCell, INT zCell, __out SHORT* pnObjectFlags);
	HRESULT SetObjectFlags (INT xCell, INT zCell, SHORT nObjectFlags);
	BOOL IsSolid (INT xCell, INT zCell);
	USHORT GetFloor (INT xCell, INT zCell);
	HRESULT FindStartSpot (__out INT& xCell, __out INT& zCell);
	HRESULT ReplaceWall (CPaintItem* pOld, CPaintItem* pNew);

	BOOL CanUndo (VOID) { return 0 < m_idxChangePtr; }
	BOOL CanRedo (VOID) { return m_idxChangePtr < m_aChangeStack.Length(); }

	HRESULT Undo (VOID);
	HRESULT Redo (VOID);

	CPaintItem* InternalSetCellData (INT idxCell, __in_opt CPaintItem* pType);
	VOID InternalSetObjectFlags (INT idxCell, SHORT nObjectFlags);

private:
	HRESULT InsertUndoItem (CBaseUndoRedo* pItem);
	HRESULT LoadItemPalette (IResolveItemPalette* pResolve, ISequentialStream* pPalette, DWORD cbPalette, TArray<CPaintItem*>& aPalette);

	static HRESULT InsertStream (ISequentialStream* pstmTarget, CMemoryStream* pstmSource);
	static HRESULT SerializeItem (TMap<CPaintItem*, DWORD>& mapPalette, TArray<CPaintItem*>& aPalette, CMemoryStream* pstmData, CPaintItem* pItem);
};
