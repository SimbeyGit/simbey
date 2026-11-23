#include "Runtime.h"

namespace QF
{
HRESULT LayoutProperty(IQuadooControl* control, PCWSTR name, IJSONValue** value)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooPropertyDescriptors> descriptors;
	UINT count;
	CheckIf(!value, E_POINTER); *value = NULL;
	Check(control->GetProperties(&properties)); CheckIf(!properties, QuadooForm::EInvalidDefinition);
	Check(properties->GetDescriptors(&descriptors)); CheckIf(!descriptors, QuadooForm::EInvalidDefinition); Check(descriptors->GetCount(&count));
	CheckIf(count > 4096, QuadooForm::EInvalidDefinition);
	for(UINT n = 0; n < count; n++)
	{
		TStackRef<IQuadooPropertyDescriptor> descriptor; CRString text;
		Check(descriptors->GetAt(n, &descriptor)); CheckIf(!descriptor, QuadooForm::EInvalidDefinition); Check(descriptor->GetName(&text));
		if(SameString(text, RSTRING_CAST(name))) { Check(properties->GetValueJSON(n, value)); goto Cleanup; }
	}
	hr = DISP_E_MEMBERNOTFOUND;
Cleanup:
	return hr;
}
INT LayoutInt(IQuadooControl* control, PCWSTR name, INT initial)
{
	TStackRef<IJSONValue> value;
	INT result = initial;
	if(SUCCEEDED(LayoutProperty(control, name, &value))) value->GetInteger(&result);
	return result;
}
static BOOL LayoutBool(IQuadooControl* control, PCWSTR name)
{
	TStackRef<IJSONValue> value;
	bool result = false;
	if(SUCCEEDED(LayoutProperty(control, name, &value))) value->GetBoolean(&result);
	return result;
}
static HRESULT LayoutMargins(IQuadooControl* control, QuadooForm::Insets* margins)
{
	HRESULT hr = S_OK;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONObject> object;
	ZeroMemory(margins, sizeof(*margins));
	if(SUCCEEDED(LayoutProperty(control, L"Margins", &value)))
	{
		Check(value->GetObject(&object));
		Check(ReadInt(object, L"Left", 0, &margins->Left)); Check(ReadInt(object, L"Top", 0, &margins->Top));
		Check(ReadInt(object, L"Right", 0, &margins->Right)); Check(ReadInt(object, L"Bottom", 0, &margins->Bottom));
	}
Cleanup:
	return hr;
}
static VOID Constrain(IQuadooControl* control, QuadooForm::Rect* rect)
{
	INT minWidth = LayoutInt(control, L"MinWidth", 0), minHeight = LayoutInt(control, L"MinHeight", 0);
	INT maxWidth = LayoutInt(control, L"MaxWidth", 0), maxHeight = LayoutInt(control, L"MaxHeight", 0);
	rect->Width = max(minWidth, rect->Width); rect->Height = max(minHeight, rect->Height);
	if(maxWidth) rect->Width = min(maxWidth, rect->Width); if(maxHeight) rect->Height = min(maxHeight, rect->Height);
}


HRESULT CControl::RequestLayout()
{
	HRESULT hr = S_OK;
	TStackRef<IQuadooControl> current(Control());
	TStackRef<IQuadooControlContainer> root;
	for(UINT depth = 0; depth < 256; depth++)
	{
		TStackRef<IQuadooControlContainer> parent;
		Check(current->GetParent(&parent));
		if(!parent)
		{
			if(SUCCEEDED(current->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&root))))
			{
				QuadooForm::ComponentState state; Check(current->GetState(&state));
				if(state >= QuadooForm::Binding && state < QuadooForm::Destroying) { Check(root->UpdateLayout()); }
			}
			goto Cleanup;
		}
		current = parent;
	}
	hr = E_UNEXPECTED;
Cleanup:
	return hr;
}
STDMETHODIMP CControl::Measure(QuadooForm::Size* size)
{
	HRESULT hr;
	QuadooForm::Rect constrained;
	CheckIf(!size, E_POINTER); Check(CheckAlive());
	Check(MeasureContent(size));
	constrained.Left = constrained.Top = 0; constrained.Width = size->Width; constrained.Height = size->Height; Constrain(Control(), &constrained);
	size->Width = constrained.Width; size->Height = constrained.Height;
Cleanup:
	return hr;
}
HRESULT CControl::MeasureContent(QuadooForm::Size* size) { size->Width = m_bounds.Width; size->Height = m_bounds.Height; return S_OK; }


HRESULT CLabel::MeasureContent(QuadooForm::Size* size)
{
	HRESULT hr;
	CRString text;
	HDC dc;
	HGDIOBJ old;
	RECT rect = { 0, 0, 1000000, 0 };
	Check(StoredString(L"Caption", &text));
	dc = GetDC(NULL); CheckIf(!dc, HrEnsureLastError());
	old = SelectObject(dc, m_font);
	DrawTextW(dc, text.c_wstr(), RStrLen(text), &rect, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_CALCRECT);
	if(!RStrLen(text)) { TEXTMETRICW metric; GetTextMetricsW(dc, &metric); rect.bottom = metric.tmHeight; }
	SelectObject(dc, old); ReleaseDC(NULL, dc);
	size->Width = MulDiv(rect.right, 96, m_dpi); size->Height = MulDiv(rect.bottom, 96, m_dpi);
	hr = S_OK;
Cleanup:
	return hr;
}

HRESULT CContainer::MeasureContent(QuadooForm::Size* size)
{
	HRESULT hr;
	QuadooForm::Insets padding;
	Check(ContentInsets(&padding));
	size->Width = padding.Left + padding.Right; size->Height = padding.Top + padding.Bottom;
	for(sysint n = 0; n < m_children.Length(); n++)
	{
		TStackRef<IQuadooControl> child(m_children[n].control);
		QuadooForm::Rect bounds; QuadooForm::Insets margins; BOOL visible;
		Check(child->GetVisible(&visible)); if(!visible) continue;
		Check(child->GetBounds(&bounds)); Check(LayoutMargins(child, &margins));
		if(LayoutBool(child, L"AutoSize"))
		{
			TStackRef<IQuadooLayoutControl> layout;
			if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooLayoutControl), reinterpret_cast<PVOID*>(&layout))))
			{ QuadooForm::Size measured; Check(layout->Measure(&measured)); bounds.Width = measured.Width; bounds.Height = measured.Height; }
		}
		size->Width = max(size->Width, bounds.Left + bounds.Width + margins.Right + padding.Right);
		size->Height = max(size->Height, bounds.Top + bounds.Height + margins.Bottom + padding.Bottom);
	}
	hr = S_OK;
Cleanup:
	return hr;
}

STDMETHODIMP CControl::Arrange(const QuadooForm::Rect* bounds, UINT dpi)
{
	HRESULT hr;
	BOOL changed, dpiChanged;
	QuadooForm::Rect arranged;
	Check(CheckAlive()); CheckIf(!bounds || dpi < 48 || dpi > 768 || bounds->Width < 0 || bounds->Height < 0, E_INVALIDARG);
	arranged = *bounds;
	{
		TStackRef<IQuadooViewportDock> dock;
		Check(QueryOptional(Control(),&dock));
		if(!dock)
		{
			Constrain(Control(),&arranged);
		}
	}
	bounds = &arranged;
	changed = m_effective.Left != bounds->Left || m_effective.Top != bounds->Top || m_effective.Width != bounds->Width || m_effective.Height != bounds->Height;
	dpiChanged = m_dpi != dpi;
	if(dpiChanged) { m_dpi = dpi; Check(RecreateFont()); }
	m_effective = *bounds;
	Check(ArrangeNative(bounds, changed || dpiChanged));
	if(changed) m_boundsDirty = TRUE;
	Check(FlushNotifications());
Cleanup:
	return hr;
}

HRESULT CControl::ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed)
{
	HRESULT hr = S_OK;
	if(m_parent)
	{
		TStackRef<IQuadooChildWindowHost> host; HWND hwnd;
		Check(m_parent->QueryInterface(__uuidof(IQuadooChildWindowHost), reinterpret_cast<PVOID*>(&host))); Check(host->GetChildHostHandle(&hwnd)); InvalidateRect(hwnd, NULL, TRUE);
	}
Cleanup:
	return hr;
}
HRESULT CWinControl::ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed)
{
	HRESULT hr = S_OK;
	if(m_hwnd)
	{
		RECT physical = { MulDiv(bounds->Left, m_dpi, 96), MulDiv(bounds->Top, m_dpi, 96), MulDiv(bounds->Left + bounds->Width, m_dpi, 96), MulDiv(bounds->Top + bounds->Height, m_dpi, 96) };
		Check(AdjustNativeRect(&physical));
		SendMessageW(m_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
		if(changed || m_state < QuadooForm::Ready)
		{ CheckIf(!SetWindowPos(m_hwnd, NULL, physical.left, physical.top, physical.right - physical.left, physical.bottom - physical.top, SWP_NOZORDER | SWP_NOACTIVATE), HrEnsureLastError()); }
	}
Cleanup:
	return hr;
}
HRESULT CContainer::ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed)
{
	HRESULT hr;
	Check(CWinControl::ArrangeNative(bounds, changed));
	if(m_hwnd) { Check(UpdateLayout()); }
Cleanup:
	return hr;
}

HRESULT CContainer::LayoutChildren()
{
	HRESULT hr;
	RECT client;
	QuadooForm::Insets padding;
	QuadooForm::Size size;
	QuadooForm::Rect available;
	TArray<IQuadooWinControl*> windows;
	UINT clientCount = 0;
	CheckIf(!GetClientRect(m_hwnd, &client), HrEnsureLastError());
	size.Width = MulDiv(client.right, 96, m_dpi); size.Height = MulDiv(client.bottom, 96, m_dpi);
	Check(ContentInsets(&padding));
	available.Left = padding.Left; available.Top = padding.Top;
	available.Width = max(0, size.Width - padding.Left - padding.Right); available.Height = max(0, size.Height - padding.Top - padding.Bottom);
	INT bottom = size.Height;
	for(sysint n = 0; n < m_children.Length(); n++)
	{
		TStackRef<IQuadooViewportDock> dock;
		BOOL visible;
		Check(QueryOptional(m_children[n].control,&dock));
		if(dock)
		{
			QuadooForm::Rect bounds;
			QuadooForm::AlignMode alignment;
			Check(dock->GetDockAlignment(&alignment));
			CheckIf(alignment != QuadooForm::AlignBottom,E_INVALIDARG);
			Check(m_children[n].control->GetVisible(&visible));
			Check(m_children[n].control->GetBounds(&bounds));
			Constrain(m_children[n].control,&bounds);
			if(visible)
			{
				bottom = max(0,bottom - bounds.Height);
			}
		}
	}
	m_viewportHeight = bottom;
	available.Height = max(0,bottom - padding.Top - padding.Bottom);
	bottom = size.Height;
	m_contentSize.Width = padding.Left + padding.Right; m_contentSize.Height = padding.Top + padding.Bottom;
	for(sysint n = 0; n < m_children.Length(); n++)
	{
		Child& entry = m_children[n]; TStackRef<IQuadooControl> child(entry.control);
		QuadooForm::Rect bounds; QuadooForm::Insets margins; BOOL visible;
		Check(child->GetBounds(&bounds)); Check(child->GetVisible(&visible)); Check(LayoutMargins(child, &margins));
		TStackRef<IQuadooViewportDock> dock;
		Check(QueryOptional(child,&dock));
		if(dock)
		{
			TStackRef<IQuadooLayoutControl> layout;
			Constrain(child,&bounds);
			bounds.Left = 0;
			bounds.Width = size.Width;
			bounds.Height = min(bottom,bounds.Height);
			bounds.Top = bottom - bounds.Height;
			if(visible)
			{
				bottom = bounds.Top;
			}
			Check(child->QueryInterface(&layout));
			Check(layout->Arrange(&bounds,m_dpi));
			continue;
		}
		INT align = LayoutInt(child, L"Align", 0), anchors = LayoutInt(child, L"Anchors", 3);
		// Detect edits through public state, including external property implementations.
		if(entry.anchored && (memcmp(&entry.stored, &bounds, sizeof(bounds)) || entry.anchors != anchors)) entry.anchored = FALSE;
		entry.stored = bounds; entry.anchors = anchors;
		if(align == QuadooForm::AlignClient) { CheckIf(++clientCount > 1, E_INVALIDARG); }
		if(LayoutBool(child, L"AutoSize"))
		{
			TStackRef<IQuadooLayoutControl> layout;
			if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooLayoutControl), reinterpret_cast<PVOID*>(&layout))))
			{ QuadooForm::Size measured; Check(layout->Measure(&measured)); bounds.Width = measured.Width; bounds.Height = measured.Height; }
		}
		if(align && visible)
		{
			INT width = max(0, available.Width - margins.Left - margins.Right), height = max(0, available.Height - margins.Top - margins.Bottom);
			bounds.Left = available.Left + margins.Left; bounds.Top = available.Top + margins.Top;
			if(align == QuadooForm::AlignTop || align == QuadooForm::AlignBottom)
			{
				bounds.Width = width; Constrain(child, &bounds);
				INT allocation = min(available.Height, bounds.Height + margins.Top + margins.Bottom);
				if(align == QuadooForm::AlignBottom) bounds.Top = available.Top + available.Height - margins.Bottom - bounds.Height;
				else available.Top += allocation;
				available.Height -= allocation;
			}
			else if(align == QuadooForm::AlignLeft || align == QuadooForm::AlignRight)
			{
				bounds.Height = height; Constrain(child, &bounds);
				INT allocation = min(available.Width, bounds.Width + margins.Left + margins.Right);
				if(align == QuadooForm::AlignRight) bounds.Left = available.Left + available.Width - margins.Right - bounds.Width;
				else available.Left += allocation;
				available.Width -= allocation;
			}
			else { bounds.Width = width; bounds.Height = height; Constrain(child, &bounds); }
		}
		else if(!align)
		{
			if(!entry.anchored) { entry.baseline = bounds; entry.parentSize = size; entry.anchored = TRUE; }
			INT deltaX = size.Width - entry.parentSize.Width, deltaY = size.Height - entry.parentSize.Height;
			bounds.Left = entry.baseline.Left; bounds.Top = entry.baseline.Top;
			if((anchors & 5) == 5) bounds.Width = max(0, entry.baseline.Width + deltaX);
			else if(anchors & 4) bounds.Left += deltaX; else if(!(anchors & 1)) bounds.Left += deltaX / 2;
			if((anchors & 10) == 10) bounds.Height = max(0, entry.baseline.Height + deltaY);
			else if(anchors & 8) bounds.Top += deltaY; else if(!(anchors & 2)) bounds.Top += deltaY / 2;
			Constrain(child, &bounds);
		}
		TStackRef<IQuadooLayoutControl> layout;
		if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooLayoutControl), reinterpret_cast<PVOID*>(&layout)))) { Check(layout->Arrange(&bounds, m_dpi)); }
		else
		{
			TStackRef<IQuadooWinControl> window; HWND hwnd;
			if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&window))))
			{ Check(window->GetHandle(&hwnd)); CheckIf(!SetWindowPos(hwnd, NULL, MulDiv(bounds.Left, m_dpi, 96), MulDiv(bounds.Top, m_dpi, 96), MulDiv(bounds.Width, m_dpi, 96), MulDiv(bounds.Height, m_dpi, 96), SWP_NOZORDER | SWP_NOACTIVATE), HrEnsureLastError()); }
		}
		if(visible)
		{
			m_contentSize.Width = max(m_contentSize.Width, bounds.Left + bounds.Width + margins.Right + padding.Right);
			m_contentSize.Height = max(m_contentSize.Height, bounds.Top + bounds.Height + margins.Bottom + padding.Bottom);
		}
	}
	// Native z-order drives dialog tab traversal; keep the public child order unchanged.
	for(sysint n = 0; n < m_children.Length(); n++)
	{
		TStackRef<IQuadooWinControl> window;
		if(SUCCEEDED(m_children[n].control->QueryInterface(__uuidof(IQuadooWinControl), reinterpret_cast<PVOID*>(&window))))
		{
			INT order; Check(window->GetTabOrder(&order)); sysint position = windows.Length();
			for(sysint w = 0; w < windows.Length(); w++) { INT other; Check(windows[w]->GetTabOrder(&other)); if(order < other) { position = w; break; } }
			IQuadooWinControl* pointer = window;
			Check(windows.InsertAt(&pointer, position)); window.Detach();
		}
	}
	for(sysint n = windows.Length(); n > 0; n--)
	{ HWND hwnd; if(SUCCEEDED(windows[n - 1]->GetHandle(&hwnd))) SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); }
	hr = S_OK;
Cleanup:
	for(sysint n = 0; n < windows.Length(); n++) windows[n]->Release();
	return hr;
}

STDMETHODIMP CContainer::UpdateLayout()
{
	HRESULT hr;
	BOOL entered = FALSE;
	TStackRef<CControl> keepAlive(this);
	Check(CheckAlive());
	CheckIfIgnore(m_registering, S_FALSE);
	m_layoutDirty = TRUE;
	// Ancestor layout defers notifications, but must still arrange ready nested
	// containers. Only explicit batches defer this top-down layout traversal.
	CheckIfIgnore(m_layoutBusy || (m_state == QuadooForm::Ready && Updating(FALSE)), S_FALSE);
	m_layoutBusy = TRUE; entered = TRUE;
	for(UINT pass = 0; pass < 8; pass++)
	{
		RECT before, after; GetClientRect(m_hwnd, &before); m_layoutDirty = FALSE;
		Check(LayoutChildren()); Check(FinishLayout());
		GetClientRect(m_hwnd, &after);
		if(!m_layoutDirty && before.right == after.right && before.bottom == after.bottom) break;
		CheckIf(pass == 7, E_UNEXPECTED);
	}
	m_layoutBusy = FALSE; Check(FlushTree(Control()));
	hr = S_OK;
Cleanup:
	if(entered) m_layoutBusy = FALSE;
	return hr;
}

HRESULT CContainer::FinishLayout() { return S_OK; }

HRESULT CScrollingContainer::FinishLayout() { return UpdateScrollbars(); }

HRESULT CScrollingContainer::UpdateScrollbars()
{
	HRESULT hr;
	RECT client;
	SCROLLINFO horizontal = { sizeof(horizontal), SIF_RANGE | SIF_PAGE | SIF_POS };
	SCROLLINFO vertical = { sizeof(vertical), SIF_RANGE | SIF_PAGE | SIF_POS };
	CheckIf(!GetClientRect(m_hwnd, &client), HrEnsureLastError());
	INT width = MulDiv(client.right, 96, m_dpi);
	INT height = min(MulDiv(client.bottom,96,m_dpi),m_viewportHeight);
	if(!Boolean(L"AutoScroll"))
	{ ShowScrollBar(m_hwnd, SB_BOTH, FALSE); m_scroll.X = m_scroll.Y = 0; }
	else
	{
		m_scroll.X = max(0, min(m_scroll.X, m_contentSize.Width - width)); m_scroll.Y = max(0, min(m_scroll.Y, m_contentSize.Height - height));
		horizontal.nMin = vertical.nMin = 0;
		horizontal.nMax = max(0, m_contentSize.Width - 1); vertical.nMax = max(0, m_contentSize.Height - 1);
		horizontal.nPage = width; vertical.nPage = height; horizontal.nPos = m_scroll.X; vertical.nPos = m_scroll.Y;
		SetScrollInfo(m_hwnd, SB_HORZ, &horizontal, TRUE); SetScrollInfo(m_hwnd, SB_VERT, &vertical, TRUE);
	}
	CheckIf(!SetWindowPos(m_content, NULL, -MulDiv(m_scroll.X, m_dpi, 96), -MulDiv(m_scroll.Y, m_dpi, 96),
		MulDiv(max(width, m_contentSize.Width), m_dpi, 96), MulDiv(max(height, m_contentSize.Height), m_dpi, 96), SWP_NOZORDER | SWP_NOACTIVATE), HrEnsureLastError());
	hr = S_OK;
Cleanup:
	return hr;
}

STDMETHODIMP CScrollingContainer::GetScrollPosition(QuadooForm::Point* position) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) { if(position) *position = m_scroll; else hr = E_POINTER; } return hr; }

STDMETHODIMP CScrollingContainer::GetContentSize(QuadooForm::Size* size) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) { if(size) *size = m_contentSize; else hr = E_POINTER; } return hr; }

STDMETHODIMP CScrollingContainer::SetScrollPosition(const QuadooForm::Point* position)
{
	HRESULT hr;
	Check(CheckAlive()); CheckIf(!position, E_INVALIDARG);
	CheckIf(position->X < 0 || position->Y < 0, E_INVALIDARG);
	m_scroll = *position; Check(UpdateScrollbars());
Cleanup:
	return hr;
}
}
