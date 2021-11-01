#pragma once

#define	NEURAL_FRAME_SIZE					150

class CNeuralFrame : public INeuralFrame, public IOleCommandTarget
{
private:
	volatile ULONG m_cRef;

protected:
	HWND m_hwndParent;

	RECT m_rcPos;

	LPSTR m_lpszLabel;
	INT m_cchLabel;

	COLORREF m_crBackground;
	BYTE m_bAlpha;
	BOOL m_fSelected;
	INT m_cHighlight;

	INetDocObject** m_lpList;
	INT m_cList;
	INT m_cMaxList;

public:
	CNeuralFrame ();
	virtual ~CNeuralFrame ();

	HRESULT SetFrameSize (INT xSize, INT ySize);

	// IUnknown
	HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* lplpvObject);
	ULONG WINAPI AddRef (VOID);
	ULONG WINAPI Release (VOID);

	// INetCycleProcessor
	virtual VOID SendPulses (VOID);
	virtual VOID CheckThresholds (VOID);

	// INetObject
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass);
	virtual VOID GetPosition (INT& x, INT& y);
	virtual VOID ReceiveValue (FLOAT fValue, ULONG iPin);
	virtual ULONG GetInputPin (INT x, INT y);
	virtual BOOL GetInputPinPosition (ULONG iPin, INT& x, INT& y);
	virtual HRESULT Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData);
	virtual HRESULT Save (INeuralNet* lpNet, ISequentialStream* lpStream);

	// INetDocObject
	virtual INT GetZOrder (VOID);
	virtual VOID DrawBackground (IGrapher* lpGraph);
	virtual VOID DrawForeground (IGrapher* lpGraph);
	virtual VOID SelectObject (BOOL fSelect);
	virtual VOID MoveObject (INT xDelta, INT yDelta);
	virtual VOID ResizeObject (INT nHitTest, INT xResizePos, INT yResizePos, INT xDelta, INT yDelta);
	virtual INT HitTest (INT x, INT y);
	virtual BOOL GetDragSourcePin (INT x, INT y, ULONG& iPin);
	virtual BOOL GetDragSourcePoint (ULONG iPin, INT& x, INT& y);
	virtual BOOL HighlightPin (ULONG iPin, BOOL fHighlight);
	virtual BOOL HighlightConn (ULONG iPin, INT index);
	virtual VOID NotifyRemovalOf (INetDocObject* lpObject);
	virtual HRESULT ConnectTo (ULONG iSourcePin, INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight);
	virtual HRESULT ContextMenu (IBaseContainer* lpContainer, INT x, INT y);
	virtual BOOL Click (INT x, INT y);
	virtual BOOL PressChar (CHAR ch);
	virtual LONG GetAccState (VOID);
	virtual HRESULT GetAccObject (IAccessibleNetDoc* lpParent, IAccessible** lplpAccessible);
	virtual HRESULT UnloadAccessibility (VOID);
	virtual HRESULT GetGraphInputCaptureObject (INeuralDocument* lpDocument, IGraphInputCapture** lplpCapture);

	// INeuralFrame
	virtual HRESULT AddObject (INetDocObject* lpObject);
	virtual HRESULT RemoveObject (INetDocObject* lpObject);
	virtual BOOL ContainsObject (INetDocObject* lpObject);
	virtual VOID HighlightFrame (BOOL fHighlight);
	virtual HRESULT SetFrameLabel (LPCSTR lpcszLabel);
	virtual LPCSTR GetFrameLabel (VOID);

	// IOleCommandTarget
	virtual HRESULT WINAPI QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText);
	virtual HRESULT WINAPI Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut);

protected:
	BOOL HitTestResizeDir (INT x, INT y, INT& nHitTest, BOOL& fInClient);

	VOID DoRenameDlg (VOID);
	VOID DoSetColorDlg (VOID);
	VOID DoRandomizeWeights (VOID);
};