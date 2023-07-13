#pragma once

#include <gdiplus.h>
#include "Library\Core\Array.h"
#include "Library\Util\RString.h"
#include "Library\Sorting.h"

class CTabs;

class CTab
{
public:
	RSTRING m_rstrLabel;
	PVOID m_pvData;
	Gdiplus::PointF m_pt[5];
	FLOAT m_xCalculated;
	bool m_fActive;
	bool m_fHighlight;

public:
	CTab (RSTRING rstrLabel, PVOID pvData) :
		m_pvData(pvData),
		m_fActive(false),
		m_fHighlight(false)
	{
		RStrSet(m_rstrLabel, rstrLabel);
	}

	~CTab ()
	{
		RStrRelease(m_rstrLabel);
	}

	VOID SetActive (bool fActive, FLOAT rOffset)
	{
		if(m_fActive != fActive)
		{
			m_fActive = fActive;

			if(fActive)
			{
				m_pt[0].X -= rOffset;
				m_pt[1].Y += rOffset;
				m_pt[2].Y += rOffset;
				m_pt[3].Y += rOffset;
			}
			else
			{
				m_pt[0].X += rOffset;
				m_pt[1].Y -= rOffset;
				m_pt[2].Y -= rOffset;
				m_pt[3].Y -= rOffset;
			}
		}
	}

	VOID Draw (CTabs* pTabs, Gdiplus::Graphics& graphics, Gdiplus::Font* pFont, bool fHover);
	BOOL IsPointOver (CTabs* pTabs, LONG x, LONG y);
};

class CTabs
{
	friend CTab;

private:
	HICON m_hCloseIcon;
	HICON m_hDropDown;

	Gdiplus::Bitmap* m_pBuffer;

	Gdiplus::Font* m_pNormal, *m_pBold;
	FLOAT m_yFontSize;
	FLOAT m_yNormalTab;
	FLOAT m_yTextOffset;
	FLOAT m_yActiveTab;
	FLOAT m_xLabelOffset;

	SIZE m_szTabs, m_szDropDown, m_szCloseButton;
	LONG m_xRightButtons, m_xButtonPadding;

	TArray<CTab*> m_aTabs;
	sysint m_cVisible;
	sysint m_idxHover;

	BOOL m_fHoverClose, m_fHoverDropDown;

public:
	CTabs (HICON hCloseIcon, HICON hDropDown);
	~CTabs ();

	HRESULT LoadMetrics (HWND hwnd);
	VOID Resize (LONG nWidth, LONG nHeight);
	const SIZE* GetSize (VOID) { return &m_szTabs; }
	VOID GetTabsRect (LONG xOffset, LONG yOffset, __out RECT& rc);

	sysint Tabs (VOID) { return m_aTabs.Length(); }
	sysint FindTab (PVOID pvData);
	HRESULT AddTab (HDC hdc, RSTRING rstrLabel, PVOID pvData);
	HRESULT SetActive (sysint idxTab);
	sysint GetActiveTab (VOID);
	HRESULT RemoveTab (sysint idxTab);
	PVOID GetTabData (sysint idxTab);
	HRESULT HighlightTab (sysint idxTab, bool fHighlight);
	bool GetActiveTabHighlight (VOID);

	VOID Draw (HDC hdc, LONG x, LONG y);
	BOOL IsDropDown (LONG x, LONG y);
	BOOL IsCloseButton (LONG x, LONG y);
	PVOID FindTabFromPoint (LONG x, LONG y);
	BOOL MouseHover (HWND hwnd, LONG x, LONG y);
	VOID ClearHoverFlags (VOID);

	BOOL PopupTabs (HWND hwnd, LONG x, LONG y, __out sysint* pidxTab);

	template<typename T>
	T* TFindTabFromPoint (LONG x, LONG y)
	{
		PVOID pvData = FindTabFromPoint(x, y);
		if(pvData)
			return reinterpret_cast<T*>(pvData);
		return NULL;
	}

	template<typename T>
	T* TGetTabData (sysint idxTab)
	{
		PVOID pvData = GetTabData(idxTab);
		if(pvData)
			return reinterpret_cast<T*>(pvData);
		return NULL;
	}

private:
	VOID DrawButton (Gdiplus::Graphics& graphics, LONG xLeft, LONG yCenter, HICON hButton, const SIZE& szButton, BOOL fHover);
	VOID CalculateVisibleTabs (VOID);

	static HRESULT CreateFont (HDC hdc, PCWSTR pcwzFamily, INT nPointSize, INT nWeight, __deref_out Gdiplus::Font** ppFont);
	static BYTE GetPreferredFontQuality (VOID);
	static BOOL GetIconDimensions (HICON hIcon, LONG& width, LONG& height);

	static INT WINAPI PopupMenuSorter (CTab** pplhItem, CTab** pprhItem, PVOID pParam);
};
