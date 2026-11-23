#include "Runtime.h"

namespace QF
{
CTabControl::CTabControl(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
	TContainerInterface(factory,context,parent), m_active(NULL), m_selecting(FALSE), m_syncTabs(FALSE),
	m_structure(FALSE), m_autoSelect(TRUE), m_nativeApproved(FALSE), m_pageGeneration(0) {}
CTabControl::~CTabControl() { ClearPages(); }
VOID CTabControl::ClearPages()
{
	m_active = NULL;
	for(sysint n = 0; n < m_pages.Length(); n++) { m_pages[n].page->Release(); RStrRelease(m_pages[n].caption); }
	m_pages.Clear();
}
INT CTabControl::PageIndex(IQuadooTabPage* page) const
{
	for(sysint n = 0; n < m_pages.Length(); n++) if(SameObject(page,m_pages[n].page)) return static_cast<INT>(n);
	return -1;
}
INT CTabControl::HeaderIndex(INT page) const
{
	INT header = 0;
	if(page < 0 || page >= m_pages.Length() || !m_pages[page].tabVisible) return -1;
	for(INT n = 0; n < page; n++) if(m_pages[n].tabVisible) header++;
	return header;
}
INT CTabControl::HeaderPage(INT header) const
{
	if(header < 0) return -1;
	for(INT n = 0; n < m_pages.Length(); n++) if(m_pages[n].tabVisible && header-- == 0) return n;
	return -1;
}
INT CTabControl::NextPage(INT from, BOOL forward) const
{
	INT count = static_cast<INT>(m_pages.Length()), index = from;
	if(index < 0) index = forward ? -1 : 0;
	for(INT n = 0; n < count; n++)
	{
		index = (index + count + (forward ? 1 : -1)) % count;
		if(m_pages[index].tabVisible) return index;
	}
	return -1;
}
HRESULT CTabControl::ReadPage(IQuadooControl* control, Page* page)
{
	HRESULT hr;
	TStackRef<IQuadooTabPage> capability; TStackRef<IJSONValue> value;
	CRString caption;
	ZeroMemory(page,sizeof(*page));
	Check(control->QueryInterface(__uuidof(IQuadooTabPage),reinterpret_cast<PVOID*>(&capability)));
	Check(LayoutProperty(control,L"Caption",&value)); CheckIf(!value,QuadooForm::EInvalidDefinition); Check(value->GetString(&caption));
	CheckIf(!RStrIsWide(static_cast<RSTRING>(caption)),E_INVALIDARG);
	for(INT n = 0; n < RStrLen(caption); n++) { CheckIf(!caption.c_wstr()[n],E_INVALIDARG); }
	Check(capability->GetTabVisible(&page->tabVisible));
	page->page = capability.Detach(); page->caption = caption.Detach();
Cleanup:
	return hr;
}
HRESULT CTabControl::InsertHeader(INT header, const Page& page)
{
	HRESULT hr = S_OK;
	TCITEMW item; ZeroMemory(&item,sizeof(item)); item.mask = TCIF_TEXT;
	item.pszText = const_cast<PWSTR>(RStrToWide(page.caption));
	BOOL previous = m_syncTabs; m_syncTabs = TRUE;
	if(m_hwnd && SendMessageW(m_hwnd,TCM_INSERTITEMW,header,reinterpret_cast<LPARAM>(&item)) != header) hr = E_OUTOFMEMORY;
	m_syncTabs = previous;
	return hr;
}
VOID CTabControl::SyncSelection()
{
	BOOL previous = m_syncTabs; m_syncTabs = TRUE;
	if(m_hwnd) SendMessageW(m_hwnd,TCM_SETCURSEL,HeaderIndex(PageIndex(m_active)),0);
	m_syncTabs = previous;
}
HRESULT CTabControl::IndexValue(INT index, IJSONValue** value) { return JSONCreateInteger(index,value); }
HRESULT CTabControl::CommitIndex(IQuadooTabPage* page, IJSONValue* index)
{
	HRESULT hr;
	TStackRef<IJSONValue> old;
	Check(StoredValue(L"ActivePageIndex",&old));
	m_active = page;
	if(JSONCompare(old,index)) { Check(SetStoredValue(L"ActivePageIndex",index)); Check(Dirty(L"ActivePageIndex")); }
	SyncSelection();
Cleanup:
	return hr;
}
VOID CTabControl::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended)
{
	CContainer::NativeStyle(className,style,extended); *className = WC_TABCONTROLW;
	*style |= WS_TABSTOP | TCS_TABS;
	if(Boolean(L"MultiLine")) *style |= TCS_MULTILINE;
	if(Boolean(L"HotTrack")) *style |= TCS_HOTTRACK;
}
HRESULT CTabControl::CreateNative()
{
	HRESULT hr;
	INITCOMMONCONTROLSEX controls = { sizeof(controls),ICC_TAB_CLASSES };
	TStackRef<IJSONObject> properties; TStackRef<IJSONValue> value;
	CheckIf(!InitCommonControlsEx(&controls),HrEnsureLastError());
	if(m_definition->HasField(RSTRING_CAST(L"properties")))
	{
		Check(m_definition->FindNonNullValueW(L"properties",&value)); Check(value->GetObject(&properties));
		m_autoSelect = !properties->HasField(RSTRING_CAST(L"ActivePageIndex"));
	}
	Check(CContainer::CreateNative());
Cleanup:
	return hr;
}
STDMETHODIMP CTabControl::BindEvents(IQuadooConstructionContext* context)
{
	HRESULT hr;
	Check(CContainer::BindEvents(context));
	INT index = Integer(L"ActivePageIndex");
	if(m_autoSelect && index == -1) index = NextPage(-1,TRUE);
	CheckIf(index < -1 || index >= m_pages.Length(),E_INVALIDARG);
	Check(SelectPage(index,FALSE));
Cleanup:
	return hr;
}
STDMETHODIMP CTabControl::RegisterControl(IQuadooControl* control)
{
	HRESULT hr;
	Page entry; ZeroMemory(&entry,sizeof(entry));
	TStackRef<CControl> keepAlive(this); TStackRef<IJSONValue> firstIndex;
	BOOL inserted = FALSE, guarded = FALSE, committed = FALSE;
	INT header = 0;
	Check(CheckAlive()); CheckIf(m_selecting || m_structure || m_registering,QuadooForm::EInvalidState);
	CheckIf(!control,E_INVALIDARG); Check(ReadPage(control,&entry));
	CheckIf(m_pages.Length() >= 4096,E_INVALIDARG);
	Check(m_pages.Reserve(1));
	for(sysint n = 0; n < m_pages.Length(); n++) if(m_pages[n].tabVisible) header++;
	if(m_state == QuadooForm::Ready && !m_active && entry.tabVisible) { Check(IndexValue(static_cast<INT>(m_pages.Length()),&firstIndex)); }
	m_structure = TRUE; guarded = TRUE;
	if(entry.tabVisible) { Check(InsertHeader(header,entry)); inserted = TRUE; SyncSelection(); }
	{
		// Establish the page viewport before a dynamic page's children are built.
		// Candidate layout can fail without changing container ownership/state.
		QuadooForm::Rect bounds; TStackRef<IQuadooLayoutControl> layout;
		Check(PageBounds(&bounds)); Check(QueryOptional(control,&layout));
		if(layout) { Check(layout->Arrange(&bounds,m_dpi)); }
		else { HWND window; Check(entry.page->GetHandle(&window)); CheckIf(!SetWindowPos(window,NULL,MulDiv(bounds.Left,m_dpi,96),MulDiv(bounds.Top,m_dpi,96),MulDiv(bounds.Width,m_dpi,96),MulDiv(bounds.Height,m_dpi,96),SWP_NOZORDER | SWP_NOACTIVATE),HrEnsureLastError()); }
	}
	Check(CContainer::RegisterControl(control));
	// Reserve completed before extension callbacks; this append cannot allocate.
	Check(m_pages.Append(entry)); entry.page = NULL; entry.caption = NULL;
	committed = TRUE;
	m_pageGeneration++;
	if(firstIndex) { Check(CommitIndex(m_pages[m_pages.Length() - 1].page,firstIndex)); }
Cleanup:
	if(FAILED(hr) && !committed && inserted && m_hwnd)
	{ m_syncTabs = TRUE; SendMessageW(m_hwnd,TCM_DELETEITEM,header,0); m_syncTabs = FALSE; SyncSelection(); }
	if(guarded) m_structure = FALSE;
	SafeRelease(entry.page); RStrRelease(entry.caption);
	return hr;
}
STDMETHODIMP CTabControl::UnregisterControl(IQuadooControl* control)
{
	HRESULT hr;
	TStackRef<IQuadooTabPage> page; TStackRef<CControl> keepAlive(this);
	TStackRef<IJSONValue> value;
	BOOL guarded = FALSE;
	Check(CheckThread()); CheckIf(!control,E_INVALIDARG);
	Check(control->QueryInterface(__uuidof(IQuadooTabPage),reinterpret_cast<PVOID*>(&page)));
	INT index = PageIndex(page), header = HeaderIndex(index);
	CheckIf(index < 0,HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	CheckIf(m_structure || m_registering,QuadooForm::EInvalidState);
	INT active = PageIndex(m_active);
	if(active == index)
	{
		active = -1;
		if(m_state < QuadooForm::Destroying)
			for(INT n = 1, count = static_cast<INT>(m_pages.Length()); n < count; n++)
			{ INT candidate = (index + n) % count; if(m_pages[candidate].tabVisible) { active = candidate; break; } }
	}
	IQuadooTabPage* replacement = active < 0 ? NULL : m_pages[active].page;
	Check(IndexValue(active < 0 ? -1 : active - (active > index ? 1 : 0),&value));
	m_structure = TRUE; guarded = TRUE;
	hr = CContainer::UnregisterControl(control);
	// A layout callback error may be returned after the base removed the child.
	for(sysint n = 0; n < m_children.Length(); n++) if(SameObject(control,m_children[n].control)) goto Cleanup;
	if(index >= m_pages.Length() || !SameObject(page,m_pages[index].page)) goto Cleanup;
	if(header >= 0 && m_hwnd) { m_syncTabs = TRUE; SendMessageW(m_hwnd,TCM_DELETEITEM,header,0); m_syncTabs = FALSE; }
	{
		Page removed = m_pages[index]; m_pages.Remove(index,NULL);
		m_pageGeneration++;
		HRESULT commit = CommitIndex(replacement,value); if(FAILED(commit) && SUCCEEDED(hr)) hr = commit;
		removed.page->Release(); RStrRelease(removed.caption);
	}
	m_structure = FALSE; guarded = FALSE;
	if(m_state < QuadooForm::Destroying)
	{ HRESULT layout = UpdateLayout(); if(FAILED(layout) && SUCCEEDED(hr)) hr = layout; }
Cleanup:
	if(guarded) m_structure = FALSE;
	return hr;
}
HRESULT CTabControl::DestroyChildren()
{
	HRESULT hr = CContainer::DestroyChildren();
	ClearPages(); return hr;
}
HRESULT CTabControl::ShowPages()
{
	HRESULT hr = S_OK;
	TArray<IQuadooTabPage*> pages;
	BOOL moveFocus = FALSE;
	for(sysint n = 0; n < m_pages.Length(); n++) { Check(pages.Append(m_pages[n].page)); m_pages[n].page->AddRef(); }
	for(sysint n = 0; n < pages.Length() && m_state < QuadooForm::Destroying; n++)
	{
		HWND hwnd; BOOL visible; QuadooForm::ComponentState state;
		Check(pages[n]->GetState(&state)); if(state != QuadooForm::Ready) continue;
		if(PageIndex(pages[n]) < 0) continue;
		Check(pages[n]->GetVisible(&visible)); Check(pages[n]->GetHandle(&hwnd));
		BOOL show = visible && SameObject(pages[n],m_active);
		if(!show && (GetFocus() == hwnd || IsChild(hwnd,GetFocus())))
		{ INT index = PageIndex(pages[n]); if(index >= 0) m_pages[index].focus = GetFocus(); moveFocus = TRUE; }
		if(((GetWindowLongW(hwnd,GWL_STYLE) & WS_VISIBLE) != 0) != show) ::ShowWindow(hwnd,show ? SW_SHOWNA : SW_HIDE);
	}
	if(moveFocus && m_state < QuadooForm::Destroying)
	{
		HWND focus = NULL, pageWindow; INT current = PageIndex(m_active); BOOL enabled, visible;
		if(current >= 0 && SUCCEEDED(m_active->GetHandle(&pageWindow)) && SUCCEEDED(m_active->GetEnabled(&enabled)) && enabled && SUCCEEDED(m_active->GetVisible(&visible)) && visible)
		{
			focus = m_pages[current].focus;
			if(!focus || !IsWindow(focus) || !IsChild(pageWindow,focus) || !IsWindowEnabled(focus) || !IsWindowVisible(focus)) focus = GetNextDlgTabItem(pageWindow,NULL,FALSE);
		}
		::SetFocus(focus ? focus : m_hwnd);
	}
Cleanup:
	for(sysint n = 0; n < pages.Length(); n++) pages[n]->Release();
	return hr;
}
HRESULT CTabControl::PageBounds(QuadooForm::Rect* bounds)
{
	HRESULT hr;
	RECT rect; QuadooForm::Insets padding;
	CheckIf(!GetClientRect(m_hwnd,&rect),HrEnsureLastError()); SendMessageW(m_hwnd,TCM_ADJUSTRECT,FALSE,reinterpret_cast<LPARAM>(&rect));
	Check(Insets(L"Padding",&padding));
	bounds->Left = MulDiv(rect.left,96,m_dpi) + padding.Left; bounds->Top = MulDiv(rect.top,96,m_dpi) + padding.Top;
	bounds->Width = max(0,MulDiv(rect.right - rect.left,96,m_dpi) - padding.Left - padding.Right);
	bounds->Height = max(0,MulDiv(rect.bottom - rect.top,96,m_dpi) - padding.Top - padding.Bottom);
Cleanup:
	return hr;
}
HRESULT CTabControl::LayoutChildren()
{
	HRESULT hr;
	QuadooForm::Rect bounds;
	TArray<IQuadooTabPage*> pages;
	CheckIfIgnore(m_structure,S_FALSE); Check(PageBounds(&bounds));
	m_contentSize.Width = bounds.Width; m_contentSize.Height = bounds.Height;
	for(sysint n = 0; n < m_pages.Length(); n++) { Check(pages.Append(m_pages[n].page)); m_pages[n].page->AddRef(); }
	for(sysint n = 0; n < pages.Length() && m_state < QuadooForm::Destroying; n++)
	{
		QuadooForm::ComponentState state; Check(pages[n]->GetState(&state));
		if(state >= QuadooForm::Destroying || PageIndex(pages[n]) < 0) continue;
		TStackRef<IQuadooLayoutControl> layout;
		Check(QueryOptional(pages[n],&layout));
		if(layout) { Check(layout->Arrange(&bounds,m_dpi)); }
		else { HWND hwnd; Check(pages[n]->GetHandle(&hwnd)); CheckIf(!SetWindowPos(hwnd,NULL,MulDiv(bounds.Left,m_dpi,96),MulDiv(bounds.Top,m_dpi,96),MulDiv(bounds.Width,m_dpi,96),MulDiv(bounds.Height,m_dpi,96),SWP_NOZORDER | SWP_NOACTIVATE),HrEnsureLastError()); }
	}
	if(m_state < QuadooForm::Destroying) { Check(ShowPages()); }
Cleanup:
	for(sysint n = 0; n < pages.Length(); n++) pages[n]->Release();
	return hr;
}
HRESULT CTabControl::SelectPage(INT index, BOOL user, BOOL approved)
{
	HRESULT hr;
	TStackRef<CControl> keepAlive(this); TStackRef<IQuadooTabPage> incoming;
	TStackRef<IJSONValue> value;
	BOOL entered = FALSE, cancelled = FALSE;
	UINT generation = m_pageGeneration;
	Check(CheckAlive()); CheckIf(index < -1 || index >= m_pages.Length(),E_INVALIDARG);
	if(index >= 0) incoming = m_pages[index].page;
	CheckIfIgnore(SameObject(incoming,m_active) || (!incoming && !m_active && Integer(L"ActivePageIndex") == -1),S_FALSE);
	CheckIf(m_selecting || m_structure,QuadooForm::EInvalidState);
	Check(IndexValue(index,&value));
	m_selecting = TRUE; entered = TRUE;
	if(user && !approved && m_state == QuadooForm::Ready) { Check(Dispatch(L"OnChanging",&cancelled)); }
	CheckIfIgnore(m_state >= QuadooForm::Destroying || cancelled || generation != m_pageGeneration,S_FALSE);
	if(incoming) { QuadooForm::ComponentState state; Check(incoming->GetState(&state)); CheckIf(state >= QuadooForm::Destroying || PageIndex(incoming) < 0,QuadooForm::EDestroyed); }
	Check(CommitIndex(incoming,value));
	// Initial binding precedes the root's first layout. Arranging descendants
	// here would capture anchor baselines against a temporary tab-control size.
	if(m_state >= QuadooForm::Initializing) { Check(UpdateLayout()); }
	Check(FlushNotifications());
	if(user && m_state == QuadooForm::Ready && SameObject(incoming,m_active)) { Check(Dispatch(L"OnChange")); }
	hr = S_OK;
Cleanup:
	if(entered) m_selecting = FALSE;
	return hr;
}
HRESULT CTabControl::Navigate(BOOL forward, BOOL user)
{
	HRESULT hr;
	Check(CheckAlive()); INT index = NextPage(PageIndex(m_active),forward);
	CheckIfIgnore(index < 0,S_FALSE); Check(SelectPage(index,user));
Cleanup:
	return hr;
}
STDMETHODIMP CTabControl::GetPageCount(UINT* count) { return GetChildCount(count); }
STDMETHODIMP CTabControl::GetPage(UINT index, IQuadooTabPage** page)
{
	HRESULT hr;
	CheckIf(!page,E_POINTER); *page = NULL; Check(CheckThread());
	CheckIf(index >= static_cast<UINT>(m_pages.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
	*page = m_pages[index].page; (*page)->AddRef(); hr = S_OK;
Cleanup:
	return hr;
}
STDMETHODIMP CTabControl::GetActivePage(IQuadooTabPage** page)
{
	HRESULT hr;
	CheckIf(!page,E_POINTER); *page = NULL; Check(CheckThread());
	*page = m_active; if(*page) (*page)->AddRef(); hr = S_OK;
Cleanup:
	return hr;
}
STDMETHODIMP CTabControl::GetActivePageIndex(INT* index)
{
	HRESULT hr;
	CheckIf(!index,E_POINTER); *index = -1; Check(CheckThread()); *index = PageIndex(m_active); hr = S_OK;
Cleanup:
	return hr;
}
STDMETHODIMP CTabControl::SetActivePage(IQuadooTabPage* page)
{
	HRESULT hr;
	Check(CheckAlive()); INT index = PageIndex(page); CheckIf(page && index < 0,E_INVALIDARG); Check(SelectPage(index,FALSE));
Cleanup:
	return hr;
}
STDMETHODIMP CTabControl::SetActivePageIndex(INT index) { return SelectPage(index,FALSE); }
HRESULT CTabControl::SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value)
{
	HRESULT hr;
	Check(CheckAlive()); CheckIf(index >= static_cast<UINT>(m_values.Length()),DISP_E_MEMBERNOTFOUND);
	if(0 == TStrCmpAssert(m_values[index].spec->name,L"ActivePageIndex"))
	{
		INT active; TStackRef<IJSONValue> normalized;
		CheckIf(type != QuadooProperty::Int,DISP_E_TYPEMISMATCH); Check(NormalizeValue(m_values[index].spec,value,&normalized)); Check(normalized->GetInteger(&active)); Check(SelectPage(active,FALSE));
	}
	else { Check(CContainer::SetValue(index,type,value)); }
Cleanup:
	return hr;
}
HRESULT CTabControl::ApplyNative(PCWSTR name)
{
	HRESULT hr;
	Check(CContainer::ApplyNative(name));
	if(m_hwnd && (0 == TStrCmpAssert(name,L"MultiLine") || 0 == TStrCmpAssert(name,L"HotTrack")))
	{
		LONG style = GetWindowLongW(m_hwnd,GWL_STYLE) & ~(TCS_MULTILINE | TCS_HOTTRACK);
		if(Boolean(L"MultiLine")) style |= TCS_MULTILINE; if(Boolean(L"HotTrack")) style |= TCS_HOTTRACK;
		SetWindowLongW(m_hwnd,GWL_STYLE,style); InvalidateRect(m_hwnd,NULL,TRUE);
	}
Cleanup:
	return hr;
}
STDMETHODIMP CTabControl::UpdatePage(IQuadooTabPage* page)
{
	HRESULT hr;
	Page replacement; ZeroMemory(&replacement,sizeof(replacement));
	TStackRef<CControl> keepAlive(this); TStackRef<IJSONValue> value;
	BOOL entered = FALSE;
	Check(CheckAlive()); CheckIf(!page,E_INVALIDARG);
	INT index = PageIndex(page); CheckIfIgnore(index < 0,S_FALSE);
	Check(ReadPage(page,&replacement));
	Page& previous = m_pages[index]; INT header = HeaderIndex(index);
	BOOL captionChanged = !SameString(previous.caption,replacement.caption);
	CheckIfIgnore(!captionChanged && previous.tabVisible == replacement.tabVisible,S_FALSE);
	CheckIf(m_structure || m_selecting || m_registering,QuadooForm::EInvalidState);
	INT active = PageIndex(m_active);
	if(active == index && previous.tabVisible && !replacement.tabVisible)
	{ active = NextPage(index,TRUE); if(active == index) active = -1; }
	Check(IndexValue(active,&value));
	m_structure = TRUE; entered = TRUE;
	if(previous.tabVisible && replacement.tabVisible)
	{
		TCITEMW item; ZeroMemory(&item,sizeof(item)); item.mask = TCIF_TEXT; item.pszText = const_cast<PWSTR>(RStrToWide(replacement.caption));
		m_syncTabs = TRUE; LRESULT changed = SendMessageW(m_hwnd,TCM_SETITEMW,header,reinterpret_cast<LPARAM>(&item)); m_syncTabs = FALSE;
		CheckIf(!changed,E_OUTOFMEMORY);
	}
	else if(replacement.tabVisible)
	{
		header = 0; for(INT n = 0; n < index; n++) if(m_pages[n].tabVisible) header++;
		Check(InsertHeader(header,replacement));
	}
	else if(previous.tabVisible && m_hwnd)
	{ m_syncTabs = TRUE; LRESULT removed = SendMessageW(m_hwnd,TCM_DELETEITEM,header,0); m_syncTabs = FALSE; CheckIf(!removed,E_FAIL); }
	RStrRelease(previous.caption); previous.caption = replacement.caption; replacement.caption = NULL; previous.tabVisible = replacement.tabVisible;
	m_pageGeneration++;
	Check(CommitIndex(active < 0 ? NULL : m_pages[active].page,value));
Cleanup:
	if(entered) m_structure = FALSE;
	SafeRelease(replacement.page); RStrRelease(replacement.caption);
	return hr;
}
STDMETHODIMP CTabControl::MovePage(IQuadooTabPage* page, UINT destination)
{
	HRESULT hr;
	TStackRef<CControl> keepAlive(this); TStackRef<IJSONValue> value;
	Page moving; Child child;
	BOOL guarded = FALSE;
	Check(CheckAlive()); INT source = PageIndex(page);
	CheckIf(source < 0 || destination >= static_cast<UINT>(m_pages.Length()),E_INVALIDARG);
	CheckIfIgnore(source == static_cast<INT>(destination),S_FALSE); CheckIf(m_selecting || m_structure || m_registering,QuadooForm::EInvalidState);
	INT active = PageIndex(m_active), target = static_cast<INT>(destination);
	if(active == source) active = target; else if(source < active && target >= active) active--; else if(source > active && target <= active) active++;
	Check(IndexValue(active,&value)); Check(m_pages.Reserve(1)); Check(m_children.Reserve(1));
	moving = m_pages[source]; child = m_children[source];
	INT oldHeader = HeaderIndex(source), newHeader = 0;
	if(moving.tabVisible)
	{
		for(INT n = 0; n < target; n++) if(m_pages[n].tabVisible) newHeader++;
		if(target > source && m_pages[target].tabVisible) newHeader++;
	}
	m_structure = TRUE; guarded = TRUE;
	if(moving.tabVisible)
	{
		Check(InsertHeader(newHeader,moving));
		m_syncTabs = TRUE; SendMessageW(m_hwnd,TCM_DELETEITEM,oldHeader + (newHeader <= oldHeader ? 1 : 0),0); m_syncTabs = FALSE;
	}
	m_pages.Remove(source,NULL); Check(m_pages.InsertAt(&moving,target));
	m_children.Remove(source,NULL); Check(m_children.InsertAt(&child,target));
	m_pageGeneration++;
	Check(CommitIndex(m_active,value));
	m_structure = FALSE; guarded = FALSE; Check(UpdateLayout()); Check(FlushNotifications());
Cleanup:
	if(guarded) m_structure = FALSE;
	return hr;
}
STDMETHODIMP CTabControl::OnPropertyChanged(IQuadooComponent* component, UINT index)
{
	HRESULT hr;
	TStackRef<IQuadooTabPage> page;
	Check(CheckAlive()); CheckIf(!component,E_INVALIDARG);
	Check(component->QueryInterface(__uuidof(IQuadooTabPage),reinterpret_cast<PVOID*>(&page)));
	Check(UpdatePage(page)); Check(CContainer::OnPropertyChanged(component,index));
Cleanup:
	return hr;
}
STDMETHODIMP CTabControl::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result)
{
	HRESULT hr = S_OK;
	CheckIf(!handled || !result,E_POINTER); *handled = FALSE; *result = 0; Check(CheckAlive());
	if(hwnd == m_hwnd && msg == WM_NOTIFY && lp)
	{
		NMHDR* notification = reinterpret_cast<NMHDR*>(lp);
		if(notification->hwndFrom == m_hwnd && (notification->code == TCN_SELCHANGING || notification->code == TCN_SELCHANGE))
		{
			*handled = TRUE;
			if(m_syncTabs || m_state != QuadooForm::Ready) goto Cleanup;
			if(notification->code == TCN_SELCHANGING)
			{
				BOOL cancel = FALSE; *result = 1; m_nativeApproved = FALSE;
				UINT generation = m_pageGeneration;
				CheckIf(m_selecting || m_structure,QuadooForm::EInvalidState);
				m_selecting = TRUE; hr = Dispatch(L"OnChanging",&cancel); m_selecting = FALSE; Check(hr);
				if(!cancel && m_state == QuadooForm::Ready && generation == m_pageGeneration) { m_nativeApproved = TRUE; *result = 0; }
			}
			else
			{
				BOOL approved = m_nativeApproved; m_nativeApproved = FALSE;
				hr = SelectPage(HeaderPage(static_cast<INT>(SendMessageW(m_hwnd,TCM_GETCURSEL,0,0))),TRUE,approved);
				if(hr != S_OK) SyncSelection();
			}
		}
	}
	else hr = CContainer::HandleMessage(hwnd,msg,wp,lp,handled,result);
Cleanup:
	return hr;
}
LRESULT CTabControl::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
{
	if(!m_syncTabs && m_state < QuadooForm::Destroying)
	{
		if(msg == TCM_SETCURSEL)
		{
			INT previous = HeaderIndex(PageIndex(m_active)), index = HeaderPage(static_cast<INT>(wp)); *handled = TRUE;
			if(static_cast<INT>(wp) != -1 && index < 0) return -1;
			HRESULT hr = SelectPage(index,FALSE); ReportCallback(hr,L"Page selection"); return SUCCEEDED(hr) ? previous : -1;
		}
		if(msg == TCM_INSERTITEMW || msg == TCM_INSERTITEMA || msg == TCM_DELETEITEM || msg == TCM_DELETEALLITEMS || msg == TCM_SETITEMW || msg == TCM_SETITEMA)
		{ *handled = TRUE; return msg == TCM_INSERTITEMW || msg == TCM_INSERTITEMA ? -1 : FALSE; }
	}
	if(msg == WM_KEYDOWN && m_state == QuadooForm::Ready && (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_HOME || wp == VK_END || (wp == VK_TAB && (GetKeyState(VK_CONTROL) & 0x8000))))
	{
		CWinControl::Message(hwnd,msg,wp,lp,content,handled); if(*handled) return 0;
		*handled = TRUE; HRESULT hr;
		if(wp == VK_HOME || wp == VK_END) hr = SelectPage(NextPage(-1,wp == VK_HOME),TRUE);
		else hr = Navigate(wp == VK_RIGHT || (wp == VK_TAB && !(GetKeyState(VK_SHIFT) & 0x8000)),TRUE);
		ReportCallback(hr,L"Page navigation"); return 0;
	}
	// Let the real common control paint its headers and border. The pages paint
	// themselves; generic container painting would cover the native tab strip.
	if(msg == WM_PAINT || msg == WM_PRINTCLIENT || msg == WM_ERASEBKGND) return CWinControl::Message(hwnd,msg,wp,lp,content,handled);
	return CContainer::Message(hwnd,msg,wp,lp,content,handled);
}
BOOL CTabPage::AcceptParent(IQuadooControlContainer* parent)
{
	TStackRef<IQuadooTabControl> tabs;
	return parent && SUCCEEDED(parent->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs)));
}
STDMETHODIMP CTabPage::GetPageIndex(INT* index)
{
	HRESULT hr;
	TStackRef<IQuadooTabControl> tabs;
	CheckIf(!index,E_POINTER); *index = -1; Check(CheckThread());
	if(m_parent)
	{
		UINT count; Check(m_parent->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))); Check(tabs->GetPageCount(&count));
		for(UINT n = 0; n < count; n++) { TStackRef<IQuadooTabPage> page; Check(tabs->GetPage(n,&page)); if(SameObject(page,Control())) { *index = static_cast<INT>(n); break; } }
	}
	hr = S_OK;
Cleanup:
	return hr;
}
STDMETHODIMP CTabPage::SetPageIndex(UINT index)
{
	HRESULT hr; TStackRef<IQuadooTabControl> tabs;
	Check(CheckAlive()); CheckIf(!m_parent,QuadooForm::EInvalidState);
	Check(m_parent->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))); Check(tabs->MovePage(this,index));
Cleanup:
	return hr;
}
STDMETHODIMP CTabPage::GetTabVisible(BOOL* visible)
{
	HRESULT hr; CheckIf(!visible,E_POINTER); *visible = FALSE; Check(CheckThread()); *visible = Boolean(L"TabVisible"); hr = S_OK;
Cleanup:
	return hr;
}
STDMETHODIMP CTabPage::SetTabVisible(BOOL visible)
{
	HRESULT hr; TStackRef<IJSONValue> value;
	Check(CheckAlive()); Check(JSONCreateBool(visible != FALSE,&value)); Check(SetNamedValue(L"TabVisible",QuadooProperty::Boolean,value));
Cleanup:
	return hr;
}
HRESULT CTabPage::ApplyNative(PCWSTR name)
{
	HRESULT hr;
	TStackRef<IQuadooTabControl> tabs;
	if(m_parent && (0 == TStrCmpAssert(name,L"Caption") || 0 == TStrCmpAssert(name,L"TabVisible")))
	{ Check(m_parent->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))); Check(tabs->UpdatePage(this)); }
	if(0 == TStrCmpAssert(name,L"Visible"))
	{
		TStackRef<IQuadooTabPage> active;
		if(!tabs && m_parent) { Check(m_parent->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))); }
		if(tabs) { Check(tabs->GetActivePage(&active)); }
		if(m_hwnd && m_state == QuadooForm::Ready) ::ShowWindow(m_hwnd,Boolean(L"Visible") && SameObject(active,Control()) ? SW_SHOWNA : SW_HIDE);
		hr = S_OK;
	}
	else if(0 == TStrCmpAssert(name,L"Caption") || 0 == TStrCmpAssert(name,L"TabVisible")) hr = S_OK;
	else { Check(CContainer::ApplyNative(name)); }
Cleanup:
	return hr;
}
HRESULT CTabPage::PropertyChanged(PCWSTR name)
{
	return m_parent ? m_parent->UpdateLayout() : S_OK;
}
}
