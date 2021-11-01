#include <math.h>
#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Util\Registry.h"
#include "Library\ChooseFile.h"
#include "Library\MenuUtil.h"
#include "NeuralAPI.h"
#include "GdiList.h"
#include "Neurone.h"
#include "NeuralChip.h"
#include "NeuralFactory.h"
#include "NeuralNet.h"
#include "AccessibleNetDoc.h"
#include "NeuralDocument.h"
#include "Library\Window\DialogHost.h"
#include "ConnectionsDlg.h"
#include "NeuronePropsDlg.h"
#include "RecentChipsMenu.h"
#include "NeuralFrame.h"
#include "NeuralLinks.h"
#include "EditLinksDlg.h"
#include "Extensibility.h"

const GUID IID_IBaseContainer = { 0xf1f1f7f, 0x1251, 0x48bb, { 0x83, 0xe8, 0x86, 0xac, 0x93, 0xb0, 0xad, 0x76 } };
const GUID IID_INetDocObject = { 0x1d5333f0, 0x6c38, 0x4551, { 0xb3, 0xbb, 0x3, 0x74, 0x76, 0x90, 0xcf, 0xae } };
const GUID IID_INeuralDocument = { 0xa41edc63, 0xf90c, 0x4f63, { 0xae, 0x76, 0xde, 0x58, 0x39, 0x0, 0xe1, 0x11 } };

#define	RECENT_CHIPS_RANGE			100	// Range start for recent chips in the menu

struct OBJECT_MAP
{
	DWORD nCmd;
	PCSTR pcszClass;
};

CNeuralDocument::CNeuralDocument (IOleCommandTarget* lpParent)
{
	m_cRef = 1;

	m_lpParent = lpParent;
	m_lpParent->AddRef();

	m_lpContainer = NULL;

	m_pGraphAdapter = NULL;
	m_lpGraph = NULL;

	m_pFactory = NULL;
	m_lpLinks = NULL;

	m_lpszFile = NULL;
	m_lpNet = NULL;
	m_lpSelectedNet = NULL;
	m_fChangesMade = FALSE;
	m_fDrawLabels = FALSE;
	m_fRunning = FALSE;

	m_lpSelection = NULL;
	m_lpToggle = NULL;
	m_fInsertSelection = FALSE;
	m_fInteractToggle = FALSE;

	m_xDragPos = 0;
	m_yDragPos = 0;
	m_iDragLevel = MOUSE_NONE;

	m_uTimerSpeed = 100;
	m_nTimerIndex = 0;
	m_lpNetNav = NULL;

	m_lpAccessible = NULL;
	m_lpContextMenuTarget = NULL;

	m_lpRecentChips = NULL;

	m_cmdDoubleClick = ID_INSERT_NEURONE;

	m_lpCapture = NULL;
}

CNeuralDocument::~CNeuralDocument ()
{
	Assert(NULL == m_lpCapture);
	Assert(NULL == m_lpContextMenuTarget);
	Assert(NULL == m_lpAccessible);
	Assert(NULL == m_lpContainer);
	Assert(0 == m_nTimerIndex);

	ClearSelection();

	SafeRelease(m_lpRecentChips);

	if(m_lpNetNav)
	{
		LPNETNAVLIST lpTemp;
		while(m_lpNetNav)
		{
			lpTemp = m_lpNetNav;
			m_lpNetNav = m_lpNetNav->Next;
			ClearSelection(lpTemp->lpSelection);
			lpTemp->lpNet->Release();
			__delete lpTemp;
		}
	}

	SafeRelease(m_lpSelectedNet);
	SafeRelease(m_lpNet);

	SafeDeleteArray(m_lpszFile);

	if(m_pGraphAdapter)
	{
		m_pGraphAdapter->AttachContainer(NULL);
		m_pGraphAdapter->Release();
		m_pGraphAdapter = NULL;

		m_lpGraph = NULL;
	}

	// This must be called after all neurones have been unloaded.
	if(m_lpLinks)
	{
		m_lpLinks->Unload();
		SafeDelete(m_lpLinks);
	}

	SafeRelease(m_pFactory);

	m_lpParent->Release();
}

HRESULT CNeuralDocument::Initialize (VOID)
{
	HRESULT hr = S_FALSE;

	if(NULL == m_lpLinks)
	{
		m_lpLinks = __new CNeuralLinks;
		if(m_lpLinks)
			hr = m_lpLinks->Initialize();
		else
			hr = E_OUTOFMEMORY;
	}

	if(hr == S_OK && m_pFactory == NULL)
	{
		m_pFactory = __new CNeuralFactory;
		if(NULL == m_pFactory)
			hr = E_OUTOFMEMORY;
	}

	if(hr == S_OK && m_lpNet == NULL)
	{
		m_lpNet = __new CNeuralNet(m_pFactory);
		if(NULL == m_lpNet)
			hr = E_OUTOFMEMORY;
	}

	if(hr == S_OK && m_lpGraph == NULL)
	{
		m_pGraphAdapter = __new CGraphCtrlAdapter;
		if(m_pGraphAdapter)
		{
			hr = m_pGraphAdapter->Initialize();
			if(SUCCEEDED(hr))
			{
				m_lpGraph = m_pGraphAdapter->GetCtrl();

				m_lpGraph->SetGraphType(GRAPH_XY);
				m_lpGraph->SetBGColor(RGB(255,255,255));
				m_lpGraph->SetGraphTarget(this);
			}
		}
		else
			hr = E_OUTOFMEMORY;
	}

	if(hr == S_OK)
	{
		m_lpRecentChips = __new CRecentChipsMenu;
		if(m_lpRecentChips)
			hr = m_lpRecentChips->Initialize(RECENT_CHIPS_RANGE, "Software\\Simbey\\NeuralNet\\RecentChips");
		else
			hr = E_OUTOFMEMORY;
	}

	if(hr == S_OK)
	{
		GetProperty("WatchValues",&m_fDrawLabels,sizeof(BOOL));

		m_lpSelectedNet = m_lpNet;
		m_lpSelectedNet->AddRef();
	}

	return hr;
}

// IUnknown

HRESULT WINAPI CNeuralDocument::QueryInterface (REFIID iid, LPVOID* lplpvObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpvObject)
	{
		if(iid == IID_IOleCommandTarget)
			*lplpvObject = (IOleCommandTarget*)this;
		else if(iid == IID_IBaseContainer)
			*lplpvObject = (IBaseContainer*)this;
		else if(iid == IID_INeuralDocument)
			*lplpvObject = (INeuralDocument*)this;
		else if(iid == IID_IUnknown)
			*lplpvObject = (IUnknown*)(IOleCommandTarget*)this;
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

ULONG WINAPI CNeuralDocument::AddRef (VOID)
{
	return (ULONG)InterlockedIncrement((LONG*)&m_cRef);
}

ULONG WINAPI CNeuralDocument::Release (VOID)
{
	ULONG c = (ULONG)InterlockedDecrement((LONG*)&m_cRef);
	if(c == 0)
		__delete this;
	return c;
}

// IOleCommandTarget

HRESULT WINAPI CNeuralDocument::QueryStatus (const GUID* lpguidCmdGroup, ULONG cCmds, OLECMD* lprgCmds, OLECMDTEXT* lpCmdText)
{
	BOOL fTopNet = (m_lpNet == m_lpSelectedNet);

	if(m_lpContextMenuTarget)
		m_lpContextMenuTarget->QueryStatus(lpguidCmdGroup,cCmds,lprgCmds,lpCmdText);
	else
		m_lpRecentChips->QueryStatus(lpguidCmdGroup,cCmds,lprgCmds,lpCmdText);

	for(ULONG i = 0; i < cCmds; i++)
	{
		if(lprgCmds[i].cmdf == 0)
		{
			switch(lprgCmds[i].cmdID)
			{
			case ID_EDIT_DELETE:
				lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
				if(m_lpSelection && (fTopNet || SelectionContainsIO() == FALSE))
					lprgCmds[i].cmdf |= OLECMDF_ENABLED;
				break;
			case ID_INSERT_NEURONE:
			case ID_INSERT_INPUTPAD:
			case ID_INSERT_SPLITTER:
			case ID_INSERT_AND_GATE:
			case ID_INSERT_OR_GATE:
			case ID_INSERT_NOT_GATE:
			case ID_INSERT_XOR_GATE:
			case ID_INSERT_PARITY_GENERATOR:
			case ID_INSERT_BIAS:
			case ID_INSERT_SIGMOID:
				lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				if(lprgCmds[i].cmdID == m_cmdDoubleClick)
					lprgCmds[i].cmdf |= OLECMDF_LATCHED;
				break;
			case ID_INSERT_GROUP:
			case ID_INSERT_SIMPLEFRAME:
			case ID_INSERT_CHIP:
			case ID_EDIT_LINKS:
				lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				break;
			case ID_INSERT_INPUT:
			case ID_INSERT_OUTPUT:
				if(fTopNet)
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				else
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
				if(lprgCmds[i].cmdID == m_cmdDoubleClick)
					lprgCmds[i].cmdf |= OLECMDF_LATCHED;
				break;
			case ID_NET_RUN:
				lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				if(m_fRunning)
					lprgCmds[i].cmdf |= OLECMDF_LATCHED;
				break;
			case ID_NET_STEP:
				if(m_fRunning)
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
				else
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				break;
			case ID_NET_WATCHVALUES:
				lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				if(m_fDrawLabels)
					lprgCmds[i].cmdf |= OLECMDF_LATCHED;
				break;
			case ID_NET_OPEN:
				if(FindOneSelectedChip(NULL))
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				else
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
				break;
			case ID_NET_BACK:
				if(m_lpNetNav)
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				else
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
				break;
			case ID_NEURONE_PROPERTIES:
				if(m_lpContextMenuTarget)
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				else
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
				break;
			case ID_EDIT_PROPERTIES:
				if(NULL == m_lpContextMenuTarget && m_lpSelection && NULL == m_lpSelection->Next)
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				else
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
				break;
			case ID_EDIT_ROTATE_AXON:
			case ID_EDIT_FIRE:
				if(m_lpSelection)
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED | OLECMDF_ENABLED;
				else
					lprgCmds[i].cmdf = OLECMDF_SUPPORTED;
				break;
			default:
				ExtQueryMenuCommand(lprgCmds + i);
				break;
			}
		}
	}

	return S_OK;
}

HRESULT WINAPI CNeuralDocument::Exec (const GUID* lpguidCmdGroup, DWORD nCmdID, DWORD nCmdExecOpt, VARIANTARG* lpvaIn, VARIANTARG* lpvaOut)
{
	HRESULT hr;

	if(m_lpContextMenuTarget)
	{
		IBaseContainer* lpBaseContainerRef = NULL;
		VARIANTARG vBaseContainer;
		if(NULL == lpvaIn || VT_EMPTY == lpvaIn->vt)
		{
			lpBaseContainerRef = this;
			lpBaseContainerRef->AddRef();
			vBaseContainer.vt = VT_UNKNOWN;
			vBaseContainer.punkVal = lpBaseContainerRef;
			lpvaIn = &vBaseContainer;
		}
		hr = m_lpContextMenuTarget->Exec(lpguidCmdGroup,nCmdID,nCmdExecOpt,lpvaIn,lpvaOut);
		if(lpBaseContainerRef)
			lpBaseContainerRef->Release();
	}
	else
		hr = m_lpRecentChips->Exec(lpguidCmdGroup,nCmdID,nCmdExecOpt,lpvaIn,lpvaOut);

	if(OLECMDERR_E_NOTSUPPORTED == hr)
	{
		switch(nCmdID)
		{
		case ID_EDIT_DELETE:
			hr = DeleteSelection();
			break;

		case ID_INSERT_NEURONE:
		case ID_INSERT_INPUT:
		case ID_INSERT_OUTPUT:
		case ID_INSERT_CHIP:
		case ID_INSERT_AND_GATE:
		case ID_INSERT_OR_GATE:
		case ID_INSERT_NOT_GATE:
		case ID_INSERT_XOR_GATE:
		case ID_INSERT_PARITY_GENERATOR:
		case ID_INSERT_INPUTPAD:
		case ID_INSERT_SPLITTER:
		case ID_INSERT_SIMPLEFRAME:
		case ID_INSERT_BIAS:
		case ID_INSERT_SIGMOID:
			hr = InsertObject(nCmdID);
			break;

		case ID_NET_RUN:
			m_fRunning = !m_fRunning;
			if(m_fRunning)
			{
				hr = m_lpContainer->SetTimer(this,m_uTimerSpeed,&m_nTimerIndex);
				if(FAILED(hr))
					m_fRunning = FALSE;
			}
			else
			{
				hr = m_lpContainer->KillTimer(m_nTimerIndex);
				if(SUCCEEDED(hr))
					m_nTimerIndex = 0;
				else
					m_fRunning = TRUE;
			}
			break;

		case ID_NET_STEP:
			if(!m_fRunning)
			{
				PerformNetCycle();
				hr = S_OK;
			}
			break;

		case ID_NET_WATCHVALUES:
			m_fDrawLabels = !m_fDrawLabels;
			SetProperty("WatchValues",&m_fDrawLabels,sizeof(BOOL));
			m_lpContainer->InvalidateContainer();
			hr = S_OK;
			break;

		case ID_NET_BACK:
			if(m_lpNetNav)
			{
				INeuralNet* lpNet = m_lpNetNav->lpNet;
				LPSELECT_LIST lpSelection = m_lpNetNav->lpSelection;
				LPNETNAVLIST lpNext = m_lpNetNav->Next;

				__delete m_lpNetNav;
				m_lpNetNav = lpNext;

				ZoomOutOfNet(lpNet,12);

				ClearSelection();
				m_lpSelection = lpSelection;

				lpNet->Release();
				hr = S_OK;
			}
			else
				hr = S_FALSE;
			break;

		case ID_NET_OPEN:
			{
				INeuralChip* lpChip;
				if(FindOneSelectedChip(&lpChip))
				{
					INT x, y, xClient, yClient;
					lpChip->GetPosition(x,y);
					m_lpGraph->GraphToClient((FLOAT)x,(FLOAT)y,xClient,yClient);
					ZoomIntoNet(lpChip->GetEmbeddedNet(),xClient,yClient,20);
					lpChip->Release();
				}
			}
			break;

		case IDM_OPEN:
			if(lpvaIn && lpvaIn->vt == (VT_BYREF | VT_I1))
			{
				PSTR pszFile = lpvaIn->pcVal, pszDup;
				INT cchFile;

				hr = TDuplicateStringChecked(pszFile, &pszDup, &cchFile);
				if(SUCCEEDED(hr))
				{
					BOOL fUseBiasNeuronesForBrainBox = FALSE;

					if(3 < cchFile && 0 == lstrcmpi(pszFile + cchFile - 4, ".bbx") && lpguidCmdGroup)
					{
						HWND hwnd;
						INT nResult;

						CopyMemory(&hwnd, lpguidCmdGroup, sizeof(hwnd));
						nResult = MessageBox(hwnd, "Do you want to use bias neurones for converting from the BrainBox format?", "Use Bias Neurones?", MB_YESNOCANCEL);
						if(IDCANCEL == nResult)
							hr = E_ABORT;
						else if(IDYES == nResult)
							fUseBiasNeuronesForBrainBox = TRUE;
					}

					if(SUCCEEDED(hr))
						hr = CNeuralNet::LoadFromFile(m_lpNet, m_lpLinks, pszFile, cchFile, fUseBiasNeuronesForBrainBox);
					if(SUCCEEDED(hr))
					{
						__delete_array m_lpszFile;
						m_lpszFile = pszDup;
					}
					else
						__delete_array pszDup;
				}
			}
			else
				hr = E_INVALIDARG;
			break;

		case IDM_SAVE:
			if(lpvaIn && lpvaIn->vt == (VT_BYREF | VT_I1))
			{
				LPSTR lpszFile = lpvaIn->pcVal;
				INT cchFile = lstrlen(lpszFile);
				LPSTR lpszDup = __new CHAR[cchFile + 1];
				if(lpszDup)
				{
					CopyMemory(lpszDup,lpszFile,cchFile + 1);
					hr = SaveToFile(lpszDup);
					if(SUCCEEDED(hr))
					{
						__delete_array m_lpszFile;
						m_lpszFile = lpszDup;
					}
					else
						__delete_array lpszDup;
				}
				else
					hr = E_OUTOFMEMORY;
			}
			else if(m_lpszFile)
				hr = SaveToFile(m_lpszFile);
			else
				hr = E_FAIL;

			if(SUCCEEDED(hr))
				m_fChangesMade = FALSE;

			break;

		case ID_NEURONE_PROPERTIES:
			if(m_lpContextMenuTarget)
			{
				INeurone* lpNeurone;
				if(SUCCEEDED(m_lpContextMenuTarget->QueryInterface(&lpNeurone)))
				{
					HWND hwnd;
					if(SUCCEEDED(m_lpContainer->GetHwnd(&hwnd)))
					{
						HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hwnd,GWL_HINSTANCE);
						CNeuronePropsDlg Edit(m_lpLinks, lpNeurone);
						CDialogHost Dialog(hInstance);
						if(SUCCEEDED(Dialog.Display(hwnd,&Edit)))
						{
							if(Dialog.GetReturnValue())
								SetChangesMade();
						}
					}
					lpNeurone->Release();
				}
			}
			break;

		case ID_EDIT_LINKS:
			{
				HWND hwnd;
				if(SUCCEEDED(m_lpContainer->GetHwnd(&hwnd)))
				{
					HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hwnd,GWL_HINSTANCE);
					CEditLinksDlg Edit(m_lpLinks, NULL);
					CDialogHost Dialog(hInstance);
					Dialog.Display(hwnd, &Edit);
				}
			}
			break;

		case ID_EDIT_PROPERTIES:
			Assert(NULL == m_lpContextMenuTarget);
			hr = m_lpSelection->lpObject->QueryInterface(&m_lpContextMenuTarget);
			if(SUCCEEDED(hr))
			{
				hr = Exec(NULL, ID_NEURONE_PROPERTIES, 0, NULL, NULL);
				SafeRelease(m_lpContextMenuTarget);
			}
			break;

		case ID_EDIT_ROTATE_AXON:
			hr = SendCommandToSelection(ID_NEURONE_ROTATE_AXON, 0);
			break;

		case ID_EDIT_FIRE:
			hr = SendCommandToSelection(ID_NEURONE_FIRE, 0);
			break;

		default:
			{
				CHAR szItem[_MAX_PATH + 1];
				INT cchItem;

				if(SUCCEEDED(m_lpRecentChips->ExecRecentItem(nCmdID, szItem, ARRAYSIZE(szItem), &cchItem)))
				{
					INetDocObject* lpChip;
					hr = LoadChipFromFile(szItem, cchItem, &lpChip);
					if(SUCCEEDED(hr))
					{
						hr = InsertObject(lpChip);
						lpChip->Release();
					}
				}
				else
					ExtExecMenuCommand(nCmdID, nCmdExecOpt);
				break;
			}
		}
	}

	if(SUCCEEDED(hr) && m_lpContainer)
		m_lpContainer->InvalidateContainer();

	return hr;
}

// IWindowless

VOID WINAPI CNeuralDocument::AttachContainer (IBaseContainer* lpContainer)
{
	// Destroy accessibility before attaching a __new container or clearing the container.
	if(m_lpAccessible)
	{
		Assert(m_lpContainer);	// Accessibility shouldn't have ever existed without a container.

		m_lpAccessible->ReleaseDocument();
		m_lpAccessible->AttachNet(NULL);
		m_lpAccessible->Release();
		m_lpAccessible = NULL;
	}

	if(m_lpContainer)
	{
		if(m_nTimerIndex > 0)
		{
			SideAssertHr(m_lpContainer->KillTimer(m_nTimerIndex));
			m_nTimerIndex = 0;
		}

		m_lpContainer->Release();
	}

	m_lpContainer = lpContainer;

	if(m_lpContainer)
		m_lpContainer->AddRef();

	m_pGraphAdapter->AttachContainer((m_lpContainer) ? this : NULL);

	SafeRelease(m_lpCapture);

	ExtSetDocument((m_lpContainer) ? this : NULL);
}

VOID WINAPI CNeuralDocument::Paint (HDC hdc)
{
	m_lpGraph->Paint(hdc);
}

VOID WINAPI CNeuralDocument::Move (LPRECT lpPosition)
{
	m_lpGraph->Move(lpPosition);
}

VOID WINAPI CNeuralDocument::SizeObject (INT nWidth, INT nHeight)
{
	m_lpGraph->SizeObject(nWidth,nHeight);
}

BOOL WINAPI CNeuralDocument::OnMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult)
{
	BOOL fHandled = FALSE;

	if(msg == WM_TIMER && wParam == (WPARAM)m_nTimerIndex)
	{
		PerformNetCycle();
		fHandled = TRUE;
	}
	else if(WM_APPCOMMAND == msg)
	{
		switch(GET_APPCOMMAND_LPARAM(lParam))
		{
		case APPCOMMAND_MEDIA_PLAY_PAUSE:
			fHandled = SUCCEEDED(Exec(NULL, ID_NET_RUN, 0, NULL, NULL));
			break;
		case APPCOMMAND_MEDIA_PAUSE:
		case APPCOMMAND_MEDIA_STOP:
			if(m_fRunning)
				fHandled = SUCCEEDED(Exec(NULL, ID_NET_RUN, 0, NULL, NULL));
			break;
		case APPCOMMAND_MEDIA_PLAY:
			if(!m_fRunning)
				fHandled = SUCCEEDED(Exec(NULL, ID_NET_RUN, 0, NULL, NULL));
			break;
		}
	}
	else
	{
		fHandled = m_lpGraph->OnMessage(msg, wParam, lParam, lResult);
		if(!fHandled)
			fHandled = ExtCustomMessage(msg, wParam, lParam, lResult);
	}

	return fHandled;
}

HRESULT WINAPI CNeuralDocument::GetAccObject (IAccessible** lplpAccessible)
{
	return m_lpGraph->GetAccObject(lplpAccessible);
}

// IBaseContainer

HRESULT WINAPI CNeuralDocument::GetHwnd (HWND* lphwnd)
{
	HRESULT hr;

	CheckIf(NULL == m_lpContainer, E_FAIL);
	Check(m_lpContainer->GetHwnd(lphwnd));

Cleanup:
	return hr;
}

HRESULT WINAPI CNeuralDocument::GetCapture (IWindowless** lplpObject)
{
	return m_lpContainer->GetCapture(lplpObject);
}

HRESULT WINAPI CNeuralDocument::SetCapture (IWindowless* lpObject, BOOL fCapture)
{
	UNREFERENCED_PARAMETER(lpObject);

	return m_lpContainer->SetCapture(this,fCapture);
}

HRESULT WINAPI CNeuralDocument::GetFocus (IWindowless** lplpObject)
{
	return m_lpContainer->GetFocus(lplpObject);
}

HRESULT WINAPI CNeuralDocument::SetFocus (IWindowless* lpObject)
{
	UNREFERENCED_PARAMETER(lpObject);

	return m_lpContainer->SetFocus(this);
}

HRESULT WINAPI CNeuralDocument::SetTimer (IWindowless* lpObject, UINT uElapse, INT* lpnTimer)
{
	return m_lpContainer->SetTimer(lpObject,uElapse,lpnTimer);
}

HRESULT WINAPI CNeuralDocument::KillTimer (INT nTimer)
{
	return m_lpContainer->KillTimer(nTimer);
}

HRESULT WINAPI CNeuralDocument::GetDC (IWindowless* lpObject, HDC* lphdc)
{
	UNREFERENCED_PARAMETER(lpObject);

	return m_lpContainer->GetDC(this,lphdc);
}

HRESULT WINAPI CNeuralDocument::ReleaseDC (IWindowless* lpObject, HDC hdc)
{
	UNREFERENCED_PARAMETER(lpObject);

	return m_lpContainer->ReleaseDC(this,hdc);
}

HRESULT WINAPI CNeuralDocument::InvalidateContainer (VOID)
{
	return m_lpContainer->InvalidateContainer();
}

HRESULT WINAPI CNeuralDocument::OnWindowlessMessage (IWindowless* lpObject, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* lplResult)
{
	UNREFERENCED_PARAMETER(lpObject);

	return m_lpContainer->OnWindowlessMessage(this,msg,wParam,lParam,lplResult);
}

HRESULT WINAPI CNeuralDocument::ScreenToWindowless (IWindowless* lpObject, LPPOINT lpPoint)
{
	UNREFERENCED_PARAMETER(lpObject);

	return m_lpContainer->ScreenToWindowless(this,lpPoint);
}

HRESULT WINAPI CNeuralDocument::TrackPopupMenu (HMENU hMenu, INT x, INT y, IOleCommandTarget* lpTarget)
{
	HRESULT hr;

	if(hMenu && lpTarget)
	{
		Assert(NULL == m_lpContextMenuTarget);
		m_lpContextMenuTarget = lpTarget;
		m_lpContextMenuTarget->AddRef();

		hr = MenuUtil::EnableMenuItems(NULL,hMenu,this);

		if(SUCCEEDED(hr))
		{
			INetDocObject* lpNetObject;
			HMENU hDeleteMenu = NULL;

			Assert(m_lpContainer);

			if(SUCCEEDED(lpTarget->QueryInterface(IID_INetDocObject, (LPVOID*)&lpNetObject)))
			{
				hr = MenuUtil::DupMenu(hMenu, &hDeleteMenu);
				if(SUCCEEDED(hr))
				{
					hMenu = hDeleteMenu;
				}
				lpNetObject->Release();
			}

			hr = m_lpContainer->TrackPopupMenu(hMenu,x,y,this);

			if(hDeleteMenu)
				DestroyMenu(hDeleteMenu);
		}

		m_lpContextMenuTarget->Release();
		m_lpContextMenuTarget = NULL;
	}
	else
		hr = E_INVALIDARG;

	return hr;
}

// IGraphClient

VOID CNeuralDocument::onGraphPaint (IGrapher* lpGraph)
{
	HPEN hpnDef = lpGraph->SelectPen(GdiList::hpnConnection);

	if(MOUSE_CONNECT == m_iDragLevel)
	{
		INT xPin, yPin;
		LPSELECT_LIST lpList = m_lpSelection;
		while(lpList)
		{
			if(lpList->lpObject->GetDragSourcePoint(m_iPinSource,xPin,yPin))
			{
				lpGraph->MoveTo((FLOAT)xPin,(FLOAT)yPin,0.0f);
				lpGraph->LineTo((FLOAT)m_xDragPos,(FLOAT)m_yDragPos,0.0f);
			}
			lpList = lpList->Next;
		}
	}

	m_lpSelectedNet->Draw(lpGraph,m_fDrawLabels);

	if(MOUSE_DRAW_SELECTION == m_iDragLevel)
	{
		LPBYTE lpBuffer;
		INT nWidth, nHeight;
		LONG lPitch;
		if(lpGraph->GetRawBuffer(lpBuffer,nWidth,nHeight,lPitch))
		{
			RECT rc;
			HPEN hDefPen = lpGraph->SelectPen(GdiList::hpnConnection);
			HBRUSH hNullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
			HBRUSH hDefBrush = lpGraph->SelectBrush(hNullBrush);
			lpGraph->Rectangle((FLOAT)m_xDown,(FLOAT)m_yDown,0,(FLOAT)m_xDragPos,(FLOAT)m_yDragPos,0);

			GetSelectionRect(lpGraph,&rc);
			DIBDrawing::AlphaFill24(lpBuffer,nWidth,nHeight,lPitch,&rc,RGB(64,128,255),64);

			lpGraph->SelectBrush(hDefBrush);
			lpGraph->SelectPen(hDefPen);
		}
	}

	lpGraph->SelectPen(hpnDef);
}

VOID CNeuralDocument::onGraphMouseMove (DWORD dwKeys, FLOAT x, FLOAT y)
{
	UNREFERENCED_PARAMETER(dwKeys);

	if(NULL == m_lpCapture || FALSE == m_lpCapture->onGraphMouseMove(dwKeys,x,y))
	{
		if(0 == m_iDragLevel)
		{
			INetDocObject* lpObject = m_lpSelectedNet->FindObject((INT)x,(INT)y);
			if(lpObject)
			{
				INT nHitTest = lpObject->HitTest((INT)x,(INT)y);
				INT nSubType = HITTEST_SUBTYPE(nHitTest);
				if(HITTEST_INTERACTIVE == nSubType)
				{
					IGraphInputCapture* lpCapture;
					if(SUCCEEDED(lpObject->GetGraphInputCaptureObject(this,&lpCapture)))
					{
						SafeRelease(m_lpCapture);
						m_lpCapture = lpCapture;

						m_lpCapture->onGraphMouseMove(dwKeys,x,y);
					}
				}
				else
					SetResizeCursor(nSubType);
			}
		}
		else if(m_iDragLevel == MOUSE_SELECT || m_iDragLevel == MOUSE_DRAG)
		{
			INT xDrag = (INT)m_lpGraph->GetGridSnap(x);
			INT yDrag = (INT)m_lpGraph->GetGridSnap(y);
			if(xDrag != m_xDragPos || yDrag != m_yDragPos)
			{
				INT xDelta = xDrag - m_xDragPos;
				INT yDelta = yDrag - m_yDragPos;
				LPSELECT_LIST lpList = m_lpSelection;

				if(m_iDragLevel == MOUSE_SELECT)
				{
					m_iDragLevel = MOUSE_DRAG;
					SetFrameHighlighting(TRUE);
				}

				while(lpList)
				{
					lpList->lpObject->MoveObject(xDelta,yDelta);
					CheckObjectFrame(lpList->lpObject, TRUE, FALSE);
					lpList = lpList->Next;
				}

				m_xDragPos = xDrag;
				m_yDragPos = yDrag;

				SetChangesMade();
			}
		}
		else if(m_iDragLevel == MOUSE_CONNECT)
		{
			INetDocObject* lpHover;

			m_xDragPos = (INT)x;
			m_yDragPos = (INT)y;
			lpHover = m_lpSelectedNet->FindObject(m_xDragPos,m_yDragPos);

			if(m_lpToggle)
			{
				m_lpToggle->HighlightPin(m_iPinHovered,FALSE);
				m_lpToggle->Release();
				m_lpToggle = NULL;
				m_iPinHovered = 0xFFFFFFFF;
			}

			if(lpHover && !InSelection(lpHover))
			{
				ULONG iPin = lpHover->GetInputPin(m_xDragPos,m_yDragPos);
				if(iPin != 0xFFFFFFFF)
				{
					m_lpToggle = lpHover;
					m_lpToggle->AddRef();
					m_lpToggle->HighlightPin(iPin,TRUE);
					m_iPinHovered = iPin;
				}
			}

			m_lpContainer->InvalidateContainer();
		}
		else if(m_iDragLevel == MOUSE_RESIZE_V || m_iDragLevel == MOUSE_RESIZE_H || m_iDragLevel == MOUSE_RESIZE_C1 || m_iDragLevel == MOUSE_RESIZE_C2)
		{
			INT xDrag = (INT)m_lpGraph->GetGridSnap(x);
			INT yDrag = (INT)m_lpGraph->GetGridSnap(y);
			INT nHitTest = HITTEST_NONE;

			switch(m_iDragLevel)
			{
			case MOUSE_RESIZE_V:
				nHitTest = HITTEST_RESIZE_V;
				break;
			case MOUSE_RESIZE_H:
				nHitTest = HITTEST_RESIZE_H;
				break;
			case MOUSE_RESIZE_C1:
				nHitTest = HITTEST_RESIZE_C1;
				break;
			case MOUSE_RESIZE_C2:
				nHitTest = HITTEST_RESIZE_C2;
				break;
			}

			if(xDrag != m_xDragPos || yDrag != m_yDragPos)
			{
				INT xDelta = xDrag - m_xDragPos;
				INT yDelta = yDrag - m_yDragPos;

				m_lpToggle->ResizeObject(nHitTest,m_xDragPos,m_yDragPos,xDelta,yDelta);
				m_xDragPos = xDrag;
				m_yDragPos = yDrag;

				m_lpContainer->InvalidateContainer();
			}

			SetResizeCursor(nHitTest);
		}
		else if(m_iDragLevel == MOUSE_PRE_DRAW_SELECTION)
		{
			FLOAT fScale = m_lpGraph->GetScale();
			FLOAT xDiff, yDiff;

			xDiff = (x - (FLOAT)m_xDown) * fScale;
			yDiff = (y - (FLOAT)m_yDown) * fScale;
			if(abs(xDiff) >= 4 || abs(yDiff) >= 4)
			{
				m_iDragLevel = MOUSE_DRAW_SELECTION;
				onGraphMouseMove(dwKeys,x,y);
			}
		}
		else if(m_iDragLevel == MOUSE_DRAW_SELECTION)
		{
			RECT rc;
			INetDocObject** lplpObjects;
			INT cObjects;
			FLOAT xLeft, yTop, xRight, yBottom;

			m_xDragPos = (INT)x;
			m_yDragPos = (INT)y;

			GetSelectionRect(m_lpGraph,&rc);
			m_lpGraph->ClientToGraph(rc.left,rc.top,xLeft,yTop);
			m_lpGraph->ClientToGraph(rc.right,rc.bottom,xRight,yBottom);

			if(SUCCEEDED(m_lpSelectedNet->GetObjectsInRange(xLeft,yTop,xRight,yBottom,&lplpObjects,&cObjects)))
			{
				for(INT i = 0; i < cObjects; i++)
				{
					AddToSelectionInternal(lplpObjects[i],SELECT_FROM_RANGE_ACTIVE);
					lplpObjects[i]->Release();
				}

				CullSelectionRange();
				m_lpContainer->InvalidateContainer();

				__delete_array lplpObjects;
			}

			m_lpContainer->InvalidateContainer();
		}
	}
}

VOID CNeuralDocument::onGraphLBtnDown (DWORD dwKeys, FLOAT x, FLOAT y)
{
	if(m_fInsertSelection)
		m_fInsertSelection = FALSE;
	else if(NULL == m_lpCapture || FALSE == m_lpCapture->onGraphLBtnDown(dwKeys,x,y))
	{
		INetDocObject* lpObject = m_lpSelectedNet->FindObject((INT)x,(INT)y);
		if(lpObject)
		{
			INT nHitTest = lpObject->HitTest((INT)x,(INT)y);
			INT nSubType = HITTEST_SUBTYPE(nHitTest);
			if(HITTEST_INTERACTIVE == nSubType)
			{
				IGraphInputCapture* lpCapture;
				if(SUCCEEDED(lpObject->GetGraphInputCaptureObject(this,&lpCapture)))
				{
					SafeRelease(m_lpCapture);
					m_lpCapture = lpCapture;

					m_lpCapture->onGraphLBtnDown(dwKeys,x,y);
				}
			}
			else if(nSubType == HITTEST_DRAG_SOURCE || nSubType == HITTEST_DRAG_SOURCE_TOGGLE)
			{
				if(lpObject->GetDragSourcePin((INT)x,(INT)y,m_iPinSource))
				{
					if(!InSelection(lpObject))
						ClearSelection();
					AddToSelection(lpObject);

					m_xDown = (INT)x;
					m_yDown = (INT)y;
					m_xDragPos = m_xDown;
					m_yDragPos = m_yDown;
					m_iPinHovered = 0xFFFFFFFF;
					m_iDragLevel = MOUSE_CONNECT;

					if(nSubType == HITTEST_DRAG_SOURCE_TOGGLE)
						m_fInteractToggle = TRUE;
					else
						m_fInteractToggle = FALSE;
				}
			}
			else if(nHitTest & HITTEST_SELECTABLE)
			{
				if((dwKeys & MK_CONTROL) == 0 && RemoveSelection(lpObject) == FALSE)
				{
					ClearSelection();
					AddToSelection(lpObject);
				}
				else if(AddToSelection(lpObject) == S_FALSE || (dwKeys & MK_CONTROL) == 0)
				{
					if(m_lpToggle)
						m_lpToggle->Release();
					m_lpToggle = lpObject;
					m_lpToggle->AddRef();
				}

				m_xDragPos = (INT)m_lpGraph->GetGridSnap(x);
				m_yDragPos = (INT)m_lpGraph->GetGridSnap(y);
				m_iDragLevel = MOUSE_SELECT;
			}
			else if(nHitTest & HITTEST_RESIZE)
			{
				switch(nHitTest)
				{
				case HITTEST_RESIZE_V:
					m_iDragLevel = MOUSE_RESIZE_V;
					break;
				case HITTEST_RESIZE_H:
					m_iDragLevel = MOUSE_RESIZE_H;
					break;
				case HITTEST_RESIZE_C1:
					m_iDragLevel = MOUSE_RESIZE_C1;
					break;
				case HITTEST_RESIZE_C2:
					m_iDragLevel = MOUSE_RESIZE_C2;
					break;
				}
				m_xDragPos = (INT)x;
				m_yDragPos = (INT)y;

				if(m_lpToggle)
					m_lpToggle->Release();
				m_lpToggle = lpObject;
				m_lpToggle->AddRef();

				SetResizeCursor(nHitTest);
			}
		}
		else
		{
			if((dwKeys & MK_CONTROL) == 0)
				ClearSelection();
			else
			{
				LPSELECT_LIST lpSelection = m_lpSelection;
				while(lpSelection)
				{
					lpSelection->dwFlags = SELECT_PRESELECTION;
					lpSelection = lpSelection->Next;
				}
			}

			m_xDown = (INT)x;
			m_yDown = (INT)y;
			m_iDragLevel = MOUSE_PRE_DRAW_SELECTION;
		}
	}
}

VOID CNeuralDocument::onGraphLBtnUp (DWORD dwKeys, FLOAT x, FLOAT y)
{
	if(NULL == m_lpCapture || FALSE == m_lpCapture->onGraphLBtnUp(dwKeys,x,y))
	{
		if(m_iDragLevel != MOUSE_NONE)
		{
			if(m_iDragLevel == MOUSE_SELECT)
			{
				if(dwKeys & MK_CONTROL)
				{
					if(m_lpToggle)
						RemoveSelection(m_lpToggle);
				}
				else if(m_lpToggle)
				{
					ClearSelection();
					AddToSelection(m_lpToggle);
				}
			}
			else if(m_iDragLevel == MOUSE_CONNECT)
			{
				FLOAT xDragDiff = x - m_xDown, yDragDiff = y - m_yDown;
				FLOAT fDragDistance = sqrtf(xDragDiff * xDragDiff + yDragDiff * yDragDiff);
				if(fDragDistance > 3.0f)
				{
					if(m_lpToggle)
					{
						LPSELECT_LIST lpList = m_lpSelection;
						INT c = 0;

						while(lpList)
						{
							if(S_OK == lpList->lpObject->ConnectTo(m_iPinSource, m_lpToggle, m_iPinHovered, 0.5f))
								c++;
							lpList = lpList->Next;
						}

						if(c > 0)
							SetChangesMade();
						else
						{
							HWND hwnd;
							if(SUCCEEDED(m_lpContainer->GetHwnd(&hwnd)))
								MessageBox(hwnd,"An existing connection may already exist, or the target neurone may not accept incoming connections.","Connection Refused",MB_OK);
						}
					}
				}
				else if(m_fInteractToggle)
				{
					INetDocObject* lpObject = m_lpSelectedNet->FindObject(m_xDown,m_yDown);
					if(lpObject)
					{
						lpObject->Click(m_xDown,m_yDown);
						SetChangesMade();
					}
				}
				m_lpContainer->InvalidateContainer();
			}
			else if(m_iDragLevel == MOUSE_DRAG)
			{
				SetFrameHighlighting(FALSE);
				m_lpContainer->InvalidateContainer();
			}
			else if(m_iDragLevel == MOUSE_RESIZE_V || m_iDragLevel == MOUSE_RESIZE_H || m_iDragLevel == MOUSE_RESIZE_C1 || m_iDragLevel == MOUSE_RESIZE_C2)
			{
				Assert(m_lpToggle);
				RegroupFrame(m_lpToggle);
			}
			else if(m_iDragLevel == MOUSE_DRAW_SELECTION)
				m_lpContainer->InvalidateContainer();

			if(m_lpToggle)
			{
				if(m_iPinHovered != 0xFFFFFFFF)
					m_lpToggle->HighlightPin(m_iPinHovered,FALSE);

				m_lpToggle->Release();
				m_lpToggle = NULL;
			}

			m_fInteractToggle = FALSE;
			m_fInsertSelection = FALSE;
			m_iDragLevel = MOUSE_NONE;
		}
	}
}

BOOL CNeuralDocument::onGraphRBtnDown (DWORD dwKeys, FLOAT x, FLOAT y)
{
	BOOL fHandled = FALSE;

	if(m_lpCapture)
		fHandled = m_lpCapture->onGraphRBtnDown(dwKeys,x,y);

	if(!fHandled && MOUSE_NONE == m_iDragLevel)
	{
		INetDocObject* lpObject = m_lpSelectedNet->FindObject((INT)x,(INT)y);
		if(lpObject)
		{
			INT nHitTest = lpObject->HitTest((INT)x,(INT)y);
			INT nSubType = HITTEST_SUBTYPE(nHitTest);
			if(nSubType == HITTEST_DRAG_SOURCE)
			{
				ULONG iPin;
				if(lpObject->GetDragSourcePin((INT)x,(INT)y,iPin))
				{
					HWND hwnd;
					if(SUCCEEDED(m_lpContainer->GetHwnd(&hwnd)))
					{
						HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hwnd,GWL_HINSTANCE);
						CConnectionsDlg Edit(hwnd,lpObject,iPin);
						CDialogHost Dialog(hInstance);
						if(SUCCEEDED(Dialog.Display(hwnd,&Edit)))
						{
							if(Dialog.GetReturnValue())
								SetChangesMade();
						}
						fHandled = TRUE;
					}
				}
			}
			else if(nHitTest & HITTEST_CONTEXT)
			{
				INT xClient, yClient;
				m_lpGraph->GraphToClient(x,y,xClient,yClient);
				lpObject->ContextMenu(this,xClient,yClient);
				fHandled = TRUE;
			}
		}
	}
	return fHandled;
}

VOID CNeuralDocument::onGraphRBtnUp (DWORD dwKeys, FLOAT x, FLOAT y)
{
	if(m_lpCapture)
		m_lpCapture->onGraphRBtnUp(dwKeys,x,y);
	else if(MOUSE_NONE == m_iDragLevel && NULL == m_lpSelectedNet->FindObject((INT)x,(INT)y))
	{
		HWND hwnd;
		if(SUCCEEDED(m_lpContainer->GetHwnd(&hwnd)))
		{
			HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hwnd,GWL_HINSTANCE);
			HMENU hInsert = MenuUtil::LoadSubPopupMenu(hInstance, IDR_MENU, "&Insert");
			if(hInsert)
			{
				POINT pt;
				if(GetCursorPos(&pt) && ScreenToClient(hwnd, &pt))
					m_lpContainer->TrackPopupMenu(hInsert,pt.x,pt.y,this);

				DestroyMenu(hInsert);
			}
		}
	}
}

VOID CNeuralDocument::onGraphLBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y)
{
	if(NULL == m_lpCapture || FALSE == m_lpCapture->onGraphLBtnDbl(dwKeys,x,y))
	{
		INetDocObject* lpObject = m_lpSelectedNet->FindObject((INT)x,(INT)y);
		if(lpObject)
		{
			INeuralChip* lpChip;
			if(SUCCEEDED(lpObject->QueryInterface(&lpChip)))
			{
				INT xClient, yClient;

				m_lpGraph->GraphToClient(x,y,xClient,yClient);
				ZoomIntoNet(lpChip->GetEmbeddedNet(),xClient,yClient,20);

				lpChip->Release();
			}
		}
		else
		{
			INetDocObject* pObject;
			if(SUCCEEDED(CreateUsingFactory(m_cmdDoubleClick, &pObject)))
			{
				INT ix = (INT)m_lpGraph->GetGridSnap(x);
				INT iy = (INT)m_lpGraph->GetGridSnap(y);
				pObject->MoveObject(ix,iy);
				if(SUCCEEDED(m_lpSelectedNet->AttachObject(pObject)))
				{
					if(m_lpAccessible)
						m_lpAccessible->NotifyAccEvent(EVENT_OBJECT_REORDER,NULL);
				}
				pObject->Release();

				SetChangesMade();
			}
		}
	}
}

VOID CNeuralDocument::onGraphRBtnDbl (DWORD dwKeys, FLOAT x, FLOAT y)
{
	if(m_lpCapture)
		m_lpCapture->onGraphRBtnDbl(dwKeys,x,y);
}

VOID CNeuralDocument::onGraphViewChanged (BOOL fZoomChanged)
{
	if(fZoomChanged)
	{
		FLOAT fScale = m_lpGraph->GetScale();
		GdiList::ScalePens(fScale);
		GdiList::ScaleFonts(fScale);
	}
}

BOOL CNeuralDocument::onGraphKeyDown (WPARAM iKey)
{
	BOOL fHandled = FALSE;

	if(m_lpCapture)
		fHandled = m_lpCapture->onGraphKeyDown(iKey);

	if(FALSE == fHandled)
	{
		OLECMD Cmd = {0};

		switch(iKey)
		{
		case VK_DELETE:
			Cmd.cmdID = ID_EDIT_DELETE;
			break;
		case VK_F5:
			Cmd.cmdID = ID_NET_RUN;
			break;
		case VK_F8:
			Cmd.cmdID = ID_NET_STEP;
			break;
		case VK_BACK:
			Cmd.cmdID = ID_NET_BACK;
			break;
		case VK_RETURN:
			Cmd.cmdID = ID_NET_OPEN;
			break;
		}

		if(SUCCEEDED(QueryStatus(NULL,1,&Cmd,NULL)) && (Cmd.cmdf & OLECMDF_ENABLED))
			fHandled = SUCCEEDED(Exec(NULL,Cmd.cmdID,0,NULL,NULL));
	}

	return fHandled;
}

BOOL CNeuralDocument::onGraphKeyUp (WPARAM iKey)
{
	return (m_lpCapture && m_lpCapture->onGraphKeyUp(iKey));
}

BOOL CNeuralDocument::onGraphChar (WPARAM iKey)
{
	return (m_lpCapture && m_lpCapture->onGraphChar(iKey));
}

BOOL CNeuralDocument::onGraphWheel (SHORT sDistance, FLOAT x, FLOAT y)
{
	return (m_lpCapture && m_lpCapture->onGraphWheel(sDistance,x,y));
}

HRESULT CNeuralDocument::onGraphGetAcc (IAccessible** lplpAccessible)
{
	HRESULT hr = E_INVALIDARG;
	if(lplpAccessible)
	{
		if(m_lpAccessible == NULL)
		{
			m_lpAccessible = __new CAccessibleNetDoc(this, m_pGraphAdapter);
			if(m_lpAccessible)
			{
				hr = m_lpAccessible->Initialize();
				if(SUCCEEDED(hr))
					m_lpAccessible->AttachNet(m_lpSelectedNet);
				else
				{
					m_lpAccessible->Release();
					m_lpAccessible = NULL;
				}
			}
			else
				hr = E_OUTOFMEMORY;
		}

		if(m_lpAccessible)
		{
			*lplpAccessible = m_lpAccessible;
			(*lplpAccessible)->AddRef();
			hr = S_OK;
		}
	}
	return hr;
}

// INeuralDocument

VOID CNeuralDocument::SetChangesMade (VOID)
{
	m_fChangesMade = TRUE;
	m_lpContainer->InvalidateContainer();
}

BOOL CNeuralDocument::IsSavedOnDisk (VOID)
{
	return (m_lpszFile != NULL) ? TRUE : FALSE;
}

BOOL CNeuralDocument::ChangesMade (VOID)
{
	return m_fChangesMade;
}

VOID CNeuralDocument::ClearSelection (VOID)
{
	if(m_lpSelection)
	{
		ClearSelection(m_lpSelection);
		m_lpSelection = NULL;

		if(m_lpContainer)
			m_lpContainer->InvalidateContainer();
	}
}

HRESULT CNeuralDocument::AddToSelection (INetDocObject* lpObject)
{
	HRESULT hr = E_INVALIDARG;
	if(lpObject)
	{
		hr = AddToSelectionInternal(lpObject, 0);
		if(SUCCEEDED(hr))
			m_lpContainer->InvalidateContainer();
	}
	return hr;
}

BOOL CNeuralDocument::RemoveSelection (INetDocObject* lpObject)
{
	BOOL fSuccess = FALSE;
	if(m_lpSelection)
	{
		LPSELECT_LIST lpList = m_lpSelection, lpPrev = NULL;
		while(lpList)
		{
			if(lpList->lpObject == lpObject)
			{
				lpList->lpObject->SelectObject(FALSE);
				lpList->lpObject->Release();
				if(lpPrev)
					lpPrev->Next = lpList->Next;
				else
					m_lpSelection = lpList->Next;
				__delete lpList;
				m_lpContainer->InvalidateContainer();
				fSuccess = TRUE;
				break;
			}
			lpPrev = lpList;
			lpList = lpList->Next;
		}
	}
	return fSuccess;
}

BOOL CNeuralDocument::InSelection (INetDocObject* lpObject)
{
	BOOL fInSelection = FALSE;
	LPSELECT_LIST lpList = m_lpSelection;
	while(lpList)
	{
		if(lpList->lpObject == lpObject)
		{
			fInSelection = TRUE;
			break;
		}
		lpList = lpList->Next;
	}
	return fInSelection;
}

VOID CNeuralDocument::OnModifyMenu (HMENU hMenu)
{
	HWND hwnd;

	if(SUCCEEDED(m_lpContainer->GetHwnd(&hwnd)))
	{
		HINSTANCE hInstance = (HINSTANCE)GetWindowLong(hwnd,GWL_HINSTANCE);
		MENUITEMINFO mii = {0};
		INT nPos = MenuUtil::FindMenuItemPos(hMenu,ID_INSERT_GROUP);
		if(nPos >= 0)
		{
			mii.cbSize = sizeof(mii);
			mii.fMask = MIIM_SUBMENU;
			mii.hSubMenu = MenuUtil::LoadPopupMenu(hInstance,IDR_GROUP);

			if(mii.hSubMenu)
			{
				ExtAddFramesToMenu(mii.hSubMenu);
				SetMenuItemInfo(hMenu,nPos,TRUE,&mii);
			}
		}

		nPos = MenuUtil::FindMenuItemPos(hMenu,ID_INSERT_MORE);
		if(nPos >= 0)
		{
			mii.cbSize = sizeof(mii);
			mii.fMask = MIIM_SUBMENU;
			mii.hSubMenu = CreatePopupMenu();

			if(mii.hSubMenu)
			{
				if(ExtAddMoreNeuronesToMenu(mii.hSubMenu))
					SetMenuItemInfo(hMenu,nPos,TRUE,&mii);
				else
				{
					DestroyMenu(mii.hSubMenu);
					RemoveMenu(hMenu,nPos,MF_BYPOSITION);
				}
			}
		}

		m_lpRecentChips->UpdateMenu(hMenu);

		ExtModifyAppMenu(hMenu);
	}
}

VOID CNeuralDocument::ClearSelection (LPSELECT_LIST lpSelection)
{
	LPSELECT_LIST lpTemp;
	while(lpSelection)
	{
		lpTemp = lpSelection;
		lpSelection = lpSelection->Next;
		lpTemp->lpObject->SelectObject(FALSE);
		lpTemp->lpObject->Release();
		__delete lpTemp;
	}
}

BOOL CNeuralDocument::SelectionContainsIO (VOID)
{
	BOOL fIO = FALSE;
	LPSELECT_LIST lpList = m_lpSelection;
	IIONeurone* lpIO;
	while(lpList)
	{
		if(SUCCEEDED(lpList->lpObject->QueryInterface(&lpIO)))
		{
			lpIO->Release();
			fIO = TRUE;
			break;
		}
		lpList = lpList->Next;
	}
	return fIO;
}

BOOL CNeuralDocument::FindOneSelectedChip (INeuralChip** lplpChip)
{
	BOOL fChip = FALSE;
	INeuralChip* lpChip = NULL;
	LPSELECT_LIST lpList = m_lpSelection;
	while(lpList)
	{
		if(SUCCEEDED(lpList->lpObject->QueryInterface(&lpChip)))
		{
			if(fChip)
			{
				// There are at least two chips selected.
				if(lplpChip)
					(*lplpChip)->Release();
				fChip = FALSE;
				break;
			}
			if(lplpChip)
				*lplpChip = lpChip;
			else
				lpChip->Release();
			fChip = TRUE;

			// Keep looking for another chip.
		}
		lpList = lpList->Next;
	}
	return fChip;
}

HRESULT CNeuralDocument::DeleteSelection (VOID)
{
	HRESULT hr = S_FALSE;
	IIONeurone* lpIO;
	INetDocObject* lpObject;
	while(m_lpSelection)
	{
		lpObject = m_lpSelection->lpObject;
		lpObject->AddRef();

		m_lpSelectedNet->RemoveObject(lpObject);
		if(SUCCEEDED(lpObject->QueryInterface(&lpIO)))
		{
			ReducePinNumbers(lpIO->GetIOType(),lpIO->GetPin());
			lpIO->Release();
		}
		if(RemoveSelection(lpObject))
		{
			SetChangesMade();
			hr = S_OK;
		}

		lpObject->Release();
	}
	return hr;
}

HRESULT CNeuralDocument::InsertObject (INetDocObject* lpObject)
{
	HRESULT hr;

	ClearSelection();
	hr = m_lpSelectedNet->AttachObject(lpObject);
	if(SUCCEEDED(hr))
	{
		hr = AddToSelection(lpObject);
		if(SUCCEEDED(hr))
		{
			m_xDragPos = (INT)m_lpGraph->GetGridSnap(0.0f);
			m_yDragPos = (INT)m_lpGraph->GetGridSnap(0.0f);
			m_iDragLevel = MOUSE_DRAG;
			m_fInsertSelection = TRUE;
		}

		SetChangesMade();

		// This must be last.
		if(m_lpAccessible)
			m_lpAccessible->NotifyAccEvent(EVENT_OBJECT_REORDER,NULL);
	}

	return hr;
}

VOID CNeuralDocument::SetGraphCapture (IGraphInputCapture* lpCapture)
{
	ReplaceInterface(m_lpCapture,lpCapture);
}

VOID CNeuralDocument::ReducePinNumbers (EIO_TYPE eType, ULONG iReducePin)
{
	LPNLIST lpList = m_lpSelectedNet->GetObjects();
	IIONeurone* lpIO;
	while(lpList)
	{
		if(SUCCEEDED(lpList->lpObject->QueryInterface(&lpIO)))
		{
			if(lpIO->GetIOType() == eType)
			{
				ULONG iPin = lpIO->GetPin();
				if(iPin > iReducePin)
					lpIO->SetPin(iPin - 1);
			}
			lpIO->Release();
		}
		lpList = lpList->Next;
	}
}

HRESULT CNeuralDocument::SaveToFile (LPCSTR lpcszFile)
{
	return CNeuralNet::SaveToFile(m_lpNet, m_lpLinks, lpcszFile);
}

VOID CNeuralDocument::ZoomIntoNet (INeuralNet* lpNet, INT x, INT y, INT cFrames)
{
	LPNETNAVLIST lpNewNav = __new NETNAVLIST;
	if(lpNewNav)
	{
		INT nWidth, nHeight;
		HDC hdc, hdcBuffer = NULL;
		HBITMAP hbmPrev = NULL, hbmBuffer = NULL;

		// Push the previous net onto the navigation list
		lpNewNav->lpNet = m_lpSelectedNet;
		lpNewNav->lpNet->AddRef();

		// Push the previous selection onto the navigation list
		lpNewNav->lpSelection = m_lpSelection;
		m_lpSelection = NULL;

		lpNewNav->Next = m_lpNetNav;
		m_lpNetNav = lpNewNav;

		m_lpGraph->GetClientSize(&nWidth,&nHeight);

		m_lpSelectedNet->Release();
		m_lpSelectedNet = lpNet;
		m_lpSelectedNet->AddRef();

		if(m_lpAccessible)
			m_lpAccessible->AttachNet(m_lpSelectedNet);

		if(SUCCEEDED(m_lpContainer->GetDC(this,&hdc)))
		{
			hbmBuffer = CreateCompatibleBitmap(hdc,nWidth,nHeight);
			if(hbmBuffer)
			{
				hdcBuffer = CreateCompatibleDC(hdc);
				if(hdcBuffer)
				{
					hbmPrev = (HBITMAP)SelectObject(hdcBuffer,hbmBuffer);
					m_lpGraph->Paint(hdcBuffer);
				}
			}
			m_lpContainer->ReleaseDC(this,hdc);
		}

		if(hdcBuffer)
		{
			FLOAT fRect[4];	// Left, Top, Right, Bottom
			FLOAT fDiff[4];	// Left, Top, Right, Bottom
			FLOAT fWidth = (FLOAT)nWidth;
			FLOAT fHeight = (FLOAT)nHeight;
			DWORD dwSleep = 400 / cFrames;
			DWORD dwStartTimer = 0;

			fRect[0] = (FLOAT)x;
			fRect[1] = (FLOAT)y;
			fRect[2] = fRect[0];
			fRect[3] = fRect[1];

			fDiff[0] = fRect[0] / (FLOAT)cFrames;
			fDiff[1] = fRect[1] / (FLOAT)cFrames;
			fDiff[2] = (fWidth - fRect[2]) / (FLOAT)cFrames;
			fDiff[3] = (fHeight - fRect[3]) / (FLOAT)cFrames;

			for(INT i = 0; i < cFrames; i++)
			{
				if(0 == i)
					dwStartTimer = GetTickCount();

				fRect[0] -= fDiff[0];
				fRect[1] -= fDiff[1];
				fRect[2] += fDiff[2];
				fRect[3] += fDiff[3];

				if(SUCCEEDED(m_lpContainer->GetDC(this,&hdc)))
				{
					if(fRect[0] < 0.0f)
						fRect[0] = 0.0f;
					if(fRect[1] < 0.0f)
						fRect[1] = 0.0f;
					if(fRect[2] > fWidth)
						fRect[2] = fWidth;
					if(fRect[3] > fHeight)
						fRect[3] = fHeight;
					StretchBlt(hdc,(INT)fRect[0],(INT)fRect[1],(INT)(fRect[2] - fRect[0]),(INT)(fRect[3] - fRect[1]),hdcBuffer,0,0,nWidth,nHeight,SRCCOPY);
					m_lpContainer->ReleaseDC(this,hdc);

					Sleep(dwSleep);
				}

				if(0 == i)
				{
					DWORD dwEndTimer = GetTickCount();
					DWORD dwDiff = dwEndTimer - dwStartTimer;
					if(dwDiff < dwSleep)
						dwSleep -= dwDiff;
					else
					{
						INT n = 1 + (dwDiff / dwSleep);
						for(INT x = 0; x < ARRAYSIZE(fDiff); x++)
							fDiff[x] *= n;
						cFrames /= n;
					}
				}
			}

			SelectObject(hdcBuffer,hbmPrev);
			DeleteDC(hdcBuffer);
		}

		if(hbmBuffer)
			DeleteObject(hbmBuffer);

		m_lpContainer->InvalidateContainer();
	}
}

VOID CNeuralDocument::ZoomOutOfNet (INeuralNet* lpNet, INT cFrames)
{
	INT nWidth, nHeight;
	HDC hdc, hdcBuffer = NULL, hdcLast = NULL, hdcNew = NULL;
	HBITMAP hbmPrev = NULL, hbmPrevLast = NULL, hbmPrevNew = NULL;
	HBITMAP hbmBuffer = NULL, hbmLast = NULL, hbmNew = NULL;

	m_lpGraph->GetClientSize(&nWidth,&nHeight);

	if(SUCCEEDED(m_lpContainer->GetDC(this,&hdc)))
	{
		hbmBuffer = CreateCompatibleBitmap(hdc,nWidth,nHeight);
		if(hbmBuffer)
		{
			hdcBuffer = CreateCompatibleDC(hdc);
			if(hdcBuffer)
				hbmPrev = (HBITMAP)SelectObject(hdcBuffer,hbmBuffer);
		}

		hbmLast = CreateCompatibleBitmap(hdc,nWidth,nHeight);
		if(hbmLast)
		{
			hdcLast = CreateCompatibleDC(hdc);
			if(hdcLast)
			{
				hbmPrevLast = (HBITMAP)SelectObject(hdcLast,hbmLast);
				m_lpGraph->Paint(hdcLast);
			}
		}

		m_lpSelectedNet->Release();
		m_lpSelectedNet = lpNet;
		m_lpSelectedNet->AddRef();

		if(m_lpAccessible)
			m_lpAccessible->AttachNet(m_lpSelectedNet);

		hbmNew = CreateCompatibleBitmap(hdc,nWidth,nHeight);
		if(hbmNew)
		{
			hdcNew = CreateCompatibleDC(hdc);
			if(hdcNew)
			{
				hbmPrevNew = (HBITMAP)SelectObject(hdcNew,hbmNew);
				m_lpGraph->Paint(hdcNew);
			}
		}

		m_lpContainer->ReleaseDC(this,hdc);
	}
	else
	{
		m_lpSelectedNet->Release();
		m_lpSelectedNet = lpNet;
		m_lpSelectedNet->AddRef();
	}

	if(hdcBuffer && hdcLast && hdcNew)
	{
		FLOAT fRect[4];	// Left, Top, Right, Bottom
		FLOAT fDiff[2];	// Width, Height
		FLOAT fWidth = (FLOAT)nWidth;
		FLOAT fHeight = (FLOAT)nHeight;
		DWORD dwSleep = 400 / cFrames;
		DWORD dwStartTimer = 0;

		fRect[0] = 0.0f;
		fRect[1] = 0.0f;
		fRect[2] = fWidth;
		fRect[3] = fHeight;

		fDiff[0] = (fWidth / 2.0f) / (FLOAT)cFrames;
		fDiff[1] = (fHeight / 2.0f) / (FLOAT)cFrames;

		for(INT i = 0; i < cFrames; i++)
		{
			if(0 == i)
				dwStartTimer = GetTickCount();

			fRect[0] += fDiff[0];
			fRect[1] += fDiff[1];
			fRect[2] -= fDiff[0];
			fRect[3] -= fDiff[1];

			if(SUCCEEDED(m_lpContainer->GetDC(this,&hdc)))
			{
				if(fRect[0] < 0.0f)
					fRect[0] = 0.0f;
				if(fRect[1] < 0.0f)
					fRect[1] = 0.0f;
				if(fRect[2] > fWidth)
					fRect[2] = fWidth;
				if(fRect[3] > fHeight)
					fRect[3] = fHeight;
				BitBlt(hdcBuffer,0,0,nWidth,nHeight,hdcNew,0,0,SRCCOPY);
				StretchBlt(hdcBuffer,(INT)fRect[0],(INT)fRect[1],(INT)(fRect[2] - fRect[0]),(INT)(fRect[3] - fRect[1]),hdcLast,0,0,nWidth,nHeight,SRCCOPY);
				BitBlt(hdc,0,0,nWidth,nHeight,hdcBuffer,0,0,SRCCOPY);
				m_lpContainer->ReleaseDC(this,hdc);

				Sleep(dwSleep);
			}

			if(0 == i)
			{
				DWORD dwEndTimer = GetTickCount();
				DWORD dwDiff = dwEndTimer - dwStartTimer;
				if(dwDiff < dwSleep)
					dwSleep -= dwDiff;
				else
				{
					INT n = 1 + (dwDiff / dwSleep);
					for(INT x = 0; x < ARRAYSIZE(fDiff); x++)
						fDiff[x] *= n;
					cFrames /= n;
				}
			}
		}
	}

	if(hdcBuffer)
	{
		SelectObject(hdcBuffer,hbmPrev);
		DeleteDC(hdcBuffer);
	}

	if(hbmBuffer)
		DeleteObject(hbmBuffer);

	if(hdcLast)
	{
		SelectObject(hdcLast,hbmPrevLast);
		DeleteDC(hdcLast);
	}

	if(hbmLast)
		DeleteObject(hbmLast);

	if(hdcNew)
	{
		SelectObject(hdcNew,hbmPrevNew);
		DeleteDC(hdcNew);
	}

	if(hbmNew)
		DeleteObject(hbmNew);
}

HRESULT CNeuralDocument::GetSelection (VARIANT** lplpvDispatchList, ULONG* lpcList)
{
	HRESULT hr = S_OK;
	VARIANT* lpvDispatchList = NULL;
	LPSELECT_LIST lpList = m_lpSelection;
	ULONG c = 0;

	Assert(m_lpAccessible);

	while(lpList)
	{
		c++;
		lpList = lpList->Next;
	}

	if(c > 0)
	{
		lpvDispatchList = __new VARIANT[c];
		if(lpvDispatchList)
		{
			IAccessible* lpAccessible;
			ULONG i = 0;
			lpList = m_lpSelection;
			while(lpList)
			{
				if(SUCCEEDED(lpList->lpObject->GetAccObject(m_lpAccessible,&lpAccessible)))
				{
					if(SUCCEEDED(lpAccessible->QueryInterface(IID_IDispatch,(LPVOID*)&lpvDispatchList[i].pdispVal)))
					{
						lpvDispatchList[i].vt = VT_DISPATCH;
						i++;
					}
					lpAccessible->Release();
				}
				lpList = lpList->Next;
			}

			Assert(c == i);	// Did everything support accessibility?
			c = i;

			if(c == 0)
			{
				__delete_array lpvDispatchList;
				lpvDispatchList = NULL;
			}
		}
		else
			hr = E_OUTOFMEMORY;
	}
	*lplpvDispatchList = lpvDispatchList;
	*lpcList = c;
	return hr;
}

HRESULT CNeuralDocument::GetFileName (LPSTR* lplpszFileName)
{
	return TDuplicateStringChecked(m_lpszFile, lplpszFileName);
}

VOID CNeuralDocument::PerformNetCycle (VOID)
{
	m_lpNet->SendPulses();
	m_lpLinks->SendPulses();
	m_lpNet->CheckThresholds();
	m_lpContainer->InvalidateContainer();
}

HRESULT CNeuralDocument::LoadChipFromFile (PCSTR pcszFile, INT cchFile, __deref_out INetDocObject** ppChip)
{
	HRESULT hr;

	Assert(pcszFile && ppChip);

	CNeuralChip* pChip = __new CNeuralChip;
	if(pChip)
	{
		hr = pChip->LoadFromFile(m_pFactory, m_lpLinks, pcszFile, cchFile);
		if(SUCCEEDED(hr))
			*ppChip = pChip;
		else
			pChip->Release();

		if(SUCCEEDED(hr))
			SideAssertHr(m_lpRecentChips->AddRecentItem(pcszFile));
		else if(FAILED(hr) && m_lpContainer)
		{
			HWND hwnd;
			if(SUCCEEDED(m_lpContainer->GetHwnd(&hwnd)))
			{
				if(HRESULT_FROM_WIN32(ERROR_CONNECTION_UNAVAIL) == hr)
					MessageBox(hwnd,"The selected file did not contain any connection pins.","No Connection Pins",MB_OK);
				else if(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr)
					MessageBox(hwnd,"The selected file could not be opened.  It no longer exists.","Missing File",MB_OK);
			}
		}
	}
	else
		hr = E_OUTOFMEMORY;

	return hr;
}

HRESULT CNeuralDocument::CreateUsingFactory (DWORD nCmdID, INetDocObject** ppObject)
{
	HRESULT hr = E_INVALIDARG;
	const OBJECT_MAP rgObjects[] =
	{
		{ ID_INSERT_NEURONE, "CNeurone" },
		{ ID_INSERT_INPUT, "CInputNeurone" },
		{ ID_INSERT_OUTPUT, "COutputNeurone" },
		{ ID_INSERT_AND_GATE, "CLogicAnd" },
		{ ID_INSERT_OR_GATE, "CLogicOr" },
		{ ID_INSERT_NOT_GATE, "CLogicNot" },
		{ ID_INSERT_XOR_GATE, "CLogicXor" },
		{ ID_INSERT_PARITY_GENERATOR, "CLogicXor3" },
		{ ID_INSERT_INPUTPAD, "CInputPad" },
		{ ID_INSERT_SPLITTER, "CSplitter" },
		{ ID_INSERT_BIAS, "CBiasNeurone" },
		{ ID_INSERT_SIGMOID, "CSigmoidNeurone" }
	};

	for(INT i = 0; i < ARRAYSIZE(rgObjects); i++)
	{
		if(nCmdID == rgObjects[i].nCmd)
		{
			hr = m_pFactory->Create(m_lpSelectedNet, rgObjects[i].pcszClass, ppObject);
			break;
		}
	}

	return hr;
}

HRESULT CNeuralDocument::InsertObject (DWORD nCmdID)
{
	HRESULT hr;
	INetDocObject* lpObject = NULL;

	switch(nCmdID)
	{
	case ID_INSERT_NEURONE:
	case ID_INSERT_INPUT:
	case ID_INSERT_OUTPUT:
	case ID_INSERT_AND_GATE:
	case ID_INSERT_OR_GATE:
	case ID_INSERT_NOT_GATE:
	case ID_INSERT_XOR_GATE:
	case ID_INSERT_PARITY_GENERATOR:
	case ID_INSERT_INPUTPAD:
	case ID_INSERT_SPLITTER:
	case ID_INSERT_BIAS:
	case ID_INSERT_SIGMOID:
		hr = CreateUsingFactory(nCmdID, &lpObject);
		if(SUCCEEDED(hr))
			m_cmdDoubleClick = nCmdID;
		break;
	case ID_INSERT_CHIP:
		{
			CChooseFile Choose;
			HWND hwnd;
			SideAssertHr(m_lpContainer->GetHwnd(&hwnd));
			hr = Choose.Initialize();
			if(SUCCEEDED(hr))
			{
				if(Choose.OpenSingleFile(hwnd,"Simbey Neural Net\0*.snn\0\0"))
				{
					PCSTR pcszFile = Choose.GetFile(0);
					hr = LoadChipFromFile(pcszFile, lstrlen(pcszFile), &lpObject);
				}
				else
					hr = OLECMDERR_E_CANCELED;
			}
			break;
		}
	case ID_INSERT_SIMPLEFRAME:
		{
			CNeuralFrame* lpFrame = __new CNeuralFrame;
			if(lpFrame)
			{
				INT nSize = (INT)m_lpGraph->GetGridSnap(NEURAL_FRAME_SIZE / 2.0f) << 1;
				hr = lpFrame->SetFrameSize(nSize, nSize);
				if(SUCCEEDED(hr))
					lpObject = lpFrame;
				else
					lpFrame->Release();
			}
			else
				hr = E_OUTOFMEMORY;
			break;
		}
	default:
		hr = E_INVALIDARG;
		break;
	}

	if(SUCCEEDED(hr))
	{
		hr = InsertObject(lpObject);
		lpObject->Release();
	}

	return hr;
}

HRESULT CNeuralDocument::SetProperty (LPCSTR lpcszName, LPVOID lpValue, DWORD cbValue)
{
	HKEY hKey;
	HRESULT hr = Registry::CreateKey(HKEY_CURRENT_USER, "Software\\Simbey\\NeuralNet", KEY_WRITE, &hKey);
	if(SUCCEEDED(hr))
	{
		hr = HRESULT_FROM_WIN32(RegSetValueEx(hKey,lpcszName,NULL,REG_BINARY,(LPBYTE)lpValue,cbValue));
		RegCloseKey(hKey);
	}
	return hr;
}

HRESULT CNeuralDocument::GetProperty (LPCSTR lpcszName, LPVOID lpValue, DWORD cbMaxValue, DWORD* lpcbValue)
{
	HKEY hKey;
	HRESULT hr = Registry::CreateKey(HKEY_CURRENT_USER, "Software\\Simbey\\NeuralNet", KEY_READ, &hKey);
	if(SUCCEEDED(hr))
	{
		hr = HRESULT_FROM_WIN32(RegQueryValueEx(hKey, lpcszName, NULL, NULL, (LPBYTE)lpValue, &cbMaxValue));
		if(SUCCEEDED(hr))
		{
			if(lpcbValue)
				*lpcbValue = cbMaxValue;
		}
		RegCloseKey(hKey);
	}
	return hr;
}

VOID CNeuralDocument::SetFrameHighlighting (BOOL fHighlight)
{
	if(m_lpSelection)
	{
		LPSELECT_LIST lpList = m_lpSelection;
		while(lpList)
		{
			CheckObjectFrame(lpList->lpObject, FALSE, fHighlight);
			lpList = lpList->Next;
		}
	}
}

VOID CNeuralDocument::CheckObjectFrame (INetDocObject* lpObject, BOOL fHover, BOOL fForceHighlight)
{
	INeuralFrame* lpFrame;
	if(FAILED(lpObject->QueryInterface(&lpFrame)))
	{
		INeuralFrame* lpCurrentFrame = NULL;
		
		m_lpSelectedNet->GetFrame(lpObject,&lpCurrentFrame);

		if(fHover)
		{
			INT x, y;
			lpObject->GetPosition(x,y);

			INeuralFrame* lpNewFrame = NULL;
			
			m_lpSelectedNet->GetFrame(x,y,&lpNewFrame);

			if(lpCurrentFrame != lpNewFrame)
			{
				if(lpCurrentFrame)
				{
					lpCurrentFrame->HighlightFrame(FALSE);
					lpCurrentFrame->RemoveObject(lpObject);
				}
				if(lpNewFrame)
				{
					lpNewFrame->AddObject(lpObject);
					lpNewFrame->HighlightFrame(TRUE);
				}
			}

			if(lpNewFrame)
				lpNewFrame->Release();
		}
		else if(lpCurrentFrame)
			lpCurrentFrame->HighlightFrame(fForceHighlight);

		if(lpCurrentFrame)
			lpCurrentFrame->Release();
	}
	else
		lpFrame->Release();
}

VOID CNeuralDocument::RegroupFrame (INetDocObject* lpObject)
{
	INeuralFrame* lpFrame, *lpContainer;

	Assert(lpObject);

	if(SUCCEEDED(lpObject->QueryInterface(&lpFrame)))
	{
		INT x, y, nHitTest;
		LPNLIST lpList = m_lpSelectedNet->GetObjects();
		while(lpList)
		{
			lpList->lpObject->GetPosition(x,y);
			nHitTest = lpFrame->HitTest(x,y);
			if(nHitTest & HITTEST_DRAG_FRAME)
			{
				if(m_lpSelectedNet->GetFrame(lpList->lpObject,&lpContainer))
					lpContainer->Release();
				else
					lpFrame->AddObject(lpList->lpObject);
			}
			else
				lpFrame->RemoveObject(lpList->lpObject);
			lpList = lpList->Next;
		}
		lpFrame->Release();
	}
}

HRESULT CNeuralDocument::AddToSelectionInternal (INetDocObject* lpObject, DWORD dwFlags)
{
	HRESULT hr = S_OK;
	LPSELECT_LIST lpList = m_lpSelection, lpPrev = NULL;
	Assert(lpObject);
	while(lpList)
	{
		if(lpList->lpObject == lpObject)
		{
			hr = S_FALSE;
			break;
		}
		lpPrev = lpList;
		lpList = lpList->Next;
	}
	if(S_OK == hr)
	{
		LPSELECT_LIST lpNew = __new SELECT_LIST;
		if(lpNew)
		{
			lpNew->dwFlags = dwFlags;
			lpNew->lpObject = lpObject;
			lpNew->lpObject->AddRef();
			lpNew->lpObject->SelectObject(TRUE);
			lpNew->Next = NULL;

			if(lpPrev)
				lpPrev->Next = lpNew;
			else
				m_lpSelection = lpNew;
		}
		else
			hr = E_OUTOFMEMORY;
	}
	else
	{
		Assert(lpList);

		if(lpList->dwFlags & SELECT_FROM_RANGE_PASSIVE)
		{
			lpList->dwFlags &= ~SELECT_FROM_RANGE_PASSIVE;
			lpList->dwFlags |= SELECT_FROM_RANGE_ACTIVE;
		}
	}
	return hr;
}

VOID CNeuralDocument::CullSelectionRange (VOID)
{
	LPSELECT_LIST lpSelection = m_lpSelection, lpNext, lpPrev = NULL;
	while(lpSelection)
	{
		lpNext = lpSelection->Next;
		if(lpSelection->dwFlags & SELECT_FROM_RANGE_PASSIVE)
		{
			lpSelection->lpObject->SelectObject(FALSE);
			lpSelection->lpObject->Release();
			if(lpPrev)
				lpPrev->Next = lpNext;
			else
				m_lpSelection = lpNext;
			__delete lpSelection;
		}
		else
		{
			if(lpSelection->dwFlags & SELECT_FROM_RANGE_ACTIVE)
			{
				lpSelection->dwFlags &= ~SELECT_FROM_RANGE_ACTIVE;
				lpSelection->dwFlags |= SELECT_FROM_RANGE_PASSIVE;
			}
			lpPrev = lpSelection;
		}
		lpSelection = lpNext;
	}
}

VOID CNeuralDocument::GetSelectionRect (IGrapher* lpGraph, RECT* lprc)
{
	lpGraph->GraphToClient((FLOAT)m_xDown,(FLOAT)m_yDown,(INT&)lprc->left,(INT&)lprc->top);
	lpGraph->GraphToClient((FLOAT)m_xDragPos,(FLOAT)m_yDragPos,(INT&)lprc->right,(INT&)lprc->bottom);
	DIBDrawing::NormalizeRect(lprc);
}

VOID CNeuralDocument::SetResizeCursor (INT nHitTest)
{
	LPCSTR lpcszCursor = NULL;
	switch(nHitTest)
	{
	case HITTEST_RESIZE_V:
		lpcszCursor = IDC_SIZENS;
		break;
	case HITTEST_RESIZE_H:
		lpcszCursor = IDC_SIZEWE;
		break;
	case HITTEST_RESIZE_C1:
		lpcszCursor = IDC_SIZENWSE;
		break;
	case HITTEST_RESIZE_C2:
		lpcszCursor = IDC_SIZENESW;
		break;
	}
	if(lpcszCursor)
		SetCursor(LoadCursor(NULL,lpcszCursor));
}

HRESULT CNeuralDocument::SendCommandToSelection (DWORD nCmd, DWORD nCmdOpt)
{
	HRESULT hr;
	LPSELECT_LIST pList = m_lpSelection;

	CheckIf(NULL == pList, E_FAIL);

	do
	{
		TStackRef<IOleCommandTarget> srCmdTarget;
		if(SUCCEEDED(pList->lpObject->QueryInterface(&srCmdTarget)))
			srCmdTarget->Exec(NULL, nCmd, nCmdOpt, NULL, NULL);
		pList = pList->Next;
	} while(pList);

	hr = S_OK;

Cleanup:
	return hr;
}
