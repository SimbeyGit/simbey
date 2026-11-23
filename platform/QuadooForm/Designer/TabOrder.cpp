#include "Designer.h"

namespace QFD
{
struct TabEntry { GUID id; IQuadooProperties* properties; UINT index; INT initial; };
class CTabEntries
{
public:
	TArray<TabEntry> items;
	~CTabEntries() { for(sysint n = 0; n < items.Length(); n++) items[n].properties->Release(); }
	HRESULT Read(IQuadooControlContainer* parent)
	{
		HRESULT hr; UINT count;
		CheckIf(!parent,E_INVALIDARG);
		Check(parent->GetChildCount(&count));
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooControl> child; TStackRef<IQuadooProperties> properties;
			TStackRef<IQuadooPropertyDescriptors> descriptors; TStackRef<IQuadooPropertyDescriptor> descriptor;
			TabEntry entry; QuadooProperty::Type type; DWORD flags;
			Check(parent->GetChild(n,&child));
			hr = Property(child,L"TabOrder",&properties,&entry.index);
			if(hr == DISP_E_MEMBERNOTFOUND) continue;
			Check(hr);
			Check(properties->GetDescriptors(&descriptors));
			Check(descriptors->GetAt(entry.index,&descriptor));
			Check(descriptor->GetType(&type));
			Check(descriptor->GetFlags(&flags));
			CheckIf(type != QuadooProperty::Int,E_INVALIDARG);
			if(flags & (QuadooForm::PropertyReadOnly | QuadooForm::PropertyRuntimeOnly)) continue;
			Check(child->GetID(&entry.id));
			Check(properties->GetValueInt(entry.index,&entry.initial));
			entry.properties = properties;
			sysint position = 0;
			while(position < items.Length() && items[position].initial <= entry.initial) position++;
			Check(items.InsertAt(entry,position));
			properties.Detach();
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
};
HRESULT NextTabOrder(IQuadooControlContainer* parent, INT* order)
{
	HRESULT hr; CTabEntries entries;
	CheckIf(!order,E_POINTER);
	*order = 0;
	Check(entries.Read(parent));
	if(entries.items.Length())
	{
		INT last = entries.items[entries.items.Length() - 1].initial;
		CheckIf(last == INT_MAX,HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW));
		*order = max(0,last + 1);
	}
Cleanup:
	return hr;
}
static HRESULT VerifyContainer(CDesignerDocument* document, IQuadooControlContainer* parent)
{
	HRESULT hr; GUID id; TStackRef<IQuadooControl> control;
	CheckIf(!parent,E_INVALIDARG);
	Check(parent->GetID(&id));
	Check(document->Find(id,&control));
	CheckIf(!SameObject(parent,control),E_INVALIDARG);
Cleanup:
	return hr;
}
HRESULT CDesignerDocument::GetTabOrder(IQuadooControlContainer* parent, TArray<GUID>& order)
{
	HRESULT hr; CTabEntries entries;
	order.Clear();
	Check(VerifyContainer(this,parent));
	Check(entries.Read(parent));
	for(sysint n = 0; n < entries.items.Length(); n++)
	{
		Check(order.Append(entries.items[n].id));
	}
Cleanup:
	if(FAILED(hr)) order.Clear();
	return hr;
}
HRESULT CDesignerDocument::SetTabOrder(IQuadooControlContainer* parent, const GUID* order, UINT count)
{
	HRESULT hr; CTabEntries entries; TStackRef<IJSONObject> snapshot;
	BOOL updating = FALSE, changed = FALSE, rollback = FALSE, dirty = m_dirty;
	CheckIf(!parent || (!order && count) || count > INT_MAX,E_INVALIDARG);
	Check(VerifyContainer(this,parent));
	Check(entries.Read(parent));
	CheckIf(count != entries.items.Length(),E_INVALIDARG);
	for(UINT n = 0; n < count; n++)
	{
		UINT found = count;
		for(UINT p = 0; p < count; p++) if(IsEqualGUID(order[n],entries.items[p].id)) found = p;
		CheckIf(found == count,E_INVALIDARG);
		for(UINT p = 0; p < n; p++)
		{
			CheckIf(IsEqualGUID(order[p],order[n]),E_INVALIDARG);
		}
		if(entries.items[found].initial != static_cast<INT>(n)) changed = TRUE;
	}
	CheckIfIgnore(!changed,S_FALSE);
	Check(Serialize(&snapshot));
	Check(parent->BeginUpdate());
	updating = TRUE; rollback = TRUE;
	for(UINT n = 0; n < count; n++)
	{
		for(UINT p = 0; p < count; p++) if(IsEqualGUID(order[n],entries.items[p].id))
		{
			Check(entries.items[p].properties->SetValueInt(entries.items[p].index,n));
			break;
		}
	}
	updating = FALSE;
	Check(parent->EndUpdate());
	rollback = FALSE; m_dirty = TRUE;
Cleanup:
	if(rollback)
	{
		BOOL failed = FALSE;
		// A failed setter may already have changed its value. Restore every entry.
		for(sysint n = 0; n < entries.items.Length(); n++)
			if(FAILED(entries.items[n].properties->SetValueInt(entries.items[n].index,entries.items[n].initial))) failed = TRUE;
		if(updating) { if(FAILED(parent->EndUpdate())) failed = TRUE; }
		else if(FAILED(parent->UpdateLayout())) failed = TRUE;
		if(failed) Load(GetParent(m_host),snapshot,m_path);
		m_dirty = dirty;
	}
	return hr;
}
HRESULT CDesignerEditor::GetTabOrderContainer(IQuadooControlContainer** parent)
{
	HRESULT hr; TStackRef<IQuadooControl> control;
	CheckIf(!parent,E_POINTER);
	*parent = NULL;
	hr = Selected(&control);
	if(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) { Check(m_document.Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(parent))); goto Cleanup; }
	Check(hr);
	hr = control->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(parent));
	if(hr == E_NOINTERFACE)
	{
		Check(control->GetParent(parent));
		CheckIf(!*parent,E_INVALIDARG);
	}
	else
	{
		Check(hr);
	}
Cleanup:
	return hr;
}
class CTabOrderDialog
{
	CDesignerDocument* m_document;
	IQuadooControlContainer* m_parent;
	HWND m_list;
	HRESULT m_error;
	static HWND Child(HWND window, PCWSTR type, PCWSTR text, DWORD style, INT id, INT x, INT y, INT width, INT height)
	{
		RECT rect = { x,y,x + width,y + height }; MapDialogRect(window,&rect);
		HWND child = CreateWindowExW(0,type,text,WS_CHILD | WS_VISIBLE | style,rect.left,rect.top,rect.right - rect.left,rect.bottom - rect.top,window,reinterpret_cast<HMENU>(id),GetModuleHandle(NULL),NULL);
		if(child) SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),FALSE);
		return child;
	}
	VOID EnableButtons(HWND window)
	{
		INT selected = static_cast<INT>(SendMessageW(m_list,LB_GETCURSEL,0,0));
		EnableWindow(GetDlgItem(window,10),selected > 0);
		EnableWindow(GetDlgItem(window,11),selected >= 0 && selected + 1 < order.Length());
	}
	HRESULT Initialize(HWND window)
	{
		HRESULT hr; CRString name, caption;
		Check(m_parent->GetName(&name));
		Check(RStrFormatW(&caption,L"Tab Order - %ls",name.c_wstr()));
		SetWindowTextW(window,caption.c_wstr());
		Check(m_document->GetTabOrder(m_parent,order));
		CheckIf(!Child(window,L"STATIC",L"Controls in this container:",SS_LEFT,20,8,8,204,12),HrEnsureLastError());
		m_list = Child(window,L"LISTBOX",L"",WS_BORDER | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,100,8,23,204,150);
		CheckIf(!m_list,HrEnsureLastError());
		CheckIf(!Child(window,L"BUTTON",L"Move &Up",WS_TABSTOP,10,220,23,52,16),HrEnsureLastError());
		CheckIf(!Child(window,L"BUTTON",L"Move &Down",WS_TABSTOP,11,220,45,52,16),HrEnsureLastError());
		CheckIf(!Child(window,L"BUTTON",L"OK",WS_TABSTOP | BS_DEFPUSHBUTTON,IDOK,160,184,52,16),HrEnsureLastError());
		CheckIf(!Child(window,L"BUTTON",L"Cancel",WS_TABSTOP,IDCANCEL,220,184,52,16),HrEnsureLastError());
		for(sysint n = 0; n < order.Length(); n++)
		{
			TStackRef<IQuadooControl> control; name = static_cast<RSTRING>(NULL);
			Check(m_document->Find(order[n],&control));
			Check(control->GetName(&name));
			CheckIf(SendMessageW(m_list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_wstr())) < 0,E_OUTOFMEMORY);
		}
		if(order.Length()) SendMessageW(m_list,LB_SETCURSEL,0,0);
		EnableButtons(window); SetFocus(m_list);
	Cleanup:
		return hr;
	}
	HRESULT Move(HWND window, INT step)
	{
		HRESULT hr = S_FALSE;
		TStackRef<IQuadooControl> control; CRString name;
		INT current = static_cast<INT>(SendMessageW(m_list,LB_GETCURSEL,0,0)), next = current + step;
		CheckIfIgnore(current < 0 || next < 0 || next >= order.Length(),S_FALSE);
		Check(m_document->Find(order[current],&control));
		Check(control->GetName(&name));
		CheckIf(SendMessageW(m_list,LB_INSERTSTRING,next > current ? next + 1 : next,reinterpret_cast<LPARAM>(name.c_wstr())) < 0,E_OUTOFMEMORY);
		SendMessageW(m_list,LB_DELETESTRING,next > current ? current : current + 1,0);
		order.MoveItem(next,current); SendMessageW(m_list,LB_SETCURSEL,next,0); EnableButtons(window);
	Cleanup:
		return hr;
	}
	static INT_PTR CALLBACK Procedure(HWND window, UINT msg, WPARAM wp, LPARAM lp)
	{
		CTabOrderDialog* data = reinterpret_cast<CTabOrderDialog*>(GetWindowLongPtrW(window,DWLP_USER));
		if(msg == WM_INITDIALOG)
		{
			data = reinterpret_cast<CTabOrderDialog*>(lp); SetWindowLongPtrW(window,DWLP_USER,lp);
			data->m_error = data->Initialize(window); if(FAILED(data->m_error)) EndDialog(window,IDCANCEL);
			return FALSE;
		}
		if(!data) return FALSE;
		if(msg == WM_CLOSE) { EndDialog(window,IDCANCEL); return TRUE; }
		if(msg == WM_COMMAND)
		{
			if(LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) { EndDialog(window,LOWORD(wp)); return TRUE; }
			if(LOWORD(wp) == 10 || LOWORD(wp) == 11)
			{
				data->m_error = data->Move(window,LOWORD(wp) == 10 ? -1 : 1);
				if(FAILED(data->m_error)) EndDialog(window,IDCANCEL);
				return TRUE;
			}
			if(LOWORD(wp) == 100 && HIWORD(wp) == LBN_SELCHANGE) { data->EnableButtons(window); return TRUE; }
		}
		return FALSE;
	}
public:
	TArray<GUID> order;
	CTabOrderDialog(CDesignerDocument* document, IQuadooControlContainer* parent) : m_document(document), m_parent(parent), m_list(NULL), m_error(S_OK) {}
	HRESULT Show(HWND owner)
	{
		#pragma pack(push,2)
		struct EmptyDialog { DLGTEMPLATE dialog; WORD menu, windowClass, title; };
		#pragma pack(pop)
		EmptyDialog definition; ZeroMemory(&definition,sizeof(definition));
		definition.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER;
		definition.dialog.cx = 280; definition.dialog.cy = 210;
		INT_PTR answer = DialogBoxIndirectParamW(GetModuleHandle(NULL),&definition.dialog,owner,Procedure,reinterpret_cast<LPARAM>(this));
		return answer == -1 ? HrEnsureLastError() : FAILED(m_error) ? m_error : answer == IDOK ? S_OK : S_FALSE;
	}
};
HRESULT CDesignerEditor::EditTabOrder()
{
	HRESULT hr; TStackRef<IQuadooControlContainer> parent;
	Check(FinishInline(TRUE));
	Check(GetTabOrderContainer(&parent));
	{
		CTabOrderDialog dialog(&m_document,parent);
		hr = dialog.Show(m_window); if(hr != S_OK) goto Cleanup;
		m_changeDepth++;
		hr = m_document.SetTabOrder(parent,dialog.order.Length() ? &dialog.order[0] : NULL,static_cast<UINT>(dialog.order.Length()));
		m_changeDepth--; Refresh(TRUE);
	}
Cleanup:
	return hr;
}
}
