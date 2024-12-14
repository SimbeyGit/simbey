#pragma once

#include "Library\Core\BaseUnknown.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Core\MemoryFile.h"
#include "Library\Window\BaseDialog.h"
#include "Library\Window\AdapterWindow.h"
#include "Library\GraphCtrl.h"
#include "..\Published\zdbsp_published.h"
#include "BlockMap.h"
#include "MapConvert.h"

class CConfigDlg;

struct DRAWLINE
{
	INT x1, y1, x2, y2;
};

struct DRAWTHING
{
	INT x, y;
};

class CZDBSPFile :
	public IZDBSPFile
{
private:
	CMemoryFile m_memFile;

public:
	CZDBSPFile ();
	CZDBSPFile (CMemFileData* pmfd);

	HRESULT InitializeNew (VOID);
	CMemFileData* GetFileData (VOID) const;

	// IZDBSPFile
	virtual void Close (void);
	virtual size_t Read (void* pvData, size_t cbMaxData);
	virtual size_t Write (const void* pcvData, size_t cbData);
	virtual int Seek (long nOffset, int nOrigin);
	virtual long Tell (void);
};

class CZDBSPFS :
	public IZDBSPFS,
	public IZDBSPLogger
{
private:
	CMemoryStream* m_pstmLogs;
	TNamedMap<CHAR, CZDBSPFile*> m_mapFiles;

public:
	CZDBSPFS (CMemoryStream* pstmLogs);
	~CZDBSPFS ();

	HRESULT AddFile (PCSTR pcszFile, CMemoryFile* pmemFile);
	HRESULT GetFileData (PCSTR pcszFile, __deref_out CMemFileData** ppmfd);

	// IZDBSPFS
	virtual bool CheckInOutNames (const char* pcszInName, const char* pcszOutName);
	virtual int Remove (const char* pcszName);
	virtual int Rename (const char* pcszOld, const char* pcszNew);
	virtual IZDBSPFile* OpenForReading (const char* pcszFile);
	virtual IZDBSPFile* OpenForWriting (const char* pcszFile);

	// IZDBSPLogger
	virtual int Log (const char* pcszFormat, ...);
	virtual int LogV (const char* pcszFormat, va_list vArgs);
};

class CMapConverterView :
	public CBaseUnknown,
	public IAdapterWindowCallback,
	public IGraphContainer,
	public IGraphClient,
	public IMapConvertProgress
{
private:
	IBaseWindow* m_pAdapter;
	CBlockMap* m_pBlockMap;
	CHAR m_szLevel[12];
	CGraphCtrl m_Graph;

	CMemoryStream m_stmLogs;
	CMemoryFile m_memFile;
	CConfigDlg* m_pdlgConfig;

	CMapConvert* m_pConvert;
	HANDLE m_hThread;

	TArray<DRAWLINE*> m_aLines;
	TArray<DRAWLINE*> m_aMerges;
	TArray<DRAWTHING*> m_aThings;

public:
	IMP_BASE_UNKNOWN

	BEGIN_UNK_MAP
		UNK_INTERFACE(IAdapterWindowCallback)
	END_UNK_MAP

public:
	CMapConverterView (CBlockMap* pBlockMap, PCWSTR pcwzName, INT xCell, INT zCell, CConfigDlg* pdlgConfig);
	~CMapConverterView ();

	CMemoryStream* GetLogs (VOID) { return &m_stmLogs; }
	CMemoryFile* GetFile (VOID) { return &m_memFile; }

	// IAdapterWindowCallback
	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	virtual VOID OnAttachingAdapter (IBaseWindow* pAdapter);
	virtual VOID OnDetachingAdapter (IBaseWindow* pAdapter);

	// IGraphContainer
	virtual VOID WINAPI OnScaleChanged (FLOAT fScale) {}
	virtual VOID WINAPI OnGridSpacingChanged (INT iSpacing) {}
	virtual HRESULT WINAPI SetFocus (__in IGrapher* pGraphCtrl);
	virtual HRESULT WINAPI ScreenToWindowless (__in IGrapher* pGraphCtrl, __inout PPOINT ppt);
	virtual HRESULT WINAPI InvalidateContainer (__in IGrapher* pGraphCtrl);
	virtual VOID WINAPI DrawDib24 (HDC hdcDest, LONG x, LONG y, HDC hdcSrc, const BYTE* pcDIB24, LONG xSize, LONG ySize, LONG lPitch);
	virtual BOOL WINAPI CaptureMouse (__in IGrapher* pGraphCtrl, BOOL fCapture);

	// IGraphClient
	virtual VOID onGraphPaint (IGrapher* lpGraph);
	virtual VOID onGraphMouseMove (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphLBtnDown (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphLBtnUp (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual BOOL onGraphRBtnDown (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphRBtnUp (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphLBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphRBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y);
	virtual VOID onGraphViewChanged (BOOL fZoomChanged);
	virtual BOOL onGraphKeyDown (WPARAM iKey);
	virtual BOOL onGraphKeyUp (WPARAM iKey);
	virtual BOOL onGraphChar (WPARAM iKey);
	virtual BOOL onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y);
	virtual HRESULT onGraphGetAcc (IAccessible** lplpAccessible);

	// IMapConvertProgress
	virtual VOID ReportAddLine (CMapLine* pLine);
	virtual VOID ReportMergeDone (CMapLine* pLine);
	virtual VOID ReportAddThing (CMapThing* pThing);

private:
	VOID DrawLine (HDC hdc, DRAWLINE* pLine);
	VOID DrawThing (HDC hdc, DRAWTHING* pThing);

	HRESULT BuildNodes (VOID);
	HRESULT ConvertMapAsync (VOID);
	VOID CloseConversion (VOID);

	static DWORD WINAPI _ConversionEntry (PVOID pvParam);
};

class CExportDlg : public CBaseDialog
{
private:
	CMapConverterView* m_pConverter;
	INT m_nBottomMargin;
	INT m_xSaveRight, m_xCancelRight;
	INT m_yButtonsDelta;
	INT m_xStatus, m_yStatusDelta;
	SIZE m_sizeMin;

public:
	CExportDlg ();
	~CExportDlg ();

	HRESULT Initialize (CBlockMap* pBlockMap, PCWSTR pcwzName, CConfigDlg* pdlgConfig);

	virtual BOOL DefWindowProc (UINT message, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

private:
	VOID AdjustLayout (INT xSize, INT ySize, BOOL fRepaint);
	VOID CheckZDBSPLogs (VOID);
};
