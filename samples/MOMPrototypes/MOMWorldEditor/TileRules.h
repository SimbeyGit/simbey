#pragma once

#include "Library\Core\RStrMap.h"
#include "Published\JSON.h"

class CTileRuleSet
{
private:
	IJSONArray* m_pSame;
	IJSONArray* m_pBorder;
	IJSONArray* m_pSpecial;

	IJSONArray* m_pAltTiles;
	RSTRING m_rstrTransition;

public:
	CTileRuleSet ();
	~CTileRuleSet ();

	HRESULT Initialize (IJSONValue* pvRuleSet);
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

	HRESULT Initialize (IJSONValue* pvRules);
	HRESULT GetTileRuleSet (RSTRING rstrTile, __deref_out CTileRuleSet** ppTileRuleSet);
};
