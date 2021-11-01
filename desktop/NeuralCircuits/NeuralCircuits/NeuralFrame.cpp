#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\GraphCtrl.h"
#include "Library\MenuUtil.h"
#include "resource.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "BaseContainer.h"
#include "NeuralNet.h"
#include "Library\Window\DialogHost.h"
#include "GetTextDlg.h"
#include "ColorPickerDlg.h"
#include "NeuralFrame.h"

CNeuralFrame::CNeuralFrame ()
{
	m_cRef = 1;

	m_hwndParent = NULL;

	ZeroMemory(&m_rcPos,sizeof(RECT));

	m_lpszLabel = NULL;
	m_cchLabel = 0;

	m_crBackground = RGB(128,128,144);
	m_bAlpha = 64;

	m_fSelected = FALSE;
	m_cHighlight = FALSE;

	m_lpList = NULL;
	m_cList = 0;
	m_cMaxList = 0;
}

CNeuralFrame::~CNeuralFrame ()
{
	__delete_array m_lpszLabel;

	for(INT i = 0; i < m_cList; i++)
		m_lpList[i]->Release();
	__delete_array m_lpList;
}

HRESULT CNeuralFrame::SetFrameSize (INT xSize, INT ySize)
{
	INT x, y;
	GetPosition(x,y);
	m_rcPos.left = x - (xSize >> 1);
	m_rcPos.top = y + (ySize >> 1);
	m_rcPos.right = x + (xSize >> 1);
	m_rcPos.bottom = y - (ySize >> 1);
	return S_OK;
}

// IUnknown

HRESULT WINAPI CNeuralFrame::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == __uuidof(INeuralFrame))
			*lplpvObject = (INeuralFrame*)this;
		else if(iid == __uuidof(INetDocObject))
			*lplpvObject = (INetDocObject*)this;
		else if(iid == IID_IUnknown)
			*lplpvObject = (IUnknown*)(INetDocObject*)this;
		else
		{
			hr = E_NOINTERFACE;
			goto exit;
		}
		AddRef();
		hr = S_OK;
	}
exit:
	return hr;
}

ULONG WINAPI CNeuralFrame::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CNeuralFrame::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// INetCycleProcessor

VOID CNeuralFrame::SendPulses (VOID)
{
}

VOID CNeuralFrame::CheckThresholds (VOID)
{
}

// INetObject

HRESULT CNeuralFrame::GetObjectClass (LPSTR lpszClass, INT cchMaxClass, __out INT* pcchClass)
{
	return TStrCchCpyLen(lpszClass, cchMaxClass, SLP("CNeuralFrame"), pcchClass);
}

VOID CNeuralFrame::GetPosition (INT& x, INT& y)
{
	x = (m_rcPos.left + m_rcPos.right) >> 1;
	y = (m_rcPos.top + m_rcPos.bottom) >> 1;
}

VOID CNeuralFrame::ReceiveValue (FLOAT fValue, ULONG iPin)
{
	UNREFERENCED_PARAMETER(fValue);
	UNREFERENCED_PARAMETER(iPin);
}

ULONG CNeuralFrame::GetInputPin (INT x, INT y)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return 0xFFFFFFFF;
}

BOOL CNeuralFrame::GetInputPinPosition (ULONG iPin, INT& x, INT& y)
{
	UNREFERENCED_PARAMETER(iPin);
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return FALSE;
}

HRESULT CNeuralFrame::Load (INeuralFactory* pFactory, LPNLOADDATA lpLoadData, DWORD cLoadData, LPBYTE lpData, ULONG cbData)
{
	UNREFERENCED_PARAMETER(pFactory);

	HRESULT hr = S_OK;

	LPVOID lpList[] = {&m_rcPos, &m_crBackground, &m_bAlpha, &m_cchLabel};
	ULONG cbList[] = {sizeof(m_rcPos), sizeof(m_crBackground), sizeof(m_bAlpha), sizeof(m_cchLabel)};
	INT c = sizeof(cbList) / sizeof(cbList[0]);

	for(INT i = 0; i < c; i++)
	{
		if(cbData < cbList[i])
		{
			hr = E_FAIL;
			break;
		}

		CopyMemory(lpList[i],lpData,cbList[i]);
		lpData += cbList[i];
		cbData -= cbList[i];
	}

	if(SUCCEEDED(hr))
	{
		if(0 < m_cchLabel)
		{
			if((ULONG)m_cchLabel < cbData)
			{
				m_lpszLabel = __new CHAR[m_cchLabel + 1];
				if(m_lpszLabel)
				{
					CopyMemory(m_lpszLabel,lpData,m_cchLabel);
					m_lpszLabel[m_cchLabel] = 0;

					lpData += m_cchLabel;
					cbData -= m_cchLabel;
				}
				else
					hr = E_OUTOFMEMORY;
			}
			else
				hr = E_FAIL;
		}
	}

	if(SUCCEEDED(hr))
	{
		if(sizeof(INT) <= cbData)
		{
			INT c, n;

			CopyMemory(&c, lpData, sizeof(INT));
			lpData += sizeof(INT);
			cbData -= sizeof(INT);

			for(INT i = 0; i < c; i++)
			{
				if(sizeof(INT) > cbData)
				{
					hr = E_FAIL;
					break;
				}

				CopyMemory(&n, lpData, sizeof(INT));
				lpData += sizeof(INT);
				cbData -= sizeof(INT);

				if((DWORD)n >= cLoadData)
				{
					hr = E_FAIL;
					break;
				}

				hr = AddObject(lpLoadData[n].lpObject);
				if(FAILED(hr))
					break;
			}
		}
		else
			hr = E_FAIL;
	}

	return hr;
}

HRESULT CNeuralFrame::Save (INeuralNet* lpNet, ISequentialStream* lpStream)
{
	HRESULT hr;
	ULONG cbWritten;

	// Write position.
	hr = lpStream->Write(&m_rcPos,sizeof(RECT),&cbWritten);

	// Write colors.
	if(SUCCEEDED(hr))
		hr = lpStream->Write(&m_crBackground,sizeof(COLORREF),&cbWritten);
	if(SUCCEEDED(hr))
		hr = lpStream->Write(&m_bAlpha,sizeof(BYTE),&cbWritten);

	// Write label.
	if(SUCCEEDED(hr))
		hr = lpStream->Write(&m_cchLabel,sizeof(INT),&cbWritten);
	if(SUCCEEDED(hr) && 0 < m_cchLabel)
		hr = lpStream->Write(m_lpszLabel,m_cchLabel,&cbWritten);

	// Write contained objects.
	if(SUCCEEDED(hr))
		hr = lpStream->Write(&m_cList,sizeof(INT),&cbWritten);
	if(SUCCEEDED(hr) && 0 < m_cList)
	{
		INT n;

		for(INT i = 0; i < m_cList; i++)
		{
			n = lpNet->GetObjectIndex(m_lpList[i]);
			if(-1 == n)
			{
				hr = E_UNEXPECTED;
				break;
			}
			hr = lpStream->Write(&n,sizeof(INT),&cbWritten);
			if(FAILED(hr))
				break;
		}
	}

	return hr;
}

// INetDocObject

INT CNeuralFrame::GetZOrder (VOID)
{
	return 0;
}

VOID CNeuralFrame::DrawBackground (IGrapher* lpGraph)
{
	LPBYTE lpBuffer;
	INT nWidth, nHeight;
	LONG lPitch;
	if(lpGraph->GetRawBuffer(lpBuffer,nWidth,nHeight,lPitch))
	{
		COLORREF cr = m_crBackground;
		BYTE bAlpha = m_bAlpha;
		RECT rc;

		lpGraph->GraphToClientRect(&m_rcPos,&rc);

		if(m_fSelected)
		{
			cr = DIBDrawing::BlendAdditive(cr,RGB(0xFF,0,0));
			if((INT)bAlpha << 1 > 255)
				bAlpha = 255;
			else
				bAlpha <<= 1;
		}

		DIBDrawing::AlphaFill24(lpBuffer,nWidth,nHeight,lPitch,&rc,cr,bAlpha);

		if(0 < m_cHighlight)
		{
			COLORREF crBorder = DIBDrawing::BlendAdditive(m_crBackground,RGB(0,255,0));
			GdiList::DrawOuterBorder(lpBuffer,nWidth,nHeight,lPitch,&rc,2,crBorder,192);
		}
		else
		{
			HBRUSH hbrDef = lpGraph->SelectBrush((HBRUSH)GetStockObject(NULL_BRUSH));
			HPEN hpnDef = lpGraph->SelectPen(GdiList::hpnBorder);
			lpGraph->Rectangle((FLOAT)m_rcPos.left,(FLOAT)m_rcPos.top,0,(FLOAT)m_rcPos.right,(FLOAT)m_rcPos.bottom,0);
			lpGraph->SelectPen(hpnDef);
			lpGraph->SelectBrush(hbrDef);
		}
	}
}

VOID CNeuralFrame::DrawForeground (IGrapher* lpGraph)
{
	UNREFERENCED_PARAMETER(lpGraph);

	if(m_lpszLabel && GdiList::hfFrameLabels)
	{
		HFONT hfDef = lpGraph->SelectFont(GdiList::hfFrameLabels);
		SIZE sText;

		if(GetTextExtentPoint32(lpGraph->GetClientDC(),m_lpszLabel,m_cchLabel,&sText))
		{
			FLOAT fScale = lpGraph->GetScale();
			HPEN hpnDef = lpGraph->SelectPen(GdiList::hpnBorder);
			FLOAT x, y;
			FLOAT fxText = (FLOAT)sText.cx / fScale;
			FLOAT fyText = (FLOAT)sText.cy / fScale;

			x = (FLOAT)(((m_rcPos.left + m_rcPos.right) >> 1) - (fxText / 2.0f));
			y = (FLOAT)(m_rcPos.top + (fyText / 2.0f));

			lpGraph->RoundRect(x - 2,y + 2,0,x + fxText + 2,(y - fyText) - 2,0,4.0f,4.0f);
			lpGraph->TextOut(x,y,0,m_lpszLabel,m_cchLabel);

			lpGraph->SelectPen(hpnDef);
		}

		lpGraph->SelectFont(hfDef);
	}
}

VOID CNeuralFrame::SelectObject (BOOL fSelect)
{
	m_fSelected = fSelect;
}

VOID CNeuralFrame::MoveObject (INT xDelta, INT yDelta)
{
	m_rcPos.left += xDelta;
	m_rcPos.top += yDelta;
	m_rcPos.right += xDelta;
	m_rcPos.bottom += yDelta;

	// Update the contained objects.
	for(INT i = 0; i < m_cList; i++)
	{
		// Only replicate the movement if the object is not selected.
		// If it is selected, it will be moved by the document.
		if(0 == (m_lpList[i]->GetAccState() & STATE_SYSTEM_SELECTED))
			m_lpList[i]->MoveObject(xDelta,yDelta);
	}
}

VOID CNeuralFrame::ResizeObject (INT nHitTest, INT xResizePos, INT yResizePos, INT xDelta, INT yDelta)
{
	switch(nHitTest)
	{
	case HITTEST_RESIZE_V:
		if(yResizePos >= m_rcPos.top - 1)
		{
			if(m_rcPos.top + yDelta > m_rcPos.bottom)
				m_rcPos.top += yDelta;
		}
		else
		{
			if(m_rcPos.bottom + yDelta < m_rcPos.top)
				m_rcPos.bottom += yDelta;
		}
		break;
	case HITTEST_RESIZE_H:
		if(xResizePos <= m_rcPos.left + 1)
		{
			if(m_rcPos.left + xDelta < m_rcPos.right)
				m_rcPos.left += xDelta;
		}
		else
		{
			if(m_rcPos.right + xDelta > m_rcPos.left)
				m_rcPos.right += xDelta;
		}
		break;
	case HITTEST_RESIZE_C1:
	case HITTEST_RESIZE_C2:
		ResizeObject(HITTEST_RESIZE_V,xResizePos,yResizePos,xDelta,yDelta);
		ResizeObject(HITTEST_RESIZE_H,xResizePos,yResizePos,xDelta,yDelta);
		break;
	}
}

INT CNeuralFrame::HitTest (INT x, INT y)
{
	INT nHitTest = HITTEST_NONE;
	BOOL fInClient = FALSE;
	if(!HitTestResizeDir(x,y,nHitTest,fInClient) && fInClient)
		nHitTest = HITTEST_SELECTABLE | HITTEST_CONTEXT | HITTEST_DRAG_FRAME;
	return nHitTest;
}

BOOL CNeuralFrame::GetDragSourcePin (INT x, INT y, ULONG& iPin)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);
	UNREFERENCED_PARAMETER(iPin);

	return FALSE;
}

BOOL CNeuralFrame::GetDragSourcePoint (ULONG iPin, INT& x, INT& y)
{
	UNREFERENCED_PARAMETER(iPin);
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return FALSE;
}

BOOL CNeuralFrame::HighlightPin (ULONG iPin, BOOL fHighlight)
{
	UNREFERENCED_PARAMETER(iPin);
	UNREFERENCED_PARAMETER(fHighlight);

	return FALSE;
}

BOOL CNeuralFrame::HighlightConn (ULONG iPin, INT index)
{
	UNREFERENCED_PARAMETER(iPin);
	UNREFERENCED_PARAMETER(index);

	return FALSE;
}

VOID CNeuralFrame::NotifyRemovalOf (INetDocObject* lpObject)
{
	RemoveObject(lpObject);
}

HRESULT CNeuralFrame::ConnectTo (ULONG iSourcePin, INetDocObject* lpTarget, ULONG iTargetPin, FLOAT fWeight)
{
	UNREFERENCED_PARAMETER(iSourcePin);
	UNREFERENCED_PARAMETER(lpTarget);
	UNREFERENCED_PARAMETER(iTargetPin);
	UNREFERENCED_PARAMETER(fWeight);

	return E_NOTIMPL;
}

HRESULT CNeuralFrame::ContextMenu (IBaseContainer* lpContainer, INT x, INT y)
{
	HRESULT hr;

	HMENU hPopup = MenuUtil::LoadPopupMenu(GetModuleHandle(NULL), IDR_FRAME);
	if(hPopup)
	{
		hr = lpContainer->GetHwnd(&m_hwndParent);
		if(SUCCEEDED(hr))
			hr = lpContainer->TrackPopupMenu(hPopup,x,y,this);
		DestroyMenu(hPopup);
	}
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	return hr;
}

BOOL CNeuralFrame::Click (INT x, INT y)
{
	UNREFERENCED_PARAMETER(x);
	UNREFERENCED_PARAMETER(y);

	return FALSE;
}

BOOL CNeuralFrame::PressChar (CHAR ch)
{
	UNREFERENCED_PARAMETER(ch);

	return FALSE;
}

LONG CNeuralFrame::GetAccState (VOID)
{
	LONG lAccState = 0;
	if(m_fSelected)
		lAccState |= STATE_SYSTEM_SELECTED;
	return lAccState;
}

HRESULT CNeuralFrame::GetAccObject (IAccessibleNetDoc* lpParent, IAccessible** lplpAccessible)
{
	UNREFERENCED_PARAMETER(lpParent);
	UNREFERENCED_PARAMETER(lplpAccessible);

	return E_NOTIMPL;
}

HRESULT CNeuralFrame::UnloadAccessibility (VOID)
{
	return E_NOTIMPL;
}

HRESULT CNeuralFrame::GetGraphInputCaptureObject (INeuralDocument* lpDocument, IGraphInputCapture** lplpCapture)
{
	UNREFERENCED_PARAMETER(lpDocument);
	UNREFERENCED_PARAMETER(lplpCapture);

	return E_NOTIMPL;
}

// INeuralFrame

HRESULT CNeuralFrame::AddObject (INetDocObject* lpObject)
{
	HRESULT hr;
	if(!ContainsObject(lpObject) && this != lpObject)
	{
		if(m_cList < m_cMaxList)
		{
			m_lpList[m_cList] = lpObject;
			m_lpList[m_cList]->AddRef();
			m_cList++;
			hr = S_OK;
		}
		else
		{
			INetDocObject** lpNew = __new INetDocObject*[m_cList + 1];
			if(lpNew)
			{
				CopyMemory(lpNew,m_lpList,m_cList * sizeof(INetDocObject*));
				lpNew[m_cList] = lpObject;
				lpNew[m_cList]->AddRef();
				m_cList++;

				__delete_array m_lpList;
				m_lpList = lpNew;
				m_cMaxList = m_cList;

				hr = S_OK;
			}
			else
				hr = E_OUTOFMEMORY;
		}
	}
	else
		hr = S_FALSE;
	return hr;
}

HRESULT CNeuralFrame::RemoveObject (INetDocObject* lpObject)
{
	HRESULT hr = S_FALSE;
	for(INT i = 0; i < m_cList; i++)
	{
		if(m_lpList[i] == lpObject)
		{
			MoveMemory(m_lpList + i,m_lpList + i + 1,(m_cList - (i + 1)) * sizeof(INetDocObject*));

			// If the __new list is less than half the size of the maximum array,
			// let's reduce the size of the allocated list to free memory.
			if(--m_cList < m_cMaxList >> 1)
			{
				if(0 == m_cList)
				{
					__delete_array m_lpList;
					m_lpList = NULL;
					m_cMaxList = 0;
				}
				else
				{
					INetDocObject** lpNew = __new INetDocObject*[m_cList];
					if(lpNew)
					{
						CopyMemory(lpNew,m_lpList,m_cList * sizeof(INetDocObject*));
						__delete_array m_lpList;
						m_lpList = lpNew;
						m_cMaxList = m_cList;
					}
				}
			}

			lpObject->Release();

			hr = S_OK;
			break;
		}
	}
	return hr;
}

BOOL CNeuralFrame::ContainsObject (INetDocObject* lpObject)
{
	BOOL fContained = FALSE;
	for(INT i = 0; i < m_cList; i++)
	{
		if(m_lpList[i] == lpObject)
		{
			fContained = TRUE;
			break;
		}
	}
	return fContained;
}

VOID CNeuralFrame::HighlightFrame (BOOL fHighlight)
{
	if(fHighlight)
		m_cHighlight++;
	else
		m_cHighlight--;
}

HRESULT CNeuralFrame::SetFrameLabel (LPCSTR lpcszLabel)
{
	return TReplaceStringChecked(lpcszLabel, &m_lpszLabel, &m_cchLabel);
}

LPCSTR CNeuralFrame::GetFrameLabel (VOID)
{
	return m_lpszLabel;
}

// IOleCommandTarget

HRESULT WINAPI CNeuralFrame::QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText)
{
	UNREFERENCED_PARAMETER(lpguidCmdGroup);
	UNREFERENCED_PARAMETER(lpCmdText);

	for(ULONG i = 0; i < cCmds; i++)
	{
		switch(lprgCmds[i].cmdID)
		{
		case ID_FRAME_RENAME:
		case ID_FRAME_COLOR:
		case ID_RANDOMIZE_WEIGHTS:
			lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
			break;
		}
	}

	return S_OK;
}

HRESULT WINAPI CNeuralFrame::Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut)
{
	UNREFERENCED_PARAMETER(lpguidCmdGroup);
	UNREFERENCED_PARAMETER(nCmdExecOpt);
	UNREFERENCED_PARAMETER(lpvaIn);
	UNREFERENCED_PARAMETER(lpvaOut);

	HRESULT hr = OLECMDERR_E_NOTSUPPORTED;

	switch(nCmdID)
	{
	case ID_FRAME_RENAME:
		DoRenameDlg();
		hr = S_OK;
		break;
	case ID_FRAME_COLOR:
		DoSetColorDlg();
		hr = S_OK;
		break;
	case ID_RANDOMIZE_WEIGHTS:
		DoRandomizeWeights();
		hr = S_OK;
		break;
	}

	return hr;
}

BOOL CNeuralFrame::HitTestResizeDir (INT x, INT y, INT& nHitTest, BOOL& fInClient)
{
	if(x >= m_rcPos.left - 1)
	{
		if(x <= m_rcPos.left + 1)
		{
			if(y <= m_rcPos.top + 1)
			{
				if(y >= m_rcPos.top - 1)
					nHitTest = HITTEST_RESIZE_C1;
				else if(y >= m_rcPos.bottom + 1)
					nHitTest = HITTEST_RESIZE_H;
				else if(y >= m_rcPos.bottom - 1)
					nHitTest = HITTEST_RESIZE_C2;
			}
		}
		else if(x < m_rcPos.right - 1)
		{
			if(y <= m_rcPos.top + 1 && y >= m_rcPos.top - 1)
				nHitTest = HITTEST_RESIZE_V;
			else if(y <= m_rcPos.bottom + 1 && y >= m_rcPos.bottom - 1)
				nHitTest = HITTEST_RESIZE_V;
			else if(y < m_rcPos.top && y > m_rcPos.bottom)
				fInClient = TRUE;
		}
		else if(x <= m_rcPos.right + 1)
		{
			if(y <= m_rcPos.top + 1)
			{
				if(y >= m_rcPos.top - 1)
					nHitTest = HITTEST_RESIZE_C2;
				else if(y >= m_rcPos.bottom + 1)
					nHitTest = HITTEST_RESIZE_H;
				else if(y >= m_rcPos.bottom - 1)
					nHitTest = HITTEST_RESIZE_C1;
			}
		}
	}

	return (HITTEST_NONE != nHitTest);
}

VOID CNeuralFrame::DoRenameDlg (VOID)
{
	HINSTANCE hInstance = (HINSTANCE)GetWindowLong(m_hwndParent,GWL_HINSTANCE);
	CGetTextDlg GetText;
	CDialogHost Dialog(hInstance);

	GetText.SetText(m_lpszLabel);

	if(SUCCEEDED(Dialog.Display(m_hwndParent,&GetText)))
	{
		if(IDOK == Dialog.GetReturnValue())
		{
			TReplaceStringChecked(GetText.GetText(), &m_lpszLabel, &m_cchLabel);
		}
	}
}

VOID CNeuralFrame::DoSetColorDlg (VOID)
{
	HINSTANCE hInstance = (HINSTANCE)GetWindowLong(m_hwndParent,GWL_HINSTANCE);
	CColorPickerDlg ColorPicker;
	CDialogHost Dialog(hInstance);

	ColorPicker.SetSolidColor(m_crBackground);
	ColorPicker.SetAlphaChannel(m_bAlpha);

	if(SUCCEEDED(Dialog.Display(m_hwndParent,&ColorPicker)))
	{
		if(IDOK == Dialog.GetReturnValue())
		{
			m_crBackground = ColorPicker.GetSolidColor();
			m_bAlpha = ColorPicker.GetAlphaChannel();
		}
	}
}

VOID CNeuralFrame::DoRandomizeWeights (VOID)
{
	INeurone* lpNeurone;

	srand(GetTickCount());

	for(INT i = 0; i < m_cList; i++)
	{
		if(SUCCEEDED(m_lpList[i]->QueryInterface(&lpNeurone)))
		{
			lpNeurone->RunTrainer(TRAIN_RANDOMIZE_WEIGHTS, 0.0f, 0, 0.0f, 0);
			lpNeurone->Release();
		}
	}
}