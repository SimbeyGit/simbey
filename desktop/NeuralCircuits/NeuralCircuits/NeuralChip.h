#pragma once

#include "Neurone.h"
#include "NeuralNet.h"

typedef struct
{
	INeurone* lpNeurone;
	ULONG iPin;
	POINT ptPin;

	// Only used with output pins
	LPCLIST lpConn;
	INT cConn;
	BOOL fHighlight;
} CHIP_PIN, *LPCHIP_PIN;

class CAccessibleNeurone;

class CNeuralChip : public INeuralChip, public INetDocObject
{
private:
	ULONG m_cRef;

protected:
	INT m_x;
	INT m_y;

	DWORD m_dwState;

	RECT m_rcPos;

	INeuralNet* m_lpNet;

	LPCHIP_PIN m_lpInput;
	INT m_cInputPins;
	ULONG m_nHighlightPin;

	LPCHIP_PIN m_lpOutput;
	INT m_cOutputPins;
	ULONG m_nHighlightPinConn;
	INT m_iHighlightIndex;

	IAccessibleNeurone* m_lpAccessible;

public:
	CNeuralChip ();
	~CNeuralChip ();

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

	// INeuralChip
	virtual INeuralNet* GetEmbeddedNet (VOID);
	virtual VOID ReceiveOutputValue (FLOAT fValue, ULONG iPin);
	virtual VOID UnloadEmbeddedNet (VOID);

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

	HRESULT LoadFromFile (INeuralFactory* pFactory, INeuralLinks* lpLinks, PCSTR pcszFile, INT cchFile);

protected:
	HRESULT LoadPins (LPNLIST lpObjects);

	VOID ResetChipPins (LPCHIP_PIN& pPins, INT& cPins);
	VOID Reset (VOID);
	VOID RecalculatePosition (VOID);

	static BOOL FindPinByPos (INT x, INT y, LPCHIP_PIN lpPins, INT cPins, INT* lpnIndex);
	static BOOL FindPin (ULONG iPin, LPCHIP_PIN lpPins, INT cPins, INT* lpnIndex);
};