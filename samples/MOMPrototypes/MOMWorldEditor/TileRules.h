#pragma once

#include "Library\Core\RStrMap.h"
#include "Published\JSON.h"

class CSmoothingCondition
{
private:
	INT* m_pnDirections;
	INT m_cDirections;
	INT m_cRepetitions;
	INT m_nValue;

public:
	CSmoothingCondition ();
	~CSmoothingCondition ();

	HRESULT Initialize (IJSONObject* pCondition);
};

class CSmoothingSet
{
private:
	INT m_nDirection;
	INT m_nValue;

public:
	CSmoothingSet ();
	~CSmoothingSet ();

	HRESULT Initialize (IJSONObject* pSet);
};

class CSmoothingReduction
{
private:
	RSTRING m_rstrDescription;
	TArray<CSmoothingCondition*> m_aConditions;
	TArray<CSmoothingSet*> m_aSets;

public:
	CSmoothingReduction ();
	~CSmoothingReduction ();

	HRESULT Initialize (IJSONObject* pReduction);
};

class CSmoothingSystem
{
private:
	RSTRING m_rstrDescription;
	INT m_nMaxValueEachDirection;
	TArray<CSmoothingReduction*> m_aReductions;

public:
	CSmoothingSystem ();
	~CSmoothingSystem ();

	HRESULT Initialize (IJSONObject* pSystem);
};

class CTileRuleSet
{
private:
	IJSONArray* m_pSame;
	IJSONArray* m_pBorder;
	IJSONArray* m_pSpecial;

	IJSONArray* m_pAltTiles;
	RSTRING m_rstrTransition;

	CSmoothingSystem* m_pSmoothingSystem;

public:
	CTileRuleSet ();
	~CTileRuleSet ();

	HRESULT Initialize (TRStrMap<CSmoothingSystem*>& mapSmoothingSystems, IJSONValue* pvRuleSet);
	bool IsSameTile (RSTRING rstrTile);
	bool IsBorderTile (RSTRING rstrTile);
	bool IsSpecialTile (RSTRING rstrTile);
	HRESULT GetAltTile (RSTRING rstrKey, __deref_out RSTRING* prstrTile);
	inline RSTRING GetTransition (VOID) { return m_rstrTransition; }

private:
	bool IsCompatibleTile (__in_opt IJSONArray* pArray, RSTRING rstrTile);
};

class CTileRules
{
private:
	TRStrMap<CTileRuleSet*> m_mapTiles;

public:
	CTileRules ();
	~CTileRules ();

	HRESULT Initialize (TRStrMap<CSmoothingSystem*>& mapSmoothingSystems, IJSONValue* pvRules);
	HRESULT GetTileRuleSet (RSTRING rstrTile, __deref_out CTileRuleSet** ppTileRuleSet);
};
