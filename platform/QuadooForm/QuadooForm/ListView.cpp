#include "ListView.h"

namespace QF
{
	CListView::CListView(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		Super(factory,context,parent), m_nativeSync(0), m_editing(FALSE), m_redrawBlocked(FALSE),
		m_nativeDPI(0), m_busy(0), m_events(0), m_loaded(FALSE),
		m_headerPending(FALSE), m_imageChanging(FALSE), m_imageRefreshing(FALSE),
		m_virtualCount(0), m_generation(1), m_virtualNativeStyle(0), m_virtualEdit(-1),
		m_editGeneration(0), m_stateError(S_OK)
	{
	}
	CListView::~CListView()
	{
		for(UINT n = 0; n < ARRAYSIZE(m_images); n++)
		{
			DetachImages(n);
		}
		if(m_link)
		{
			m_link->view = NULL;
		}
		ReleaseColumns(columns);
		ReleaseItems(rows);
	}
	HRESULT CListView::Mutable()
	{
		HRESULT hr;
		Check(CheckAlive());
		CheckIf(m_busy || m_events || m_replacing || m_nativeSync || m_editing,QuadooForm::EInvalidState);
		CheckIf(IsVirtual() && m_generation == ~static_cast<ULONGLONG>(0),E_UNEXPECTED);
		if(m_headerPending)
		{
			Check(ReadNativeColumns());
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::SyncHeader()
	{
		HRESULT hr;
		Check(CheckAlive());
		if(m_headerPending)
		{
			Check(ReadNativeColumns());
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::CreateNative()
	{
		HRESULT hr;
		INITCOMMONCONTROLSEX controls = {sizeof(controls),ICC_LISTVIEW_CLASSES};
		CheckIf(!InitCommonControlsEx(&controls),HrEnsureLastError());
		Check(Super::CreateNative());
	Cleanup:
		return hr;
	}
	VOID CListView::NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended)
	{
		static const DWORD nativeViews[] = {LVS_ICON,LVS_SMALLICON,LVS_LIST,LVS_REPORT};
		Super::NativeStyle(name,style,extended);
		*name = WC_LISTVIEWW;
		// Model enum values deliberately differ from the native LVS_* values.
		*style |= WS_TABSTOP | LVS_SHAREIMAGELISTS | nativeViews[Integer(L"ViewStyle")];
		*extended |= WS_EX_CLIENTEDGE;
		if(Boolean(L"OwnerData"))
		{
			*style |= LVS_OWNERDATA;
		}
		if(!Boolean(L"MultiSelect"))
		{
			*style |= LVS_SINGLESEL;
		}
		if(!Boolean(L"HideSelection"))
		{
			*style |= LVS_SHOWSELALWAYS;
		}
		if(!Boolean(L"ReadOnly"))
		{
			*style |= LVS_EDITLABELS;
		}
		if(!Boolean(L"ShowColumnHeaders"))
		{
			*style |= LVS_NOCOLUMNHEADER;
		}
		if(!Boolean(L"ColumnClick"))
		{
			*style |= LVS_NOSORTHEADER;
		}
	}
	HRESULT CListView::NativeCreated()
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		TStackRef<IJSONArray> items;
		DWORD styles = 0;
		Check(Super::NativeCreated());
		ListView_SetUnicodeFormat(m_hwnd,TRUE);
		if(!m_loaded)
		{
			m_link.Attach(__new CListLink(m_factory));
			CheckAlloc(m_link);
			m_link->view = this;
			m_collection.Attach(__new CListColumns(m_link));
			CheckAlloc(m_collection);
			m_items.Attach(__new CListItems(m_link));
			CheckAlloc(m_items);
			Check(StoredValue(L"Columns",&value));
			Check(ParseColumns(value,columns));
			value.Release();
			Check(StoredValue(L"Items",&value));
			Check(ParseItems(value,rows));
			value.Release();
			Check(StoredValue(L"StateImages",&value));
			CheckIf(value && Boolean(L"CheckBoxes"),E_INVALIDARG);
			m_loaded = TRUE;
		}
		if(Boolean(L"FullRowSelect"))
		{
			styles |= LVS_EX_FULLROWSELECT;
		}
		if(Boolean(L"GridLines"))
		{
			styles |= LVS_EX_GRIDLINES;
		}
		if(Boolean(L"ColumnReorder"))
		{
			styles |= LVS_EX_HEADERDRAGDROP;
		}
		if(Boolean(L"CheckBoxes") && !IsVirtual())
		{
			styles |= LVS_EX_CHECKBOXES;
		}
		ListView_SetExtendedListViewStyle(m_hwnd,styles);
		CheckIf(Boolean(L"CheckBoxes") && !IsVirtual() && !ListView_GetImageList(m_hwnd,LVSIL_STATE),E_OUTOFMEMORY);
		if(Boolean(L"CheckBoxes") && !IsVirtual())
		{
			HIMAGELIST checks = ListView_GetImageList(m_hwnd,LVSIL_STATE);
			if(!SetPropW(m_hwnd,L"QuadooForm.ListView.Checks",checks))
			{
				hr = HrEnsureLastError();
				ListView_SetImageList(m_hwnd,NULL,LVSIL_STATE);
				ImageList_Destroy(checks);
				goto Cleanup;
			}
		}
		SendMessageW(m_hwnd,LVM_SETBKCOLOR,0,ResolveQuadooColor(Integer(L"BackColor")));
		SendMessageW(m_hwnd,LVM_SETTEXTBKCOLOR,0,ResolveQuadooColor(Integer(L"BackColor")));
		SendMessageW(m_hwnd,LVM_SETTEXTCOLOR,0,ResolveQuadooColor(Integer(L"ForeColor")));
		Check(PopulateColumns());
		Check(RefreshImages());
		Check(ApplyImageHandles());
		Check(PopulateItems());
		Check(PopulateVirtual());
		// Construction starts with an internal update count that InitializeControl
		// clears without EndUpdate. The new HWND is hidden already; suspending its
		// redraw here would leave redraw suspended and the header hidden on activation.
		// Preserve redraw suspension only for an explicit update on a ready control.
		if(m_updates && m_state == QuadooForm::Ready && Boolean(L"Visible"))
		{
			m_redrawBlocked = TRUE;
			SendMessageW(m_hwnd,WM_SETREDRAW,FALSE,0);
		}
		m_nativeDPI = m_dpi;
	Cleanup:
		return hr;
	}
	HRESULT CListView::Rebuild()
	{
		return IsVirtual() && m_hwnd && (GetWindowLongW(m_hwnd,GWL_STYLE) & LVS_OWNERDATA) ?
			VirtualNative() : ReplaceListWindow();
	}
	HRESULT CListView::ReplaceListWindow()
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		// Old HWNDs still borrow these until ReplaceNative commits or rolls back.
		TStackRef<IQuadooImageListSnapshot> retained[3];
		TStackRef<IQuadooImageListSnapshot> retainedVirtual[3];
		UINT previousVirtualStyle = m_virtualNativeStyle;
		UINT previousDPI = m_nativeDPI;
		HFONT previousFont = m_font;
		INT top = m_hwnd ? ListView_GetTopIndex(m_hwnd) : 0;
		for(UINT n = 0; n < ARRAYSIZE(retained); n++)
		{
			retained[n] = m_images[n].snapshot;
			retainedVirtual[n] = m_virtualImages[n];
		}
		m_busy++;
		m_font = NULL;
		hr = ReplaceNative();
		m_busy--;
		if(SUCCEEDED(hr) && top > 0 && top < rows.Length())
		{
			ListView_EnsureVisible(m_hwnd,top,FALSE);
		}
		if(FAILED(hr))
		{
			m_nativeDPI = previousDPI;
			for(UINT n = 0; n < ARRAYSIZE(retained); n++)
			{
				m_images[n].snapshot = retained[n];
				m_virtualImages[n] = retainedVirtual[n];
			}
			m_virtualNativeStyle = previousVirtualStyle;
			if(m_font)
			{
				DeleteObject(m_font);
			}
			m_font = previousFont;
			previousFont = NULL;
			SendMessageW(m_hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(m_font),FALSE);
		}
		if(previousFont)
		{
			DeleteObject(previousFont);
		}
		return hr;
	}
	HRESULT CListView::ApplyNative(PCWSTR name)
	{
		HRESULT hr;
		static PCWSTR const styles[] = {L"ViewStyle",L"ReadOnly",L"FullRowSelect",L"OwnerData",
			L"CheckBoxes",L"MultiSelect",L"HideSelection",L"GridLines",L"ShowColumnHeaders",L"ColumnClick",L"ColumnReorder"};
		for(UINT role = 0; role < ARRAYSIZE(m_images); role++)
		{
			static PCWSTR const names[] = {L"LargeImages",L"SmallImages",L"StateImages"};
			if(!lstrcmpW(name,names[role]))
			{
				Check(ResolveImages(role));
				goto Cleanup;
			}
		}
		for(UINT n = 0; n < ARRAYSIZE(styles); n++)
		{
			if(!lstrcmpW(name,styles[n]))
			{
				TArray<UINT> previous;
				CListItem* chosen = NULL;
				for(sysint row = 0; row < rows.Length(); row++)
				{
					if(rows[row]->selected && (!chosen || rows[row]->focused))
					{
						chosen = rows[row];
					}
					Check(previous.Append(INDEXTOSTATEIMAGEMASK(rows[row]->state + 1) |
						(rows[row]->selected ? LVIS_SELECTED : 0)));
				}
				for(sysint row = 0; row < rows.Length(); row++)
				{
					if(Boolean(L"CheckBoxes"))
					{
						rows[row]->state = max(0,rows[row]->state);
					}
					if(!Boolean(L"MultiSelect"))
					{
						rows[row]->selected = rows[row] == chosen;
					}
				}
				hr = Rebuild();
				if(FAILED(hr))
				{
					for(sysint row = 0; row < rows.Length(); row++)
					{
						rows[row]->state = static_cast<INT>((previous[row] & LVIS_STATEIMAGEMASK) >> 12) - 1;
						rows[row]->selected = (previous[row] & LVIS_SELECTED) != 0;
					}
					goto Cleanup;
				}
				goto Cleanup;
			}
		}
		Check(Super::ApplyNative(name));
		if(!lstrcmpW(name,L"BackColor") || !lstrcmpW(name,L"ForeColor"))
		{
			SendMessageW(m_hwnd,LVM_SETBKCOLOR,0,ResolveQuadooColor(Integer(L"BackColor")));
			SendMessageW(m_hwnd,LVM_SETTEXTBKCOLOR,0,ResolveQuadooColor(Integer(L"BackColor")));
			SendMessageW(m_hwnd,LVM_SETTEXTCOLOR,0,ResolveQuadooColor(Integer(L"ForeColor")));
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		Check(Mutable());
		CheckIf(m_editing,QuadooForm::EInvalidState);
		if(index == FindProperty(L"Columns"))
		{
			TStackRef<IJSONValue> current;
			CheckIf(type != QuadooProperty::Collection,DISP_E_TYPEMISMATCH);
			Check(WriteColumns(&current));
			CheckIfIgnore(!JSONCompare(current,value),S_FALSE);
			Check(LoadColumns(value));
			Check(NotifyColumns());
		}
		else if(index == FindProperty(L"Items"))
		{
			TStackRef<IJSONValue> current;
			CheckIf(type != QuadooProperty::Collection,DISP_E_TYPEMISMATCH);
			Check(WriteItems(&current));
			CheckIfIgnore(!JSONCompare(current,value),S_FALSE);
			Check(LoadItems(value));
			Check(NotifyItems());
		}
		else
		{
			if(index == FindProperty(L"CheckBoxes"))
			{
				bool enabled;
				TStackRef<IJSONValue> reference;
				CheckIf(!value,E_INVALIDARG);
				Check(value->GetBoolean(&enabled));
				Check(StoredValue(L"StateImages",&reference));
				CheckIf(enabled && reference,E_INVALIDARG);
				for(sysint n = 0; enabled && n < rows.Length(); n++)
				{
					CheckIf(rows[n]->state > 1,E_INVALIDARG);
				}
			}
			if(index == FindProperty(L"OwnerData"))
			{
				bool enabled;
				CheckIf(!value,E_INVALIDARG);
				Check(value->GetBoolean(&enabled));
				CheckIfIgnore(enabled == (IsVirtual() != FALSE),S_FALSE);
				CheckIf(enabled && rows.Length(),QuadooForm::EInvalidState);
				CheckIf(m_virtualCount || m_updates,QuadooForm::EInvalidState);
				CheckIf(m_generation == ~static_cast<ULONGLONG>(0),E_UNEXPECTED);
			}
			m_busy++;
			hr = Super::SetValue(index,type,value);
			m_busy--;
			if(SUCCEEDED(hr) && hr != S_FALSE && (index == FindProperty(L"OwnerData") ||
				(IsVirtual() && index == FindProperty(L"CheckBoxes"))))
			{
				m_generation++;
			}
			if(SUCCEEDED(hr))
			{
				Check(DrainVirtualStates());
			}
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::GetValueJSON(UINT index, IJSONValue** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckThread());
		if(index == FindProperty(L"Columns") && m_loaded)
		{
			if(m_headerPending && !m_busy)
			{
				Check(ReadNativeColumns());
			}
			Check(WriteColumns(value));
		}
		else if(index == FindProperty(L"Items") && m_loaded)
		{
			Check(WriteItems(value));
		}
		else
		{
			Check(Super::GetValueJSON(index,value));
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed)
	{
		HRESULT hr;
		if(m_nativeDPI != m_dpi)
		{
			Check(Rebuild());
		}
		Check(Super::ArrangeNative(bounds,changed));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::Arrange(const QuadooForm::Rect* bounds, UINT dpi)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		UINT previousDPI = m_dpi;
		QuadooForm::Rect previousBounds = m_effective;
		HFONT previousFont = NULL;
		HWND previousWindow = m_hwnd;
		Check(CheckAlive());
		CheckIf(IsVirtual() && (m_events || m_requests.Length()) && dpi != previousDPI,QuadooForm::EInvalidState);
		CheckIf(!bounds || dpi < 48 || dpi > 768 || bounds->Width < 0 || bounds->Height < 0,E_INVALIDARG);
		if(dpi != previousDPI)
		{
			// Consume pending native header edits while m_nativeDPI still
			// describes that HWND. Rebuilding first would lose the resize.
			Check(SyncHeader());
			// The old HWND still uses this font while its replacement is staged.
			previousFont = m_font;
			m_font = NULL;
		}
		hr = Super::Arrange(bounds,dpi);
		if(FAILED(hr) && m_hwnd == previousWindow)
		{
			m_dpi = previousDPI;
			m_effective = previousBounds;
			if(previousFont)
			{
				if(m_font)
				{
					DeleteObject(m_font);
				}
				m_font = previousFont;
				previousFont = NULL;
				SendMessageW(m_hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(m_font),FALSE);
			}
		}
	Cleanup:
		if(previousFont)
		{
			DeleteObject(previousFont);
		}
		return hr;
	}
	HRESULT CListView::DestroyChildren()
	{
		for(UINT n = 0; n < 3; n++)
		{
			if(m_hwnd)
			{
				ListView_SetImageList(m_hwnd,NULL,n == 0 ? LVSIL_NORMAL : n == 1 ? LVSIL_SMALL : LVSIL_STATE);
			}
			m_virtualImages[n].Release();
		}
		for(UINT n = 0; n < ARRAYSIZE(m_images); n++)
		{
			DetachImages(n);
		}
		if(m_link)
		{
			m_link->view = NULL;
		}
		ReleaseColumns(columns);
		ReleaseItems(rows);
		return S_OK;
	}
	HRESULT CListView::BeforeDestroy()
	{
		HRESULT hr = S_OK;
		// External providers cannot destroy a control midway through committing
		// a replacement HWND or image binding. Normal event destruction is valid.
		CheckIf(m_replacing || m_imageChanging || m_imageRefreshing,QuadooForm::EInvalidState);
		CheckIf(m_busy && !m_events,QuadooForm::EInvalidState);
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::BindEvents(IQuadooConstructionContext* context)
	{
		HRESULT hr;
		Check(CheckAlive());
		for(UINT n = 0; n < ARRAYSIZE(m_images); n++)
		{
			Check(ResolveImages(n));
		}
		Check(Super::BindEvents(context));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::GetColumns(IQuadooListColumns** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckAlive());
		*value = m_collection;
		(*value)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::BeginUpdate(VOID)
	{
		HRESULT hr;
		Check(Mutable());
		CheckIf(m_updates == static_cast<UINT>(-1),E_UNEXPECTED);
		m_updates++;
		if(m_updates == 1)
		{
			m_redrawBlocked = (GetWindowLongW(m_hwnd,GWL_STYLE) & WS_VISIBLE) != 0;
			if(m_redrawBlocked)
			{
				SendMessageW(m_hwnd,WM_SETREDRAW,FALSE,0);
			}
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::EndUpdate(VOID)
	{
		HRESULT hr;
		Check(Mutable());
		CheckIf(!m_updates,QuadooForm::EInvalidState);
		m_updates--;
		if(!m_updates)
		{
			if(m_redrawBlocked)
			{
				SendMessageW(m_hwnd,WM_SETREDRAW,TRUE,0);
				m_redrawBlocked = FALSE;
				if(!Boolean(L"Visible"))
				{
					ShowWindow(m_hwnd,SW_HIDE);
				}
			}
			InvalidateRect(m_hwnd,NULL,TRUE);
			Check(FlushNotifications());
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CListView::Refresh(VOID)
	{
		HRESULT hr;
		Check(Mutable());
		Check(RefreshImages());
		if(IsVirtual())
		{
			CheckIf(m_generation == ~static_cast<ULONGLONG>(0),E_UNEXPECTED);
			m_generation++;
		}
		InvalidateRect(m_hwnd,NULL,TRUE);
	Cleanup:
		return hr;
	}
}
