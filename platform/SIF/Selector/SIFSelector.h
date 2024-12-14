#pragma once

#include "Library\Core\Array.h"
#include "Library\Core\BaseUnknown.h"
#include "Library\Util\RString.h"
#include "Library\Window\AdapterWindow.h"
#include "Published\SIF.h"

struct ITEM_SIZING
{
	LONG xItemSize;
	LONG yItemSize;
	LONG yTextHeight;
};

struct SIF_ITEM
{
	ISimbeyInterchangeFileLayer* pLayer;
	RSTRING rstrLabel;
	HBITMAP hbmCache;
	LONG xPos;
};

class CScrollableObject
{
public:
	LONG m_yPosition;
	LONG m_ySize;

public:
	CScrollableObject ();

	BOOL IsVisible (LONG nHeight, LONG yScroll, __out LONG* pyStart);
	BOOL IsPositionWithin (LONG yPosition);
};

class CSIFTitle : public CScrollableObject
{
public:
	RSTRING m_rstrTitle;

public:
	CSIFTitle (RSTRING rstrTitle);
	~CSIFTitle ();

	VOID Layout (LONG nWidth, LONG yOffset, const ITEM_SIZING& sizing, __out LONG* pyFinalOffset);
	VOID Paint (HDC hdc, LONG nWidth, LONG nHeight, LONG yScroll, LOGFONT& logFont);
};

class CSIFRow : public CScrollableObject
{
public:
	ITEM_SIZING m_sizing;
	LONG m_cItems;
	SIF_ITEM* m_pItems;

public:
	CSIFRow ();
	~CSIFRow ();

	HRESULT Layout (LONG nWidth, LONG yOffset, const ITEM_SIZING& sizing, ISimbeyInterchangeFile* pSIF, DWORD nOffset, __out LONG* pyFinalOffset);
	VOID Paint (HDC hdc, LONG nHeight, LONG yScroll, SIF_ITEM* pSelected);
	BOOL SelectItem (INT x, INT y, __deref_out SIF_ITEM** ppSelected);
};

class CSIFGroup : public CScrollableObject
{
public:
	ISimbeyInterchangeFile* m_pSIF;
	TArray<CSIFRow*> m_aRows;
	CSIFTitle m_title;

	SIF_ITEM* m_pSelected;

public:
	CSIFGroup (RSTRING rstrTitle, ISimbeyInterchangeFile* pSIF);
	~CSIFGroup ();

	HRESULT Layout (LONG nWidth, LONG yOffset, const ITEM_SIZING& sizing, __out LONG* pyFinalOffset);
	VOID Paint (HDC hdc, LONG nWidth, LONG nHeight, LONG yScroll, const ITEM_SIZING& sizing);
	BOOL SelectItem (INT x, INT y);
	BOOL SelectItemByIndex (sysint idxItem);
	BOOL SelectItemByID (DWORD idLayer);
	BOOL NavigateSelection (WPARAM wKey);
	BOOL GetSelected (__out sysint* pidxRow, __out sysint* pidxItem);
};

class CSIFSelector :
	public CBaseUnknown,
	public IAdapterWindowCallback
{
private:
	IBaseWindow* m_pWindow;
	TArray<CSIFGroup*> m_aGroups;
	LONG m_xClientSize, m_yClientSize;
	LONG m_yTotalSize;

	ITEM_SIZING m_sizing;

	// Cached selection
	RSTRING m_rstrSelection;
	DWORD m_idSelection;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IAdapterWindowCallback)
	END_UNK_MAP

public:
	CSIFSelector ();
	~CSIFSelector ();

	HRESULT ResizeItems (LONG xItemSize, LONG yItemSize, LONG yTextHeight);
	HRESULT AddSIF (RSTRING rstrTitle, ISimbeyInterchangeFile* pSIF);
	BOOL HasSelection (VOID);
	VOID DeferSelection (RSTRING rstrTitle, DWORD idLayer);
	VOID LoadSelection (VOID);
	HRESULT GetSelected (__out RSTRING* prstrTitle, __out DWORD* pidSelection);

	// IAdapterWindowCallback
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
	virtual VOID OnAttachingAdapter (IBaseWindow* pAdapter);
	virtual VOID OnDetachingAdapter (IBaseWindow* pAdapter);

private:
	HRESULT SetSelection (RSTRING rstrTitle, DWORD idLayer);
	HRESULT GetScrollPos (__out INT* pnPos, BOOL fTrackPos);
	HRESULT UpdateScrollRange (VOID);
	HRESULT Relayout (VOID);

	CSIFGroup* FindSelectedGroup (__out_opt sysint* pidxGroup = NULL);

	HRESULT Click (INT xClient, INT yClient);
	HRESULT NavigateSelection (WPARAM wParam);
};
