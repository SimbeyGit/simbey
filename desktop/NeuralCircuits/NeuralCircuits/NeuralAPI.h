#pragma once

#include <OleAcc.h>
#include <DocObj.h>

interface INetDocObject;
interface INeuralNet;
interface INeuralSource;
interface INeuralLinks;
interface INeurone;
interface IAccessibleNetDoc;

interface IGrapher;
interface IAccessible;
interface IBaseContainer;
interface IWindowless;
interface INeuralDocument;
interface IGraphInputCapture;

#define	HITTEST_SUBTYPE(x)			(x & 0x0f)

#define	HITTEST_NONE				0x00
#define	HITTEST_INTERACTIVE			0x01
#define	HITTEST_DRAG_SOURCE			0x02
#define	HITTEST_DRAG_SOURCE_TOGGLE	0x03
#define	HITTEST_DRAG_TARGET			0x04
#define	HITTEST_DRAG_FRAME			0x05
#define	HITTEST_RESIZE				0x08		// Hit test can be checked using this
#define	HITTEST_RESIZE_V			0x08
#define	HITTEST_RESIZE_H			0x09
#define	HITTEST_RESIZE_C1			0x0A
#define	HITTEST_RESIZE_C2			0x0B
#define	HITTEST_SELECTABLE			0x10		// This can be combined with the others
#define	HITTEST_CONTEXT				0x20		// This can be combined with the others

#define	NSTATE_NORMAL				0x00
#define	NSTATE_EXCITED				0x01
#define	NSTATE_SELECTED				0x10		// This can be combined with the others
#define	NSTATE_HIGHLIGHT			0x20		// This can be combined with the others

#define	PLUGIN_MENU_EXT_APP			0x00
#define	PLUGIN_MENU_EXT_NEURONES	0x01
#define	PLUGIN_MENU_EXT_GROUPS		0x02

#define	PLUGIN_FEATURE_SOURCE		"LINKSOURCE"

#define	DEFAULT_SOURCE_IO_NAME		"<Default Source>"

typedef enum
{
	INPUT_NEURONE,
	OUTPUT_NEURONE,
	INPUT_LINK,
	OUTPUT_LINK
} EIO_TYPE;

typedef struct
{
	INeuralLinks* lpLinks;
	INetDocObject* lpObject;
	LPBYTE lpData;
	DWORD cbData;
} NLOADDATA, *LPNLOADDATA;

typedef struct
{
	FLOAT left;
	FLOAT top;
	FLOAT right;
	FLOAT bottom;
} NRECT, *LPNRECT;

typedef VOID (WINAPI* ENUMCONNLIST)(INT, INeurone*, INeurone*, ULONG, FLOAT, LPVOID);

interface __declspec(uuid("F0E032E9-07C0-478c-B2C3-190E94F6BF46")) IMessageHandler : IUnknown
{
	virtual BOOL OnMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult) = 0;
};

interface INeuralFactory : IUnknown
{
	virtual HRESULT Create (__in_opt INeuralNet* pNet, PCSTR pcszClass, __deref_out INetDocObject** ppObject) = 0;
};

interface INetCycleProcessor : IUnknown
{
	virtual VOID SendPulses (VOID) = 0;
	virtual VOID CheckThresholds (VOID) = 0;
};

interface INetObject : INetCycleProcessor
{
	virtual HRESULT GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass) = 0;
	virtual VOID GetPosition (INT& x, INT& y) = 0;
	virtual VOID ReceiveValue (FLOAT fValue, ULONG iPin) = 0;
	virtual ULONG GetInputPin (INT x, INT y) = 0;
	virtual BOOL GetInputPinPosition (ULONG iPin, INT& x, INT& y) = 0;
	virtual HRESULT Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData) = 0;
	virtual HRESULT Save (INeuralNet* lpNet, ISequentialStream* lpStream) = 0;
};

interface __declspec(uuid("1D5333F0-6C38-4551-B3BB-03747690CFAE")) INetDocObject : INetObject
{
	virtual INT GetZOrder (VOID) = 0;
	virtual VOID DrawBackground (IGrapher* lpGraph) = 0;
	virtual VOID DrawForeground (IGrapher* lpGraph) = 0;
	virtual VOID SelectObject (BOOL fSelect) = 0;
	virtual VOID MoveObject (INT xDelta, INT yDelta) = 0;
	virtual VOID ResizeObject (INT nHitTest, INT xResizePos, INT yResizePos, INT xDelta, INT yDelta) = 0;
	virtual INT HitTest (INT x, INT y) = 0;
	virtual BOOL GetDragSourcePin (INT x, INT y, ULONG& iPin) = 0;
	virtual BOOL GetDragSourcePoint (ULONG iPin, INT& x, INT& y) = 0;
	virtual BOOL HighlightPin (ULONG iPin, BOOL fHighlight) = 0;
	virtual BOOL HighlightConn (ULONG iPin, INT index) = 0;
	virtual VOID NotifyRemovalOf (INetDocObject* lpObject) = 0;
	virtual HRESULT ConnectTo (ULONG iSourcePin, INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight) = 0;
	virtual HRESULT ContextMenu (IBaseContainer* lpContainer, INT x, INT y) = 0;
	virtual BOOL Click (INT x, INT y) = 0;
	virtual BOOL PressChar (CHAR ch) = 0;
	virtual LONG GetAccState (VOID) = 0;
	virtual HRESULT GetAccObject (IAccessibleNetDoc* lpParent, IAccessible** lplpAccessible) = 0;
	virtual HRESULT UnloadAccessibility (VOID) = 0;
	virtual HRESULT GetGraphInputCaptureObject (INeuralDocument* lpDocument, IGraphInputCapture** lplpCapture) = 0;
};

interface __declspec(uuid("FF7D045A-3230-42e5-AFB0-752DE618DD26")) INeurone : INetObject
{
	virtual VOID ResetNeurone (VOID) = 0;
	virtual INT CountConnections (ULONG iSourcePin) = 0;
	virtual VOID EnumConnections (ULONG iSourcePin, ENUMCONNLIST lpfnCallback, LPVOID lpParam) = 0;
	virtual BOOL ClearConnection (ULONG iSourcePin, INT index) = 0;
	virtual BOOL SetConnectionWeight (ULONG iSourcePin, INT index, FLOAT fWeight) = 0;
	virtual BOOL GetCurrentValue (FLOAT& fValue) = 0;
	virtual VOID SetThreshold (FLOAT fThreshold) = 0;
	virtual FLOAT GetThreshold (VOID) = 0;
	virtual VOID RunTrainer (DWORD dwTrainer, FLOAT fInput, ULONG iInputPin, FLOAT fOutput, ULONG iOutputPin) = 0;
};

interface __declspec(uuid("6EF4344E-611D-4ddf-A210-C68E8D65F471")) INeuralChip : INeurone
{
	virtual INeuralNet* GetEmbeddedNet (VOID) = 0;
	virtual VOID ReceiveOutputValue (FLOAT fValue, ULONG iPin) = 0;
	virtual VOID UnloadEmbeddedNet (VOID) = 0;
};

interface __declspec(uuid("512D532B-EA63-42bc-A316-3B9B0FF24B22")) IIONeurone : INetObject
{
	virtual ULONG GetPin (VOID) = 0;
	virtual VOID SetPin (ULONG iPin) = 0;
	virtual EIO_TYPE GetIOType (VOID) = 0;
	virtual VOID AttachParentChip (INeuralChip* lpParent) = 0;
	virtual BOOL HasParentChip (VOID) = 0;
	virtual HRESULT GetLinkName (LPSTR lpszName, INT cchMaxName) = 0;
	virtual HRESULT SetLinkName (INeuralLinks* lpLinks, LPCSTR lpcszName) = 0;
};

interface __declspec(uuid("A38CCC6B-37BA-425d-9996-F913A0E2FD57")) IBiasNeurone : INetObject
{
	virtual FLOAT GetBias (VOID) = 0;
	virtual VOID SetBias (FLOAT rBias) = 0;
};

interface __declspec(uuid("B9A61AF3-BABB-49f7-A09F-48428722407B")) INeuralFrame : INetDocObject
{
	virtual HRESULT AddObject (INetDocObject* lpObject) = 0;
	virtual HRESULT RemoveObject (INetDocObject* lpObject) = 0;
	virtual BOOL ContainsObject (INetDocObject* lpObject) = 0;
	virtual VOID HighlightFrame (BOOL fHighlight) = 0;
	virtual HRESULT SetFrameLabel (LPCSTR lpcszLabel) = 0;
	virtual LPCSTR GetFrameLabel (VOID) = 0;
};

interface IAccessibleNetDoc : IAccessible
{
	virtual HRESULT NotifyAccEvent (DWORD dwEvent, INetDocObject* lpObject) = 0;
	virtual VOID GraphToClient (FLOAT gx, FLOAT gy, LONG* px, LONG* py) = 0;
	virtual FLOAT GetGraphScale (VOID) = 0;
	virtual HRESULT ScreenToGraph (POINT& ptScreen, FLOAT& fxGraph, FLOAT& fyGraph) = 0;
};

interface IAccessibleNeurone : IAccessible
{
	virtual HRESULT Initialize (INetDocObject* lpObject, INT nWidth, INT nHeight) = 0;
	virtual HRESULT InitConnectors (POINT* lpConnectors, INT cList) = 0;
	virtual HRESULT Select (BOOL fSelect) = 0;
	virtual HRESULT MoveTo (INT x, INT y) = 0;
	virtual HRESULT Delete (VOID) = 0;
	virtual VOID GetObjectRect (LPNRECT lpRect) = 0;
};

interface IGraphInputCapture : IUnknown
{
	virtual BOOL onGraphMouseMove (DWORD dwKeys, FLOAT x, FLOAT y) = 0;
	virtual BOOL onGraphLBtnDown (DWORD dwKeys, FLOAT x, FLOAT y) = 0;
	virtual BOOL onGraphLBtnUp (DWORD dwKeys, FLOAT x, FLOAT y) = 0;
	virtual BOOL onGraphRBtnDown (DWORD dwKeys, FLOAT x, FLOAT y) = 0;
	virtual BOOL onGraphRBtnUp (DWORD dwKeys, FLOAT x, FLOAT y) = 0;
	virtual BOOL onGraphLBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y) = 0;
	virtual BOOL onGraphRBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y) = 0;
	virtual BOOL onGraphKeyDown (WPARAM iKey) = 0;
	virtual BOOL onGraphKeyUp (WPARAM iKey) = 0;
	virtual BOOL onGraphChar (WPARAM iKey) = 0;
	virtual BOOL onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y) = 0;
};

interface __declspec(uuid("A41EDC63-F90C-4f63-AE76-DE583900E111")) INeuralDocument : IUnknown
{
	virtual VOID SetChangesMade (VOID) = 0;
	virtual BOOL IsSavedOnDisk (VOID) = 0;
	virtual BOOL ChangesMade (VOID) = 0;
	virtual VOID ClearSelection (VOID) = 0;
	virtual HRESULT AddToSelection (INetDocObject* lpObject) = 0;
	virtual BOOL RemoveSelection (INetDocObject* lpObject) = 0;
	virtual BOOL InSelection (INetDocObject* lpObject) = 0;
	virtual HRESULT SaveToFile (LPCSTR lpcszFile) = 0;
	virtual BOOL SelectionContainsIO (VOID) = 0;
	virtual BOOL FindOneSelectedChip (INeuralChip** lplpChip) = 0;
	virtual HRESULT DeleteSelection (VOID) = 0;
	virtual HRESULT InsertObject (INetDocObject* lpObject) = 0;
	virtual VOID SetGraphCapture (IGraphInputCapture* lpCapture) = 0;
};

interface INeuralLink : IUnknown
{
	virtual HRESULT Register (__in IIONeurone* lpIO) = 0;
	virtual HRESULT Unregister (__in IIONeurone* lpIO) = 0;
	virtual VOID OutputValue (FLOAT fValue, ULONG iPin) = 0;
	virtual VOID InputValue (FLOAT fValue, ULONG iPin) = 0;
	virtual HRESULT GetLinkName (__out_ecount(cchMaxName) LPSTR lpszName, INT cchMaxName) = 0;
	virtual HRESULT GetLinkSource (__deref_out INeuralSource** lplpSource) = 0;
	virtual VOID SetSourceData (PVOID pvData) = 0;
	virtual PVOID GetSourceData (VOID) = 0;
	virtual HRESULT Remove (VOID) = 0;
};

interface INeuralLinks
{
	virtual VOID EditLink (HWND hwndParent, LPCSTR szName) = 0;
	virtual VOID FillLinksList (HWND hwndBox, LPCSTR lpcszSelected, BOOL fIsComboBox) = 0;
	virtual VOID FillSources (HWND hwndBox) = 0;
	virtual HRESULT GetNeuralLink (LPCSTR lpcszName, __deref_out INeuralLink** ppLink) = 0;
	virtual HRESULT CreateLink (__in_opt HWND hwndParent, LPCSTR lpcszSource, LPCSTR lpcszName) = 0;
	virtual HRESULT Load (LPBYTE* lplpData, DWORD* lpcbData) = 0;
	virtual HRESULT Save (ISequentialStream* lpStream) = 0;
};

interface __declspec(uuid("F7842DC3-55FE-4178-ACA5-832DD2AF26B3")) INeuralSource : IUnknown
{
	virtual HRESULT AttachLink (__in INeuralLink* lpLink) = 0;
	virtual HRESULT DetachLink (__in INeuralLink* lpLink) = 0;
	virtual VOID ReceiveOutputValue (__in INeuralLink* lpLink, FLOAT fValue, ULONG iPin) = 0;
	virtual HRESULT GetSourceName (__out_ecount(cchMaxName) LPSTR lpszName, INT cchMaxName) = 0;
	virtual HRESULT LoadLinkData (__in INeuralLink* lpLink, __in_ecount(cbData) LPBYTE lpData, DWORD cbData) = 0;
	virtual HRESULT SaveLinkData (__in INeuralLink* lpLink, __out ISequentialStream* lpStream) = 0;
	virtual VOID SendPulses (__in INeuralLink* lpLink) = 0;
	virtual VOID EditLinkProperties (HWND hwndParent, __in INeuralLink* lpLink) = 0;
};

interface INeuralPlugins
{
	virtual HRESULT GetServiceByName (LPCSTR lpcszName, DWORD dwVersion, __deref_out IUnknown** ppunkService) = 0;
	virtual HRESULT RegisterCustomMenuCmd (__in IOleCommandTarget* lpTarget, __out INT* pnCmd) = 0;
	virtual HRESULT UnregisterCustomMenuCmd (__in IOleCommandTarget* lpTarget, INT nCmd) = 0;
	virtual HRESULT RegisterCustomMessage (__in IMessageHandler* lpHandler, UINT* pmsg) = 0;
	virtual HRESULT UnregisterCustomMessage (__in IMessageHandler* lpHandler, UINT msg) = 0;
	virtual HRESULT GetDocument (INeuralDocument** lplpDocument) = 0;
	virtual HRESULT SendAsyncMessage (IMessageHandler* pTarget, UINT nMsg, WPARAM wParam, LPARAM lParam) = 0;
};

interface __declspec(uuid("C1D21C12-C118-40ab-952F-748E1B45A0E8")) INeuralPlugin : IUnknown
{
	virtual HRESULT Initialize (VOID) = 0;
	virtual VOID Unload (VOID) = 0;
	virtual HRESULT GetServiceByName (LPCSTR lpcszName, DWORD dwVersionRequested, __deref_out IUnknown** ppunkService) = 0;
	virtual HRESULT GetMultipleFeatureCount (LPCSTR lpcszFeature, __out INT* lpcFeatures) = 0;
	virtual HRESULT GetMultipleFeatures (LPCSTR lpcszFeature, __out_ecount(cFeatures) IUnknown** lplpFeatures, INT cFeatures) = 0;
	virtual BOOL ExtendMenu (HMENU hMenu, INT nExtMenuType) = 0;
};

#define	PROC_GETNEURALPLUGIN	"GetNeuralPlugin"

typedef HRESULT (WINAPI* LPFNGETNEURALPLUGIN)(__in INeuralPlugins* lpManager, __deref_out INeuralPlugin** lplpPlugin);
