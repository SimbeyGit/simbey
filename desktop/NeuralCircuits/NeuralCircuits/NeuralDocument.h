#pragma once

#include <docobj.h>
#include "BaseContainer.h"
#include "GraphCtrlAdapter.h"
#include "Neurone.h"

interface INeuralFactory;
interface INeuralNet;
interface INetDocObject;
interface IGraphInputCapture;

class CNeuralLinks;

#define	MOUSE_NONE					0
#define	MOUSE_SELECT				1
#define	MOUSE_DRAG					2
#define	MOUSE_CONNECT				3
#define	MOUSE_RESIZE_V				4
#define	MOUSE_RESIZE_H				5
#define	MOUSE_RESIZE_C1				6
#define	MOUSE_RESIZE_C2				7
#define	MOUSE_PRE_DRAW_SELECTION	8
#define	MOUSE_DRAW_SELECTION		9

#define	SELECT_PRESELECTION			1
#define	SELECT_FROM_RANGE_ACTIVE	2
#define	SELECT_FROM_RANGE_PASSIVE	4

typedef struct tagSELECT_LIST
{
	DWORD dwFlags;	// Used with drawing a selection box
	INetDocObject* lpObject;
	struct tagSELECT_LIST* Next;
} SELECT_LIST, *LPSELECT_LIST;

typedef struct tagNETNAVLIST
{
	INeuralNet* lpNet;
	LPSELECT_LIST lpSelection;
	struct tagNETNAVLIST* Next;
} NETNAVLIST, *LPNETNAVLIST;

class CAccessibleNetDoc;
class CRecentChipsMenu;

class CNeuralDocument :
	public IOleCommandTarget,
	public IWindowless,
	public IBaseContainer,
	public IGraphClient,
	public INeuralDocument
{
private:
	ULONG m_cRef;

protected:
	IOleCommandTarget* m_lpParent;
	IBaseContainer* m_lpContainer;

	CGraphCtrlAdapter* m_pGraphAdapter;
	CGraphCtrl* m_lpGraph;

	INeuralFactory* m_pFactory;
	CNeuralLinks* m_lpLinks;

	LPSTR m_lpszFile;
	INeuralNet* m_lpNet;
	INeuralNet* m_lpSelectedNet;
	BOOL m_fChangesMade;
	BOOL m_fDrawLabels;
	BOOL m_fRunning;

	LPSELECT_LIST m_lpSelection;
	INetDocObject* m_lpToggle;
	INT m_xDown, m_yDown;
	INT m_xDragPos, m_yDragPos;
	INT m_iDragLevel;
	ULONG m_iPinSource;
	ULONG m_iPinHovered;
	BOOL m_fInsertSelection;
	BOOL m_fInteractToggle;

	UINT m_uTimerSpeed;
	INT m_nTimerIndex;
	LPNETNAVLIST m_lpNetNav;

	CAccessibleNetDoc* m_lpAccessible;
	IOleCommandTarget* m_lpContextMenuTarget;
	CRecentChipsMenu* m_lpRecentChips;

	DWORD m_cmdDoubleClick;

	IGraphInputCapture* m_lpCapture;

public:
	CNeuralDocument (IOleCommandTarget* lpParent);
	virtual ~CNeuralDocument ();

	HRESULT Initialize (VOID);

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// IOleCommandTarget
	HRESULT WINAPI QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText);
	HRESULT WINAPI Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut);

	// IWindowless
	virtual VOID WINAPI AttachContainer (IBaseContainer* lpContainer);
	virtual VOID WINAPI Paint (HDC hdc);
	virtual VOID WINAPI Move (LPRECT lpPosition);
	virtual VOID WINAPI SizeObject (INT nWidth, INT nHeight);
	virtual BOOL WINAPI OnMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult);
	virtual HRESULT WINAPI GetAccObject (IAccessible** lplpAccessible);

	// IBaseContainer
	virtual HRESULT WINAPI GetHwnd (HWND* lphwnd);
	virtual HRESULT WINAPI GetCapture (IWindowless** lplpObject);
	virtual HRESULT WINAPI SetCapture (IWindowless* lpObject, BOOL fCapture);
	virtual HRESULT WINAPI GetFocus (IWindowless** lplpObject);
	virtual HRESULT WINAPI SetFocus (IWindowless* lpObject);
	virtual HRESULT WINAPI SetTimer (IWindowless* lpObject, UINT uElapse, INT* lpnTimer);
	virtual HRESULT WINAPI KillTimer (INT nTimer);
	virtual HRESULT WINAPI GetDC (IWindowless* lpObject, HDC* lphdc);
	virtual HRESULT WINAPI ReleaseDC (IWindowless* lpObject, HDC hdc);
	virtual HRESULT WINAPI InvalidateContainer (VOID);
	virtual HRESULT WINAPI OnWindowlessMessage (IWindowless* lpObject, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* lplResult);
	virtual HRESULT WINAPI ScreenToWindowless (IWindowless* lpObject, LPPOINT lpPoint);
	virtual HRESULT WINAPI TrackPopupMenu (HMENU hMenu, INT x, INT y, IOleCommandTarget* lpTarget);

	// IGraphClient
	virtual VOID onGraphPaint (IGrapher* lpGraph);
	virtual VOID onGraphMouseMove (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphLBtnDown (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphLBtnUp (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual BOOL onGraphRBtnDown (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphRBtnUp (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphLBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphRBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphViewChanged (BOOL bZoomChanged);
	virtual BOOL onGraphKeyDown (WPARAM iKey);
	virtual BOOL onGraphKeyUp (WPARAM iKey);
	virtual BOOL onGraphChar (WPARAM iKey);
	virtual BOOL onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y);
	virtual HRESULT onGraphGetAcc (IAccessible** lplpAccessible);

	// INeuralDocument
	virtual VOID SetChangesMade (VOID);
	virtual BOOL IsSavedOnDisk (VOID);
	virtual BOOL ChangesMade (VOID);
	virtual VOID ClearSelection (VOID);
	virtual HRESULT AddToSelection (INetDocObject* lpObject);
	virtual BOOL RemoveSelection (INetDocObject* lpObject);
	virtual BOOL InSelection (INetDocObject* lpObject);
	virtual HRESULT SaveToFile (LPCSTR lpcszFile);
	virtual BOOL SelectionContainsIO (VOID);
	virtual BOOL FindOneSelectedChip (INeuralChip** lplpChip);
	virtual HRESULT DeleteSelection (VOID);
	virtual HRESULT InsertObject (INetDocObject* lpObject);
	virtual VOID SetGraphCapture (IGraphInputCapture* lpCapture);

	VOID OnModifyMenu (HMENU hMenu);

	VOID ClearSelection (LPSELECT_LIST lpSelection);
	VOID ReducePinNumbers (EIO_TYPE eType, ULONG iReducePin);

	VOID ZoomIntoNet (INeuralNet* lpNet, INT x, INT y, INT cFrames);
	VOID ZoomOutOfNet (INeuralNet* lpNet, INT cFrames);

	HRESULT GetSelection (VARIANT** lplpvDispatchList, ULONG* lpcList);
	HRESULT GetFileName (LPSTR* lplpszFileName);

protected:
	VOID PerformNetCycle (VOID);

	HRESULT LoadChipFromFile (PCSTR pcszFile, INT cchFile, __deref_out INetDocObject** ppChip);
	HRESULT CreateUsingFactory (DWORD nCmdID, INetDocObject** ppObject);
	HRESULT InsertObject (DWORD nCmdID);

	HRESULT SetProperty (LPCSTR lpcszName, LPVOID lpValue, DWORD cbValue);
	HRESULT GetProperty (LPCSTR lpcszName, LPVOID lpValue, DWORD cbMaxValue, DWORD* lpcbValue = NULL);

	VOID SetFrameHighlighting (BOOL fHighlight);
	VOID CheckObjectFrame (INetDocObject* lpObject, BOOL fHover, BOOL fForceHighlight);
	VOID RegroupFrame (INetDocObject* lpObject);

	HRESULT AddToSelectionInternal (INetDocObject* lpObject, DWORD dwFlags);
	VOID CullSelectionRange (VOID);
	VOID GetSelectionRect (IGrapher* lpGraph, RECT* lprc);

	VOID SetResizeCursor (INT nHitTest);

	HRESULT SendCommandToSelection (DWORD nCmd, DWORD nCmdOpt);
};