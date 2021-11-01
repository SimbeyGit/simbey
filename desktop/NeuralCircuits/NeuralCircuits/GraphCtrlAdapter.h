#pragma once

#include "Library\GraphCtrl.h"
#include "Windowless.h"

class CGraphCtrlAdapter :
	public IWindowless,
	public IGraphContainer
{
private:
	ULONG m_cRef;

protected:
	IBaseContainer* m_pContainer;
	CGraphCtrl* m_pCtrl;

public:
	CGraphCtrlAdapter ();
	~CGraphCtrlAdapter ();

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// IWindowless
	virtual VOID WINAPI AttachContainer (IBaseContainer* lpContainer);
	virtual VOID WINAPI Paint (HDC hdc);
	virtual VOID WINAPI Move (LPRECT lpPosition);
	virtual VOID WINAPI SizeObject (INT nWidth, INT nHeight);
	virtual BOOL WINAPI OnMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult);
	virtual HRESULT WINAPI GetAccObject (IAccessible** lplpAccessible);

	// IGraphContainer
	virtual VOID WINAPI OnScaleChanged (FLOAT /*fScale*/) {}
	virtual VOID WINAPI OnGridSpacingChanged (INT /*iSpacing*/) {}
	virtual HRESULT WINAPI SetFocus (__in IGrapher* pGraphCtrl);
	virtual HRESULT WINAPI ScreenToWindowless (__in IGrapher* pGraphCtrl, __inout PPOINT pPoint);
	virtual HRESULT WINAPI InvalidateContainer (__in IGrapher* pGraphCtrl);
	virtual VOID WINAPI DrawDib24 (HDC hdcDest, LONG x, LONG y, HDC hdcSrc, const BYTE* pcDIB24, LONG xSize, LONG ySize, LONG lPitch);
	virtual BOOL WINAPI CaptureMouse (__in IGrapher* pGraphCtrl, BOOL fCapture);

	HRESULT Initialize (VOID);
	CGraphCtrl* GetCtrl (VOID);
};