#pragma once

#include <SIFRibbon.h>
#include <BaseRibbonItem.h>
#include "Library\Core\MemoryStream.h"
#include "Library\GraphCtrl.h"

class CBlockMap;

namespace MapCell
{
	enum Type
	{
		Void,
		Wall,
		Floor,
		Object,
		Elevator,
		Door,
		Start,
		SecretDoor,
		WallCage,
		End,
		Sky
	};
};

struct TEXTURE
{
	PCWSTR pcwzName;
	INT xSize, ySize;
	CMemoryStream stmBits32;
	COLORREF crAverage;
};

struct ACTOR
{
	INT idActor;
	INT nDirection;
	TEXTURE* pTexture;
};

class __declspec(uuid("A0B51635-5CEB-4791-9309-CE512D2C57CA")) CPaintItem : public CBaseRibbonItem
{
public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(CPaintItem)
		UNK_CHAIN_TO(CBaseRibbonItem)
	END_UNK_MAP

protected:
	CPaintItem (CSIFRibbon* pRibbon);

public:
	virtual MapCell::Type GetType (VOID) = 0;
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z) = 0;
	virtual VOID InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y) = 0;
	virtual HRESULT Serialize (ISequentialStream* pstmDef) = 0;
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb) = 0;

protected:
	HRESULT SerializeType (ISequentialStream* pstmDef);
};

class CVoidItem : public CPaintItem
{
public:
	static HRESULT Create (CSIFRibbon* pRibbon, __deref_out CVoidItem** ppItem);

protected:
	CVoidItem (CSIFRibbon* pRibbon);
	~CVoidItem ();

public:
	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::Void; }
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z);
	virtual VOID InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y);
	virtual HRESULT Serialize (ISequentialStream* pstmDef) { return E_NOTIMPL; }
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb) { return FALSE; }

	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);
};

class CFloorItem : public CPaintItem
{
private:
	USHORT m_nFloor;

public:
	static HRESULT Create (CSIFRibbon* pRibbon, USHORT nFloor, PCWSTR pcwzName, __deref_out CFloorItem** ppItem);

protected:
	CFloorItem (CSIFRibbon* pRibbon, INT nFloor);
	~CFloorItem ();

public:
	inline USHORT GetFloor (VOID) { return m_nFloor; }

	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::Floor; }
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z);
	virtual VOID InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y);
	virtual HRESULT Serialize (ISequentialStream* pstmDef);
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb);

	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);
};

class CTextureItem : public CPaintItem
{
protected:
	TEXTURE* m_pTexture;

public:
	static HRESULT Create (CSIFRibbon* pRibbon, TEXTURE* pTexture, __deref_out CTextureItem** ppItem);

protected:
	CTextureItem (CSIFRibbon* pRibbon, TEXTURE* pTexture);
	~CTextureItem ();

public:
	inline const TEXTURE* GetTexture (VOID) { return m_pTexture; }

	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::Wall; }
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z);
	virtual VOID InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y);
	virtual HRESULT Serialize (ISequentialStream* pstmDef);
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb);

	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);

private:
	HRESULT LoadRibbonImage (PROPVARIANT* pValue);
};

class CActorItem : public CTextureItem
{
protected:
	ACTOR* m_pActor;

public:
	static HRESULT Create (CSIFRibbon* pRibbon, ACTOR* pActor, __deref_out CActorItem** ppItem);

protected:
	CActorItem (CSIFRibbon* pRibbon, ACTOR* pActor);
	~CActorItem ();

public:
	SHORT GetActorID (VOID) { return m_pActor->idActor; }
	INT GetDirection (VOID) { return m_pActor->nDirection; }

	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::Object; }
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z);
	virtual HRESULT Serialize (ISequentialStream* pstmDef);
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb);
};

class CElevatorSwitch : public CTextureItem
{
public:
	bool m_fSecret;

	static HRESULT Create (CSIFRibbon* pRibbon, TEXTURE* pTexture, bool fSecret, __deref_out CElevatorSwitch** ppItem);

protected:
	CElevatorSwitch (CSIFRibbon* pRibbon, TEXTURE* pTexture, bool fSecret);
	~CElevatorSwitch ();

	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::Elevator; }
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z);
	virtual HRESULT Serialize (ISequentialStream* pstmDef);
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb);
};

class CDoorObject : public CTextureItem
{
public:
	enum DoorType
	{
		Normal,
		SilverKey,
		GoldKey,
		Elevator,
		RubyKey
	};

	CBlockMap* m_pMap;
	TEXTURE* m_pAltTexture;
	DoorType m_eType;

	static HRESULT Create (CSIFRibbon* pRibbon, CBlockMap* pMap, TEXTURE* pTexture, TEXTURE* pAltTexture, DoorType eType, __deref_out CDoorObject** ppItem);

protected:
	CDoorObject (CSIFRibbon* pRibbon, CBlockMap* pMap, TEXTURE* pTexture, TEXTURE* pAltTexture, DoorType eType);
	~CDoorObject ();

public:
	inline const TEXTURE* GetAltTexture (VOID) { return m_pAltTexture; }

	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::Door; }
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z);
	virtual HRESULT Serialize (ISequentialStream* pstmDef);
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb);
};

class CWallCage : public CTextureItem
{
private:
	BYTE m_bType;

public:
	static HRESULT Create (CSIFRibbon* pRibbon, TEXTURE* pTexture, BYTE bType, __deref_out CWallCage** ppItem);

protected:
	CWallCage (CSIFRibbon* pRibbon, TEXTURE* pTexture, BYTE bType);
	~CWallCage ();

public:
	PCWSTR GetSecondaryTexture (VOID);

	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::WallCage; }
	virtual HRESULT Serialize (ISequentialStream* pstmDef);
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb);
};

class CStartItem : public CPaintItem
{
private:
	INT m_nDir;

public:
	static HRESULT Create (CSIFRibbon* pRibbon, INT nDir, __deref_out CStartItem** ppItem);

protected:
	CStartItem (CSIFRibbon* pRibbon, INT nDir);
	~CStartItem ();

public:
	INT GetDirection (VOID) { return m_nDir; }

	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::Start; }
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z);
	virtual VOID InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y);
	virtual HRESULT Serialize (ISequentialStream* pstmDef);
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb);

	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);
};

class CSecretDoor : public CPaintItem
{
private:
	BYTE m_fDir;

public:
	static HRESULT Create (CSIFRibbon* pRibbon, BYTE fDir, __deref_out CSecretDoor** ppItem);

protected:
	CSecretDoor (CSIFRibbon* pRibbon, BYTE fDir);
	~CSecretDoor ();

public:
	BYTE GetDirection (VOID) { return m_fDir; }

	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::SecretDoor; }
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z);
	virtual VOID InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y);
	virtual HRESULT Serialize (ISequentialStream* pstmDef);
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb);

	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);
};

class CEndSpot : public CPaintItem
{
public:
	static HRESULT Create (CSIFRibbon* pRibbon, __deref_out CEndSpot** ppItem);

protected:
	CEndSpot (CSIFRibbon* pRibbon);
	~CEndSpot ();

public:
	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::End; }
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z);
	virtual VOID InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y);
	virtual HRESULT Serialize (ISequentialStream* pstmDef);
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb);

	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);
};

class CSkyLight : public CPaintItem
{
public:
	static HRESULT Create (CSIFRibbon* pRibbon, __deref_out CSkyLight** ppItem);

protected:
	CSkyLight (CSIFRibbon* pRibbon);
	~CSkyLight ();

public:
	// CPaintItem
	virtual MapCell::Type GetType (VOID) { return MapCell::Sky; }
	virtual VOID Paint (IGrapher* pGraph, FLOAT x, FLOAT z);
	virtual VOID InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y);
	virtual HRESULT Serialize (ISequentialStream* pstmDef);
	virtual BOOL Deserialize (const BYTE* pcb, DWORD cb);

	// IUISimplePropertySet
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value);
};

interface IResolveItemPalette
{
	virtual HRESULT InitializePaintItems (CBlockMap* pMap) = 0;
	virtual HRESULT ResolveItemPalette (MapCell::Type eType, const BYTE* pcb, DWORD cb, __deref_out CPaintItem** ppItem) = 0;
};
