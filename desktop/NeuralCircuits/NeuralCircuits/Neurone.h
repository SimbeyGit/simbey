#pragma once

#include <docobj.h>
#include "ConnList.h"

interface INeurone;
interface INeuralNet;

#define	DEFAULT_RADIUS			6
#define	AXON_RADIUS				3

#define	TRAIN_RANDOMIZE_WEIGHTS	1

class CAccessibleNeurone;

class CNeurone : public INeurone, public INetDocObject, public IOleCommandTarget
{
private:
	ULONG m_cRef;

protected:
	CConnList m_Connections;

	INT m_x;
	INT m_y;

	DWORD m_dwState;
	INT m_iRadius;
	BYTE m_iAxonPosition;

	FLOAT m_fThreshold;
	FLOAT m_fAccumulator;
	FLOAT m_fFinalValue;

	CAccessibleNeurone* m_lpAccessible;

public:
	CNeurone ();
	virtual ~CNeurone ();

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

	// INeurone
	virtual VOID ResetNeurone (VOID);
	virtual INT CountConnections (ULONG iSourcePin);
	virtual VOID EnumConnections (ULONG iSourcePin, ENUMCONNLIST lpfnCallback, LPVOID lpParam);
	virtual BOOL ClearConnection (ULONG iSourcePin, INT index);
	virtual BOOL SetConnectionWeight (ULONG iSourcePin, INT index, FLOAT fWeight);
	virtual BOOL GetCurrentValue (FLOAT& fValue);
	virtual VOID SetThreshold (FLOAT fThreshold);
	virtual FLOAT GetThreshold (VOID);
	virtual VOID RunTrainer (DWORD dwTrainer, FLOAT fInput, ULONG iInputPin, FLOAT fOutput, ULONG iOutputPin);

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

	// IOleCommandTarget
	virtual HRESULT WINAPI QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText);
	virtual HRESULT WINAPI Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut);

protected:
	virtual BOOL GetAxonPosition (INT& x, INT& y);
	virtual HRESULT CmdFire (VOID);
};