#include <math.h>
#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Spatial\Geometry.h"
#include "Library\DPI.h"
#include "BlockMap.h"
#include "PaintItems.h"

HRESULT SquareSwatchImage (CSIFRibbon* pRibbon, INT nSize, COLORREF cr, __out PROPVARIANT* pValue)
{
	HRESULT hr;
	HBITMAP hDIB;
	PBYTE pbDIB;
	IUIImage* pImage;

	BYTE bBlue = GetBValue(cr);
	BYTE bGreen = GetGValue(cr);
	BYTE bRed = GetRValue(cr);

	Check(sifCreateBlankDIB(NULL, nSize, nSize, 32, (PVOID*)&pbDIB, &hDIB));
	Check(pRibbon->CreateImageFromDIB(hDIB, &pImage));
	pValue->punkVal = pImage;
	pValue->vt = VT_UNKNOWN;
	hDIB = NULL;

	for(INT y = 0; y < nSize; y++)
	{
		for(INT x = 0; x < nSize; x++)
		{
			pbDIB[0] = bBlue;
			pbDIB[1] = bGreen;
			pbDIB[2] = bRed;
			pbDIB[3] = 255;	// Alpha
			pbDIB += 4;
		}
	}

Cleanup:
	SafeDeleteGdiObject(hDIB);
	return hr;
}

VOID DrawStartArrow (HDC hdc, INT x, INT y, INT nSize, INT nDir)
{
	HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
	HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 0));
	HPEN hArrowPen = CreatePen(PS_SOLID, 2, RGB(32, 32, 32));

	HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
	HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);

	Ellipse(hdc, x, y, x + nSize, y + nSize);

	DOUBLE angle = Geometry::TDegreesToRadians<DOUBLE>(nDir);  // Convert Doom angle to radians
	INT centerX = x + nSize / 2;
	INT centerY = y + nSize / 2;
	INT radius = nSize / 3;  // Set radius for arrow

	// Arrow points
	INT arrowX = centerX + static_cast<INT>(radius * cos(angle));
	INT arrowY = centerY - static_cast<INT>(radius * sin(angle));

	SelectObject(hdc, hArrowPen);
	MoveToEx(hdc, centerX, centerY, NULL);
	LineTo(hdc, arrowX, arrowY);

	SelectObject(hdc, oldPen);
	SelectObject(hdc, oldBrush);

	SafeDeleteGdiObject(hPen);
	SafeDeleteGdiObject(hBrush);
	SafeDeleteGdiObject(hArrowPen);
}

VOID DrawSecretDoor (HDC hdc, INT x, INT y, INT nSize, INT fDir)
{
	HPEN hPen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
	HBRUSH hBrush = CreateSolidBrush(RGB(64, 64, 64));

	HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
	HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);

	Rectangle(hdc, x, y, x + nSize, y + nSize);

	if(fDir)	// North/South
	{
		MoveToEx(hdc, x + nSize / 2, y + 5, NULL);
		LineTo(hdc, x + nSize / 2, y + nSize - 5);
	}
	else		// East/West
	{
		MoveToEx(hdc, x + 5, y + nSize / 2, NULL);
		LineTo(hdc, x + nSize - 5, y + nSize / 2);
	}

	SelectObject(hdc, oldPen);
	SelectObject(hdc, oldBrush);

	SafeDeleteGdiObject(hPen);
	SafeDeleteGdiObject(hBrush);
}

VOID DrawEndSpot (HDC hdc, INT x, INT y, INT nSize, INT nDir)
{
	HPEN hpnDef = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, RGB(255, 0, 0)));

	MoveToEx(hdc, x + 1, y + 1, NULL);
	LineTo(hdc, x + nSize - 1, y + nSize - 1);

	MoveToEx(hdc, x + 1, y + nSize - 1, NULL);
	LineTo(hdc, x + nSize - 1, y + 1);

	DeleteObject(SelectObject(hdc, hpnDef));
}

VOID DrawSkyLight (HDC hdc, INT x, INT y, INT nSize, INT nDir)
{
	HPEN hpnDef = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 1, RGB(220, 230, 255)));
	HBRUSH hbrDef = (HBRUSH)SelectObject(hdc, CreateSolidBrush(RGB(80, 100, 255)));

	Rectangle(hdc, x + 1, y + 1, x + nSize - 1, y + nSize - 1);

	DeleteObject(SelectObject(hdc, hbrDef));
	DeleteObject(SelectObject(hdc, hpnDef));
}

HRESULT DrawImageToDIB (CSIFRibbon* pRibbon, VOID (*pfnDrawImage)(HDC,INT,INT,INT,INT), INT nSize, INT nDir, __out PROPVARIANT* pValue)
{
	HRESULT hr;
	HDC hdc = NULL;
	HBITMAP hDIB, hOldDIB;
	PBYTE pbDIB;
	IUIImage* pImage;

	Check(sifCreateBlankDIB(NULL, nSize, nSize, 32, (PVOID*)&pbDIB, &hDIB));

	hdc = CreateCompatibleDC(NULL);
	CheckIfGetLastError(NULL == hdc);
	hOldDIB = (HBITMAP)SelectObject(hdc, hDIB);

	pfnDrawImage(hdc, 0, 0, nSize, nDir);

	SelectObject(hdc, hOldDIB);
	DeleteDC(hdc);

	for(INT y = 0; y < nSize; y++)
	{
		for(INT x = 0; x < nSize; x++)
		{
			if(pbDIB[0] || pbDIB[1] || pbDIB[2])
				pbDIB[3] = 255;	// Alpha
			pbDIB += 4;
		}
	}

	Check(pRibbon->CreateImageFromDIB(hDIB, &pImage));
	pValue->punkVal = pImage;
	pValue->vt = VT_UNKNOWN;
	hDIB = NULL;

Cleanup:
	SafeDeleteGdiObject(hDIB);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CPaintItem
///////////////////////////////////////////////////////////////////////////////

CPaintItem::CPaintItem (CSIFRibbon* pRibbon) :
	CBaseRibbonItem(pRibbon)
{
}

HRESULT CPaintItem::SerializeType (ISequentialStream* pstmDef)
{
	DWORD cb;
	MapCell::Type eType = GetType();
	BYTE bType = (BYTE)eType;
	return pstmDef->Write(&bType, sizeof(bType), &cb);
}

///////////////////////////////////////////////////////////////////////////////
// CVoidItem
///////////////////////////////////////////////////////////////////////////////

HRESULT CVoidItem::Create (CSIFRibbon* pRibbon, __deref_out CVoidItem** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CVoidItem(pRibbon);
	CheckAlloc(*ppItem);
	Check((*ppItem)->SetItemText(L"Void"));

Cleanup:
	return hr;
}

CVoidItem::CVoidItem (CSIFRibbon* pRibbon) :
	CPaintItem(pRibbon)
{
}

CVoidItem::~CVoidItem ()
{
}

// CPaintItem

VOID CVoidItem::Paint (IGrapher* pGraph, FLOAT x, FLOAT z)
{
}

VOID CVoidItem::InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y)
{
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CVoidItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = __super::GetValue(key, value);

	if(E_NOTIMPL == hr)
	{
		if(UI_PKEY_ItemImage == key || UI_PKEY_LargeImage == key)
			hr = SquareSwatchImage(m_pRibbon, (INT)DPI::Scale(32.0f), RGB(127, 127, 127), value);
		else if(UI_PKEY_SmallImage == key)
			hr = SquareSwatchImage(m_pRibbon, (INT)DPI::Scale(16.0f), RGB(127, 127, 127), value);
	}

	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CFloorItem
///////////////////////////////////////////////////////////////////////////////

HRESULT CFloorItem::Create (CSIFRibbon* pRibbon, USHORT nFloor, PCWSTR pcwzName, __deref_out CFloorItem** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CFloorItem(pRibbon, nFloor);
	CheckAlloc(*ppItem);
	Check((*ppItem)->SetItemText(pcwzName));

Cleanup:
	return hr;
}

CFloorItem::CFloorItem (CSIFRibbon* pRibbon, INT nFloor) :
	CPaintItem(pRibbon),
	m_nFloor(nFloor)
{
}

CFloorItem::~CFloorItem ()
{
}

// CPaintItem

VOID CFloorItem::Paint (IGrapher* pGraph, FLOAT x, FLOAT z)
{
	pGraph->FillRect(x, 0.0f, z, x + CELL_SCALE, 0.0f, z + CELL_SCALE, (HBRUSH)GetStockObject(BLACK_BRUSH));
}

VOID CFloorItem::InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y)
{
	RECT rc = { x, y, x + 64, y + 64 };
	FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
}

HRESULT CFloorItem::Serialize (ISequentialStream* pstmDef)
{
	HRESULT hr;
	DWORD cb;

	Check(SerializeType(pstmDef));
	Check(pstmDef->Write(&m_nFloor, sizeof(m_nFloor), &cb));

Cleanup:
	return hr;
}

BOOL CFloorItem::Deserialize (const BYTE* pcb, DWORD cb)
{
	USHORT nFloor;

	if(cb == sizeof(nFloor))
	{
		CopyMemory(&nFloor, pcb, sizeof(nFloor));
		return nFloor == m_nFloor;
	}
	return FALSE;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CFloorItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = __super::GetValue(key, value);

	if(E_NOTIMPL == hr)
	{
		if(UI_PKEY_ItemImage == key || UI_PKEY_LargeImage == key)
			hr = SquareSwatchImage(m_pRibbon, (INT)DPI::Scale(32.0f), RGB(32, 32, 32), value);
		else if(UI_PKEY_SmallImage == key)
			hr = SquareSwatchImage(m_pRibbon, (INT)DPI::Scale(16.0f), RGB(32, 32, 32), value);
	}

	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CTextureItem
///////////////////////////////////////////////////////////////////////////////

HRESULT CTextureItem::Create (CSIFRibbon* pRibbon, TEXTURE* pTexture, __deref_out CTextureItem** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CTextureItem(pRibbon, pTexture);
	CheckAlloc(*ppItem);

	hr = S_OK;

Cleanup:
	return hr;
}

CTextureItem::CTextureItem (CSIFRibbon* pRibbon, TEXTURE* pTexture) :
	CPaintItem(pRibbon),
	m_pTexture(pTexture)
{
}

CTextureItem::~CTextureItem ()
{
}

// CPaintItem

VOID CTextureItem::Paint (IGrapher* pGraph, FLOAT x, FLOAT z)
{
	HBRUSH hbr = CreateSolidBrush(m_pTexture->crAverage);
	pGraph->FillRect(x, 0.0f, z, x + CELL_SCALE, 0.0f, z + CELL_SCALE, hbr);
	DeleteObject(hbr);
}

VOID CTextureItem::InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y)
{
	sifDrawBits32ToDIB24Pitch(psifSurface->pbSurface, x, y, psifSurface->xSize, psifSurface->ySize, psifSurface->lPitch,
		m_pTexture->stmBits32.TGetReadPtr<BYTE>(), 64, 64, 64 * sizeof(DWORD));
}

HRESULT CTextureItem::Serialize (ISequentialStream* pstmDef)
{
	HRESULT hr;
	DWORD cb;
	INT cchName = TStrLenAssert(m_pTexture->pcwzName);

	Check(SerializeType(pstmDef));
	Check(pstmDef->Write(&cchName, sizeof(cchName), &cb));
	Check(pstmDef->Write(m_pTexture->pcwzName, sizeof(WCHAR) * cchName, &cb));

Cleanup:
	return hr;
}

BOOL CTextureItem::Deserialize (const BYTE* pcb, DWORD cb)
{
	INT cch;

	if(cb > sizeof(cch))
	{
		CopyMemory(&cch, pcb, sizeof(cch));
		return TStrLenAssert(m_pTexture->pcwzName) == cch && 0 == memcmp(m_pTexture->pcwzName, pcb + sizeof(cch), cch * sizeof(WCHAR));
	}
	return FALSE;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CTextureItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = __super::GetValue(key, value);

	if(E_NOTIMPL == hr)
	{
		if(UI_PKEY_ItemImage == key)
			hr = LoadRibbonImage(value);
		else if(UI_PKEY_LargeImage == key || UI_PKEY_SmallImage == key)
			hr = LoadRibbonImage(value);
	}

	return hr;
}

HRESULT CTextureItem::LoadRibbonImage (PROPVARIANT* pValue)
{
	HRESULT hr;
	PVOID pvBits;
	PBYTE pbResized = NULL;
	HBITMAP hDIB = NULL;
	HDC hdc = GetDC(NULL);
	TStackRef<IUIImage> srImage;

	Check(sifCreateBlankDIB(hdc, (LONG)DPI::Scale(32.0f), (LONG)DPI::ScaleY(32.0f), 32, &pvBits, &hDIB));
	Check(sifResizeImageBits32(m_pTexture->stmBits32.TGetReadPtr<BYTE>(),
		m_pTexture->xSize, m_pTexture->ySize,
		&pbResized, (INT)DPI::Scale(32.0f), (INT)DPI::ScaleY(32.0f)));
	sifCopyBits32ToDIB32(pbResized, (INT)DPI::Scale(32.0f), (INT)DPI::ScaleY(32.0f), reinterpret_cast<PBYTE>(pvBits));

	Check(m_pRibbon->CreateImageFromDIB(hDIB, &srImage));
	hDIB = NULL;	// Now owned by srImage

	pValue->punkVal = srImage.Detach();
	pValue->vt = VT_UNKNOWN;

Cleanup:
	sifDeleteMemoryPtr(pbResized);
	ReleaseDC(NULL, hdc);
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CActor
///////////////////////////////////////////////////////////////////////////////

HRESULT CActorItem::Create (CSIFRibbon* pRibbon, ACTOR* pActor, __deref_out CActorItem** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CActorItem(pRibbon, pActor);
	CheckAlloc(*ppItem);

	hr = S_OK;

Cleanup:
	return hr;
}

CActorItem::CActorItem (CSIFRibbon* pRibbon, ACTOR* pActor) :
	CTextureItem(pRibbon, pActor->pTexture),
	m_pActor(pActor)
{
}

CActorItem::~CActorItem ()
{
}

// CPaintItem

VOID CActorItem::Paint (IGrapher* pGraph, FLOAT x, FLOAT z)
{
	HBRUSH hbrDef = pGraph->SelectBrush(CreateSolidBrush(m_pTexture->crAverage));
	pGraph->Ellipse(x, 0.0f, z, x + CELL_SCALE, 0.0f, z + CELL_SCALE);
	DeleteObject(pGraph->SelectBrush(hbrDef));
}

HRESULT CActorItem::Serialize (ISequentialStream* pstmDef)
{
	HRESULT hr;
	DWORD cb;

	Check(SerializeType(pstmDef));
	Check(pstmDef->Write(&m_pActor->idActor, sizeof(m_pActor->idActor), &cb));
	Check(pstmDef->Write(&m_pActor->nDirection, sizeof(m_pActor->nDirection), &cb));

Cleanup:
	return hr;
}

BOOL CActorItem::Deserialize (const BYTE* pcb, DWORD cb)
{
	INT rgData[2];

	if(cb == sizeof(rgData))
	{
		CopyMemory(rgData, pcb, sizeof(rgData));
		return rgData[0] == m_pActor->idActor && rgData[1] == m_pActor->nDirection;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// CElevatorSwitch
///////////////////////////////////////////////////////////////////////////////

HRESULT CElevatorSwitch::Create (CSIFRibbon* pRibbon, TEXTURE* pTexture, bool fSecret, __deref_out CElevatorSwitch** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CElevatorSwitch(pRibbon, pTexture, fSecret);
	CheckAlloc(*ppItem);

	if(fSecret)
		Check((*ppItem)->SetItemText(L"Elevator (Secret)"));
	else
		Check((*ppItem)->SetItemText(L"Elevator (Normal)"));

Cleanup:
	return hr;
}

CElevatorSwitch::CElevatorSwitch (CSIFRibbon* pRibbon, TEXTURE* pTexture, bool fSecret) :
	CTextureItem(pRibbon, pTexture),
	m_fSecret(fSecret)
{
}

CElevatorSwitch::~CElevatorSwitch ()
{
}

// CPaintItem

VOID CElevatorSwitch::Paint (IGrapher* pGraph, FLOAT x, FLOAT z)
{
	HBRUSH hbr = CreateSolidBrush(RGB(255, 255, 0));
	pGraph->FillRect(x, 0.0f, z, x + CELL_SCALE, 0.0f, z + CELL_SCALE, hbr);
	DeleteObject(hbr);
}

HRESULT CElevatorSwitch::Serialize (ISequentialStream* pstmDef)
{
	HRESULT hr;
	DWORD cb;

	Check(SerializeType(pstmDef));
	Check(pstmDef->Write(&m_fSecret, sizeof(m_fSecret), &cb));

Cleanup:
	return hr;
}

BOOL CElevatorSwitch::Deserialize (const BYTE* pcb, DWORD cb)
{
	return 1 == cb && pcb[0] == (BYTE)m_fSecret;
}

///////////////////////////////////////////////////////////////////////////////
// CDoorObject
///////////////////////////////////////////////////////////////////////////////

HRESULT CDoorObject::Create (CSIFRibbon* pRibbon, CBlockMap* pMap, TEXTURE* pTexture, DoorType eType, __deref_out CDoorObject** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CDoorObject(pRibbon, pMap, pTexture, eType);
	CheckAlloc(*ppItem);

	switch(eType)
	{
	case Normal:
		Check((*ppItem)->SetItemText(L"Normal Door"));
		break;
	case SilverKey:
		Check((*ppItem)->SetItemText(L"Locked - Silver"));
		break;
	case GoldKey:
		Check((*ppItem)->SetItemText(L"Locked - Gold"));
		break;
	case Elevator:
		Check((*ppItem)->SetItemText(L"Elevator Door"));
		break;
	case RubyKey:
		Check((*ppItem)->SetItemText(L"Locked - Ruby"));
		break;
	default:
		Check(E_UNEXPECTED);
	}

Cleanup:
	return hr;
}

CDoorObject::CDoorObject (CSIFRibbon* pRibbon, CBlockMap* pMap, TEXTURE* pTexture, DoorType eType) :
	CTextureItem(pRibbon, pTexture),
	m_pMap(pMap),
	m_eType(eType)
{
}

CDoorObject::~CDoorObject ()
{
}

// CPaintItem

VOID CDoorObject::Paint (IGrapher* pGraph, FLOAT x, FLOAT z)
{
	INT xCell = (INT)(x / CELL_SCALE), zCell = (INT)(z / CELL_SCALE);
	HBRUSH hbr = NULL;

	switch(m_eType)
	{
	case Normal:
		hbr = CreateSolidBrush(RGB(0, 255, 255));
		break;
	case SilverKey:
		hbr = CreateSolidBrush(RGB(0, 128, 255));
		break;
	case GoldKey:
		hbr = CreateSolidBrush(RGB(255, 220, 80));
		break;
	case Elevator:
		hbr = CreateSolidBrush(RGB(100, 120, 164));
		break;
	case RubyKey:
		hbr = CreateSolidBrush(RGB(255, 0, 0));
		break;
	}

	xCell += MAP_WIDTH / 2;
	zCell += MAP_HEIGHT / 2;

	if(m_pMap->IsSolid(xCell - 1, zCell) && m_pMap->IsSolid(xCell + 1, zCell))
	{
		FLOAT rPadding = CELL_SCALE * 0.4f;
		pGraph->FillRect(x, 0.0f, z + rPadding, x + CELL_SCALE, 0.0f, z + CELL_SCALE - rPadding, hbr);
	}
	else if(m_pMap->IsSolid(xCell, zCell - 1) && m_pMap->IsSolid(xCell, zCell + 1))
	{
		FLOAT rPadding = CELL_SCALE * 0.4f;
		pGraph->FillRect(x + rPadding, 0.0f, z, x + CELL_SCALE - rPadding, 0.0f, z + CELL_SCALE, hbr);
	}
	else
	{
		pGraph->FillRect(x, 0.0f, z, x + CELL_SCALE, 0.0f, z + CELL_SCALE, hbr);

		HPEN hDefault = pGraph->SelectPen(CreatePen(PS_SOLID, 2, RGB(255, 0, 0)));
		pGraph->MoveTo(x, 0.0f, z);
		pGraph->LineTo(x + CELL_SCALE, 0.0f, z + CELL_SCALE);
		DeleteObject(pGraph->SelectPen(hDefault));
	}
	DeleteObject(hbr);
}

HRESULT CDoorObject::Serialize (ISequentialStream* pstmDef)
{
	HRESULT hr;
	DWORD cb;
	BYTE bDoorType = (BYTE)m_eType;

	Check(SerializeType(pstmDef));
	Check(pstmDef->Write(&bDoorType, sizeof(bDoorType), &cb));

Cleanup:
	return hr;
}

BOOL CDoorObject::Deserialize (const BYTE* pcb, DWORD cb)
{
	return 1 == cb && pcb[0] == (BYTE)m_eType;
}

///////////////////////////////////////////////////////////////////////////////
// CWallCage
///////////////////////////////////////////////////////////////////////////////

HRESULT CWallCage::Create (CSIFRibbon* pRibbon, TEXTURE* pTexture, BYTE bType, __deref_out CWallCage** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CWallCage(pRibbon, pTexture, bType);
	CheckAlloc(*ppItem);

	hr = S_OK;

Cleanup:
	return hr;
}

CWallCage::CWallCage (CSIFRibbon* pRibbon, TEXTURE* pTexture, BYTE bType) :
	CTextureItem(pRibbon, pTexture),
	m_bType(bType)
{
}

CWallCage::~CWallCage ()
{
}

PCWSTR CWallCage::GetSecondaryTexture (VOID)
{
	switch(m_bType)
	{
	case 0:
		return L"BLUESTN2";
	case 1:
		return L"BRWNSTN0";
	case 2:
		return L"WOODWAL0";
	}

	return NULL;
}

// CPaintItem

HRESULT CWallCage::Serialize (ISequentialStream* pstmDef)
{
	HRESULT hr;
	DWORD cb;

	Check(SerializeType(pstmDef));
	Check(pstmDef->Write(&m_bType, sizeof(m_bType), &cb));

Cleanup:
	return hr;
}

BOOL CWallCage::Deserialize (const BYTE* pcb, DWORD cb)
{
	return 1 == cb && pcb[0] == m_bType;
}

///////////////////////////////////////////////////////////////////////////////
// CStartItem
///////////////////////////////////////////////////////////////////////////////

HRESULT CStartItem::Create (CSIFRibbon* pRibbon, INT nDir, __deref_out CStartItem** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CStartItem(pRibbon, nDir);
	CheckAlloc(*ppItem);

	switch(nDir)
	{
	case 90:
		Check((*ppItem)->SetItemText(L"Start North"));
		break;
	case 0:
		Check((*ppItem)->SetItemText(L"Start East"));
		break;
	case 270:
		Check((*ppItem)->SetItemText(L"Start South"));
		break;
	case 180:
		Check((*ppItem)->SetItemText(L"Start West"));
		break;
	default:
		Check(E_UNEXPECTED);
	}

Cleanup:
	return hr;
}

CStartItem::CStartItem (CSIFRibbon* pRibbon, INT nDir) :
	CPaintItem(pRibbon),
	m_nDir(nDir)
{
}

CStartItem::~CStartItem ()
{
}

// CPaintItem

VOID CStartItem::Paint (IGrapher* pGraph, FLOAT x, FLOAT z)
{
	HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
	HBRUSH hBrush = CreateSolidBrush(RGB(255, 0, 0));
	HPEN hArrowPen = CreatePen(PS_SOLID, 2, RGB(32, 32, 32));

	HPEN oldPen = pGraph->SelectPen(hPen);
	HBRUSH oldBrush = pGraph->SelectBrush(hBrush);

	pGraph->Ellipse(x, 0.0f, z, x + CELL_SCALE, 0.0f, z + CELL_SCALE);

	DOUBLE angle = Geometry::TDegreesToRadians<DOUBLE>(m_nDir);  // Convert Doom angle to radians
    FLOAT centerX = x + CELL_SCALE / 2.0f;
    FLOAT centerZ = z + CELL_SCALE / 2.0f;
    FLOAT radius = CELL_SCALE / 3.0f;  // Set radius for arrow

    // Arrow points
    FLOAT arrowX = centerX + static_cast<FLOAT>(radius * cos(angle));
    FLOAT arrowZ = centerZ - static_cast<FLOAT>(radius * sin(angle));

	pGraph->SelectPen(hArrowPen);
	pGraph->MoveTo(centerX, 0.0f, centerZ);
	pGraph->LineTo(arrowX, 0.0f, arrowZ);

	pGraph->SelectPen(oldPen);
	pGraph->SelectBrush(oldBrush);

	SafeDeleteGdiObject(hPen);
	SafeDeleteGdiObject(hBrush);
	SafeDeleteGdiObject(hArrowPen);
}

VOID CStartItem::InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y)
{
	DrawStartArrow(hdc, x, y, 64, m_nDir);
}

HRESULT CStartItem::Serialize (ISequentialStream* pstmDef)
{
	HRESULT hr;
	DWORD cb;

	Check(SerializeType(pstmDef));
	Check(pstmDef->Write(&m_nDir, sizeof(m_nDir), &cb));

Cleanup:
	return hr;
}

BOOL CStartItem::Deserialize (const BYTE* pcb, DWORD cb)
{
	INT nDir;

	if(sizeof(nDir) == cb)
	{
		CopyMemory(&nDir, pcb, sizeof(nDir));
		return nDir == m_nDir;
	}
	return FALSE;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CStartItem::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = __super::GetValue(key, value);

	if(E_NOTIMPL == hr)
	{
		if(UI_PKEY_ItemImage == key || UI_PKEY_LargeImage == key)
			hr = DrawImageToDIB(m_pRibbon, DrawStartArrow, (INT)DPI::Scale(32.0f), m_nDir, value);
		else if(UI_PKEY_SmallImage == key)
			hr = DrawImageToDIB(m_pRibbon, DrawStartArrow, (INT)DPI::Scale(16.0f), m_nDir, value);
	}

	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CSecretDoor
///////////////////////////////////////////////////////////////////////////////

HRESULT CSecretDoor::Create (CSIFRibbon* pRibbon, BYTE fDir, __deref_out CSecretDoor** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CSecretDoor(pRibbon, fDir);
	CheckAlloc(*ppItem);

	switch(fDir)
	{
	case 0:
		Check((*ppItem)->SetItemText(L"Secret E/W"));
		break;
	case 1:
		Check((*ppItem)->SetItemText(L"Secret N/S"));
		break;
	default:
		Check(E_UNEXPECTED);
	}

Cleanup:
	return hr;
}

CSecretDoor::CSecretDoor (CSIFRibbon* pRibbon, BYTE fDir) :
	CPaintItem(pRibbon),
	m_fDir(fDir)
{
}

CSecretDoor::~CSecretDoor ()
{
}

// CPaintItem

VOID CSecretDoor::Paint (IGrapher* pGraph, FLOAT x, FLOAT z)
{
	HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));

	HPEN oldPen = pGraph->SelectPen(hPen);
	HBRUSH oldBrush = pGraph->SelectBrush((HBRUSH)GetStockObject(NULL_BRUSH));

	pGraph->Rectangle(x, 0.0f, z, x + CELL_SCALE, 0.0f, z + CELL_SCALE);

	if(m_fDir)	// North/South
	{
		pGraph->MoveTo(x + CELL_SCALE / 2.0f, 0.0f, z + 4.0f);
		pGraph->LineTo(x + CELL_SCALE / 2.0f, 0.0f, z + CELL_SCALE - 4.0f);
	}
	else		// East/West
	{
		pGraph->MoveTo(x + 4.0f, 0.0f, z + CELL_SCALE / 2.0f);
		pGraph->LineTo(x + CELL_SCALE - 4.0f, 0.0f, z + CELL_SCALE / 2.0f);
	}

	pGraph->SelectPen(oldPen);
	pGraph->SelectBrush(oldBrush);

	SafeDeleteGdiObject(hPen);
}

VOID CSecretDoor::InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y)
{
	DrawSecretDoor(hdc, x, y, 64, m_fDir);
}

HRESULT CSecretDoor::Serialize (ISequentialStream* pstmDef)
{
	HRESULT hr;
	DWORD cb;

	Check(SerializeType(pstmDef));
	Check(pstmDef->Write(&m_fDir, sizeof(m_fDir), &cb));

Cleanup:
	return hr;
}

BOOL CSecretDoor::Deserialize (const BYTE* pcb, DWORD cb)
{
	BYTE fDir;

	if(sizeof(fDir) == cb)
	{
		CopyMemory(&fDir, pcb, sizeof(fDir));
		return fDir == m_fDir;
	}
	return FALSE;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CSecretDoor::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = __super::GetValue(key, value);

	if(E_NOTIMPL == hr)
	{
		if(UI_PKEY_ItemImage == key || UI_PKEY_LargeImage == key)
			hr = DrawImageToDIB(m_pRibbon, DrawSecretDoor, (INT)DPI::Scale(32.0f), m_fDir, value);
		else if(UI_PKEY_SmallImage == key)
			hr = DrawImageToDIB(m_pRibbon, DrawSecretDoor, (INT)DPI::Scale(16.0f), m_fDir, value);
	}

	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CEndSpot
///////////////////////////////////////////////////////////////////////////////

HRESULT CEndSpot::Create (CSIFRibbon* pRibbon, __deref_out CEndSpot** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CEndSpot(pRibbon);
	CheckAlloc(*ppItem);
	Check((*ppItem)->SetItemText(L"End"));

Cleanup:
	return hr;
}

CEndSpot::CEndSpot (CSIFRibbon* pRibbon) :
	CPaintItem(pRibbon)
{
}

CEndSpot::~CEndSpot ()
{
}

// CPaintItem

VOID CEndSpot::Paint (IGrapher* pGraph, FLOAT x, FLOAT z)
{
	HPEN hpnDef = pGraph->SelectPen(CreatePen(PS_SOLID, 2, RGB(255, 0, 0)));

	pGraph->MoveTo(x + 1.0f, 0.0f, z + 1.0f);
	pGraph->LineTo(x + CELL_SCALE - 1.0f, 0.0f, z + CELL_SCALE - 1.0f);

	pGraph->MoveTo(x + 1.0f, 0.0f, z + CELL_SCALE - 1.0f);
	pGraph->LineTo(x + CELL_SCALE - 1.0f, 0.0f, z + 1.0f);

	DeleteObject(pGraph->SelectPen(hpnDef));
}

VOID CEndSpot::InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y)
{
	DrawEndSpot(hdc, x, y, 64, 0);
}

HRESULT CEndSpot::Serialize (ISequentialStream* pstmDef)
{
	return CPaintItem::SerializeType(pstmDef);
}

BOOL CEndSpot::Deserialize (const BYTE* pcb, DWORD cb)
{
	return TRUE;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CEndSpot::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = __super::GetValue(key, value);

	if(E_NOTIMPL == hr)
	{
		if(UI_PKEY_ItemImage == key || UI_PKEY_LargeImage == key)
			hr = DrawImageToDIB(m_pRibbon, DrawEndSpot, (INT)DPI::Scale(32.0f), 0, value);
		else if(UI_PKEY_SmallImage == key)
			hr = DrawImageToDIB(m_pRibbon, DrawEndSpot, (INT)DPI::Scale(16.0f), 0, value);
	}

	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CSkyLight
///////////////////////////////////////////////////////////////////////////////

HRESULT CSkyLight::Create (CSIFRibbon* pRibbon, __deref_out CSkyLight** ppItem)
{
	HRESULT hr;

	CheckIf(NULL == pRibbon, E_INVALIDARG);

	*ppItem = __new CSkyLight(pRibbon);
	CheckAlloc(*ppItem);
	Check((*ppItem)->SetItemText(L"Sky Light"));

Cleanup:
	return hr;
}

CSkyLight::CSkyLight (CSIFRibbon* pRibbon) :
	CPaintItem(pRibbon)
{
}

CSkyLight::~CSkyLight ()
{
}

// CPaintItem

VOID CSkyLight::Paint (IGrapher* pGraph, FLOAT x, FLOAT z)
{
	HPEN hpnDef = pGraph->SelectPen(CreatePen(PS_SOLID, 1, RGB(220, 230, 255)));
	HBRUSH hbrDef = pGraph->SelectBrush(CreateSolidBrush(RGB(80, 100, 255)));

	pGraph->Rectangle(x + 1, 0.0f, z + 1, x + CELL_SCALE - 1.0f, 0.0f, z + CELL_SCALE - 1.0f);

	DeleteObject(pGraph->SelectBrush(hbrDef));
	DeleteObject(pGraph->SelectPen(hpnDef));
}

VOID CSkyLight::InfoPaint (SIF_SURFACE* psifSurface, HDC hdc, INT x, INT y)
{
	DrawSkyLight(hdc, x, y, 64, 0);
}

HRESULT CSkyLight::Serialize (ISequentialStream* pstmDef)
{
	return CPaintItem::SerializeType(pstmDef);
}

BOOL CSkyLight::Deserialize (const BYTE* pcb, DWORD cb)
{
	return TRUE;
}

// IUISimplePropertySet

HRESULT STDMETHODCALLTYPE CSkyLight::GetValue (REFPROPERTYKEY key, PROPVARIANT* value)
{
	HRESULT hr = __super::GetValue(key, value);

	if(E_NOTIMPL == hr)
	{
		if(UI_PKEY_ItemImage == key || UI_PKEY_LargeImage == key)
			hr = DrawImageToDIB(m_pRibbon, DrawSkyLight, (INT)DPI::Scale(32.0f), 0, value);
		else if(UI_PKEY_SmallImage == key)
			hr = DrawImageToDIB(m_pRibbon, DrawSkyLight, (INT)DPI::Scale(16.0f), 0, value);
	}

	return hr;
}
