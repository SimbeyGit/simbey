#include "Designer.h"

namespace QFD
{
VOID CDesignerEditor::Layout()
{
	if(!m_canvas || m_layingOut) return;
	m_layingOut = TRUE;
	RECT client; GetClientRect(m_window,&client);
	INT left = m_embedded ? 0 : min(m_leftWidth,max(120,client.right / 3)), right = min(m_rightWidth,max(220,client.right / 3));
	INT tree = min(m_treeHeight,max(100,client.bottom - 150));
	struct Placement { HWND window; INT x, y, width, height; };
	Placement panes[] = {
		{ GetDlgItem(m_window,110),6,4,left - 12,20 },
		{ m_tree,4,25,left - 8,max(40,tree - 25) },
		{ GetDlgItem(m_window,111),6,tree + 8,left - 12,20 },
		{ m_tools,4,tree + 30,left - 8,max(30,client.bottom - tree - 34) },
		{ m_canvas,left + 5,0,max(20,client.right - left - right - 10),max(20,client.bottom - 92) },
		{ GetDlgItem(m_window,112),left + 9,max(20,client.bottom - 88),max(20,client.right - left - right - 18),18 },
		{ m_tray,left + 5,max(40,client.bottom - 68),max(20,client.right - left - right - 10),max(20,min(64,client.bottom - 40)) },
		{ m_tabs,max(0,client.right - right + 3),3,right - 6,26 },
		{ m_list,max(0,client.right - right + 3),31,right - 6,max(20,client.bottom - 34) }
	};
	const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS;
	HDWP positions = BeginDeferWindowPos(ARRAYSIZE(panes));
	for(UINT n = m_embedded ? 4 : 0; positions && n < ARRAYSIZE(panes); n++) positions = DeferWindowPos(positions,panes[n].window,NULL,panes[n].x,panes[n].y,panes[n].width,panes[n].height,flags);
	if(!positions || !EndDeferWindowPos(positions))
		for(UINT n = m_embedded ? 4 : 0; n < ARRAYSIZE(panes); n++) SetWindowPos(panes[n].window,NULL,panes[n].x,panes[n].y,panes[n].width,panes[n].height,flags);
	ListView_SetColumnWidth(m_list,0,max(80,(right - 30) * 45 / 100)); ListView_SetColumnWidth(m_list,1,max(80,(right - 30) * 55 / 100));
	{ RECT tools; GetClientRect(m_tools,&tools); ListView_SetColumnWidth(m_tools,0,max(20,tools.right)); }
	PositionInline();
	m_layingOut = FALSE;
	ArrangeSurface();
	RedrawWindow(m_window,NULL,NULL,RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

VOID CDesignerEditor::ArrangeSurface()
{
	if(!m_document.Root() || !m_canvas || m_arranging) return;
	const INT margin = 10;

	m_arranging = TRUE;
	m_changeDepth++;
	QuadooForm::Rect bounds;
	RECT insets, client;
	UINT scale = MulDiv(m_dpi,m_zoom,100);

	if(SUCCEEDED(m_document.Root()->GetBounds(&bounds)) && GetClientRect(m_canvas,&client))
	{
		CDesignerWindowFrame::Insets(m_document.Root(),m_document.IsForm(),scale,&insets);
		INT width = MulDiv(bounds.Width,scale,96), height = MulDiv(bounds.Height,scale,96);
		SCROLLINFO scroll = { sizeof(scroll),SIF_RANGE | SIF_PAGE | SIF_POS };
		m_scroll.X = max(0,min(m_scroll.X,width + insets.left + insets.right + margin * 2 - client.right));
		m_scroll.Y = max(0,min(m_scroll.Y,height + insets.top + insets.bottom + margin * 2 - client.bottom));
		scroll.nMin = 0; scroll.nMax = width + insets.left + insets.right + margin * 2 - 1; scroll.nPage = client.right; scroll.nPos = m_scroll.X; SetScrollInfo(m_canvas,SB_HORZ,&scroll,TRUE);
		scroll.nMax = height + insets.top + insets.bottom + margin * 2 - 1; scroll.nPage = client.bottom; scroll.nPos = m_scroll.Y; SetScrollInfo(m_canvas,SB_VERT,&scroll,TRUE);
		TStackRef<IQuadooLayoutControl> layout;
		bounds.Left = bounds.Top = 0;
		if(SUCCEEDED(m_document.Root()->QueryInterface(__uuidof(IQuadooLayoutControl),reinterpret_cast<PVOID*>(&layout)))) layout->Arrange(&bounds,scale);
		// Creation order determines design stacking; runtime native tab traversal
		// may arrange HWNDs by TabOrder. Keep the design view and hit testing alike.
		for(sysint n = 1; n < m_nodes.Length(); n++) if(m_nodes[n].control)
		{
			TStackRef<IQuadooWinControl> native;
			HWND child;
			if(SUCCEEDED(m_nodes[n].control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))) && SUCCEEDED(native->GetHandle(&child)))
				SetWindowPos(child,HWND_TOP,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		}
		MoveWindow(m_document.Host(),margin + insets.left - m_scroll.X,margin + insets.top - m_scroll.Y,max(1,width),max(1,height),TRUE);
		ShowWindow(m_document.Host(),SW_SHOWNOACTIVATE);
		GetClientRect(m_canvas,&client); SetWindowPos(m_overlay,HWND_TOP,0,0,client.right,client.bottom,SWP_NOACTIVATE);
	}
	RedrawWindow(m_canvas,NULL,NULL,RDW_INVALIDATE | RDW_ALLCHILDREN);
	m_changeDepth--;
	m_arranging = FALSE;
}

HRESULT CDesignerEditor::ControlRect(IQuadooControl* control, RECT* rect)
{
	HRESULT hr;
	TStackRef<IQuadooWinControl> native; TStackRef<IQuadooControlContainer> parent; TStackRef<IQuadooChildWindowHost> host;
	HWND window; UINT scale = MulDiv(m_dpi,m_zoom,100);
	CheckIf(!control || !rect,E_INVALIDARG);
	hr = control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native));
	if(SUCCEEDED(hr)) { Check(native->GetHandle(&window)); CheckIf(!GetWindowRect(window,rect),HrEnsureLastError()); MapWindowPoints(NULL,m_canvas,reinterpret_cast<POINT*>(rect),2); }
	else
	{
		QuadooForm::Rect bounds; Check(control->GetEffectiveBounds(&bounds)); Check(control->GetParent(&parent)); CheckIf(!parent,E_INVALIDARG);
		Check(parent->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host))); Check(host->GetChildHostHandle(&window));
		SetRect(rect,MulDiv(bounds.Left,scale,96),MulDiv(bounds.Top,scale,96),MulDiv(bounds.Left + bounds.Width,scale,96),MulDiv(bounds.Top + bounds.Height,scale,96)); MapWindowPoints(window,m_canvas,reinterpret_cast<POINT*>(rect),2);
	}
	if(SameObject(control,m_document.Root()))
	{ RECT insets; CDesignerWindowFrame::Insets(control,m_document.IsForm(),scale,&insets); rect->left -= insets.left; rect->right += insets.right; rect->top -= insets.top; rect->bottom += insets.bottom; }
	hr = S_OK;
Cleanup:
	return hr;
}

HRESULT CDesignerEditor::SelectionRect(RECT* rect)
{
	HRESULT hr; TStackRef<IQuadooControl> control;
	CheckIf(!rect,E_POINTER);
	SetRectEmpty(rect);
	if(SelectionCount() > 1)
	{
		for(sysint n = 0; n < m_selection.Length(); n++)
		{
			RECT bounds;
			control.Release();
			Check(m_document.Find(m_selection[n],&control));
			Check(ControlRect(control,&bounds));
			UnionRect(rect,rect,&bounds);
		}
		goto Cleanup;
	}
	hr = MenuSelectionRect(rect);
	if(hr == S_OK)
	{
		goto Cleanup;
	}
	Check(Selected(&control));
	Check(ControlRect(control,rect));
Cleanup:
	return hr;
}

HRESULT CDesignerEditor::SelectionCapabilities(DWORD* capabilities)
{
	HRESULT hr; TStackRef<IQuadooControl> control; TStackRef<IQuadooDesignCapabilities> capability;
	CheckIf(!capabilities,E_POINTER); *capabilities = 0; Check(Selected(&control));
	if(SelectionCount() > 1)
	{
		DWORD shared = QuadooForm::DesignMoveX | QuadooForm::DesignMoveY;
		for(sysint n = 0; n < m_selection.Length(); n++)
		{
			DWORD flags = 0;
			control.Release();
			capability.Release();
			Check(m_document.Find(m_selection[n],&control));
			hr = control->QueryInterface(__uuidof(IQuadooDesignCapabilities),reinterpret_cast<PVOID*>(&capability));
			if(hr != E_NOINTERFACE)
			{
				Check(hr);
				Check(capability->GetDesignCapabilities(&flags));
			}
			shared &= flags;
		}
		*capabilities = shared;
		hr = S_OK;
		goto Cleanup;
	}
	hr = control->QueryInterface(__uuidof(IQuadooDesignCapabilities),reinterpret_cast<PVOID*>(&capability));
	if(hr == E_NOINTERFACE)
		hr = S_OK;
	else
	{
		Check(hr);
		Check(capability->GetDesignCapabilities(capabilities));
	}
Cleanup:
	return hr;
}

IQuadooControl* CDesignerEditor::HitControl(POINT point)
{
	for(sysint n = m_nodes.Length(); n > 0; n--)
	{
		IQuadooControl* control = m_nodes[n - 1].control;
		RECT rect;
		BOOL visible;

		if(!control)
			continue;
		if(FAILED(control->GetVisible(&visible)) || (!visible && !SameObject(control,m_document.Root())))
			continue;

		// Visible is the requested property. Inactive TabPages retain it while
		// their native windows are hidden, so exclude their overlapping bounds.
		TStackRef<IQuadooWinControl> native;
		if(SUCCEEDED(control->QueryInterface(__uuidof(IQuadooWinControl),reinterpret_cast<PVOID*>(&native))))
		{
			HWND window;
			if(FAILED(native->GetHandle(&window)) || !IsWindowVisible(window))
				continue;
		}

		TStackRef<IQuadooControlContainer> parent;
		TStackRef<IQuadooChildWindowHost> host; HWND window;
		if(SUCCEEDED(control->GetParent(&parent)) && parent && SUCCEEDED(parent->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host))) && SUCCEEDED(host->GetChildHostHandle(&window)))
		{
			RECT clip; GetClientRect(window,&clip); MapWindowPoints(window,m_canvas,reinterpret_cast<POINT*>(&clip),2);
			if(!IsWindowVisible(window) || !PtInRect(&clip,point))
				continue;
		}
		if(SUCCEEDED(ControlRect(control,&rect)) && PtInRect(&rect,point))
			return control;
	}
	return NULL;
}

HRESULT CDesignerEditor::ParentAt(POINT point, RSTRING type, IQuadooControlContainer** parent)
{
	HRESULT hr;
	TStackRef<IQuadooControl> current(HitControl(point)); TStackRef<IQuadooControlFactory> factory; TStackRef<IQuadooDesignFactory> placement;
	CheckIf(!parent,E_POINTER); *parent = NULL; CheckIf(!current,E_INVALIDARG); Check(m_document.Registry()->FindFactory(type,&factory));
	hr = factory->QueryInterface(__uuidof(IQuadooDesignFactory),reinterpret_cast<PVOID*>(&placement)); if(hr != E_NOINTERFACE) { Check(hr); }
	for(UINT depth = 0; current && depth < 128; depth++)
	{
		TStackRef<IQuadooControlContainer> container, next; BOOL allowed = TRUE;
		if(SUCCEEDED(current->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&container))))
		{
			if(placement) { Check(placement->CanCreate(container,&allowed)); }
			if(allowed) { *parent = container.Detach(); hr = S_OK; goto Cleanup; }
		}
		Check(current->GetParent(&next)); current = next;
	}
	hr = E_INVALIDARG;
Cleanup:
	return hr;
}
VOID CDesignerEditor::GripRects(const RECT* rect, RECT* grips)
{
	INT x[] = { rect->left,(rect->left + rect->right) / 2,rect->right,rect->left,rect->right,rect->left,(rect->left + rect->right) / 2,rect->right };
	INT y[] = { rect->top,rect->top,rect->top,(rect->top + rect->bottom) / 2,(rect->top + rect->bottom) / 2,rect->bottom,rect->bottom,rect->bottom };
	for(UINT n = 0; n < 8; n++) SetRect(&grips[n],x[n] - 3,y[n] - 3,x[n] + 4,y[n] + 4);
}
static BOOL GripEnabled(INT grip, DWORD flags)
{
	BOOL width = grip != 1 && grip != 6, height = grip != 3 && grip != 4;
	return (!width || (flags & QuadooForm::DesignResizeWidth)) && (!height || (flags & QuadooForm::DesignResizeHeight));
}
INT CDesignerEditor::HitGrip(POINT point, DWORD* capabilities)
{
	RECT rect, grips[8]; *capabilities = 0;
	if(SelectionCount() > 1)
	{
		SelectionCapabilities(capabilities);
		return -1;
	}
	if(SUCCEEDED(SelectionRect(&rect)) && SUCCEEDED(SelectionCapabilities(capabilities)))
	{ GripRects(&rect,grips); for(INT n = 0; n < 8; n++) if(PtInRect(&grips[n],point)) return GripEnabled(n,*capabilities) ? n : -2; }
	return -1;
}
VOID CDesignerEditor::PaintCanvas(HDC dc)
{
	RECT client; GetClientRect(m_canvas,&client); FillRect(dc,&client,GetSysColorBrush(COLOR_APPWORKSPACE));
	if(m_grid)
	{
		INT step = max(4,MulDiv(8,MulDiv(m_dpi,m_zoom,100),96));
		for(INT y = -m_scroll.Y % step; y < client.bottom; y += step) for(INT x = -m_scroll.X % step; x < client.right; x += step) if(x >= 0 && y >= 0) SetPixelV(dc,x,y,GetSysColor(COLOR_3DSHADOW));
	}
	if(m_document.Root())
	{
		RECT root; if(GetWindowRect(m_document.Host(),&root)) { MapWindowPoints(NULL,m_canvas,reinterpret_cast<POINT*>(&root),2); CDesignerWindowFrame::Paint(m_canvas,dc,m_document.Root(),m_document.IsForm(),MulDiv(m_dpi,m_zoom,100),&root,m_selected); }
	}
}
VOID CDesignerEditor::PaintBuffered(HDC dc, const RECT* update, UINT surface)
{
	if(IsRectEmpty(update)) return;
	INT width = update->right - update->left, height = update->bottom - update->top;
	HDC memory = CreateCompatibleDC(dc);
	HBITMAP bitmap = memory ? CreateCompatibleBitmap(dc,width,height) : NULL;
	HGDIOBJ previous = bitmap ? SelectObject(memory,bitmap) : NULL;
	BOOL buffered = previous && previous != HGDI_ERROR;
	HDC target = buffered ? memory : dc; INT saved = SaveDC(target);
	if(buffered) SetWindowOrgEx(target,update->left,update->top,NULL);
	if(surface == 0) FillRect(target,update,GetSysColorBrush(COLOR_BTNFACE)); else PaintCanvas(target);
	if(buffered) BitBlt(dc,update->left,update->top,width,height,memory,update->left,update->top,SRCCOPY);
	if(saved) RestoreDC(target,saved);
	if(buffered) SelectObject(memory,previous);
	if(bitmap) DeleteObject(bitmap); if(memory) DeleteDC(memory);
}
VOID CDesignerEditor::PaintOverlay(HDC dc)
{
	RECT rect, grips[8]; DWORD flags;
	HPEN dashed = CreatePen(PS_DASH,1,GetSysColor(COLOR_GRAYTEXT)); HGDIOBJ previousPen = dashed ? SelectObject(dc,dashed) : NULL;
	HGDIOBJ previousBrush = SelectObject(dc,GetStockObject(HOLLOW_BRUSH)); INT previousBackground = SetBkMode(dc,TRANSPARENT);
	for(sysint n = 0; n < m_nodes.Length(); n++) if(m_nodes[n].control)
	{
		TStackRef<IQuadooDesignPlaceholder> placeholder;
		if(SUCCEEDED(m_nodes[n].component->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder))) && SUCCEEDED(ControlRect(m_nodes[n].control,&rect))) Rectangle(dc,rect.left,rect.top,rect.right,rect.bottom);
	}
	SetBkMode(dc,previousBackground); SelectObject(dc,previousBrush); if(previousPen) SelectObject(dc,previousPen); if(dashed) DeleteObject(dashed);
	HGDIOBJ oldBrush = SelectObject(dc,GetStockObject(HOLLOW_BRUSH)); HPEN pen = CreatePen(PS_SOLID,1,RGB(30,100,210)); HGDIOBJ oldPen = SelectObject(dc,pen);
	if(SelectionCount() > 1)
	{
		for(sysint n = 0; n < m_selection.Length(); n++)
		{
			TStackRef<IQuadooControl> control;
			if(SUCCEEDED(m_document.Find(m_selection[n],&control)) && SUCCEEDED(ControlRect(control,&rect)))
			{
				Rectangle(dc,rect.left,rect.top,rect.right,rect.bottom);
				GripRects(&rect,grips);
				static const UINT corners[] = {0,2,5,7};
				for(UINT grip = 0; grip < ARRAYSIZE(corners); grip++)
				{
					SetDCBrushColor(dc,RGB(30,100,210));
					FillRect(dc,&grips[corners[grip]],static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
					FrameRect(dc,&grips[corners[grip]],GetSysColorBrush(COLOR_WINDOW));
				}
				if(m_dragging && !m_newControl)
				{
					UINT scale = MulDiv(m_dpi,m_zoom,100);
					OffsetRect(&rect,MulDiv(m_pendingBounds.Left - m_startBounds.Left,scale,96),MulDiv(m_pendingBounds.Top - m_startBounds.Top,scale,96));
					Rectangle(dc,rect.left,rect.top,rect.right,rect.bottom);
				}
			}
		}
	}
	else if(SUCCEEDED(SelectionRect(&rect)) && SUCCEEDED(SelectionCapabilities(&flags)))
	{
		Rectangle(dc,rect.left,rect.top,rect.right,rect.bottom); GripRects(&rect,grips);
		for(INT n = 0; n < 8; n++) { SetDCBrushColor(dc,GripEnabled(n,flags) ? RGB(30,100,210) : GetSysColor(COLOR_GRAYTEXT)); FillRect(dc,&grips[n],static_cast<HBRUSH>(GetStockObject(DC_BRUSH))); FrameRect(dc,&grips[n],GetSysColorBrush(COLOR_WINDOW)); }
	}
	if(m_dragging && !IsRectEmpty(&m_ghost)) Rectangle(dc,m_ghost.left,m_ghost.top,m_ghost.right,m_ghost.bottom);
	if(m_tool > 0 && m_tool <= m_palette.Length())
	{
		POINT point; GetCursorPos(&point); ScreenToClient(m_canvas,&point); TStackRef<IQuadooControlContainer> parent;
		if(SUCCEEDED(ParentAt(point,m_palette[m_tool - 1].name,&parent)) && SUCCEEDED(ControlRect(parent,&rect))) { InflateRect(&rect,-2,-2); DrawFocusRect(dc,&rect); }
	}
	SelectObject(dc,oldPen); SelectObject(dc,oldBrush); if(pen) DeleteObject(pen);
}
VOID CDesignerEditor::MouseDown(POINT point, BOOL add)
{
	if(!m_document.Root()) return;
	m_clickSelection = GUID_NULL;
	if(m_tool == 0 && SelectMenuAt(point) == S_OK)
	{
		SetFocus(m_overlay);
		return;
	}
	SetFocus(m_overlay); m_start = point; m_newControl = m_tool > 0; m_grip = -1;
	if(m_newControl) { m_dragging = TRUE; SetRect(&m_ghost,point.x,point.y,point.x,point.y); SetCapture(m_overlay); return; }
	DWORD flags = 0;
	if(!add && !m_readOnly)
	{
		m_grip = HitGrip(point,&flags);
	}
	if(m_grip == -2) return;
	if(m_grip < 0)
	{
		IQuadooControl* hit = HitControl(point); if(!hit) return;
		TStackRef<IQuadooTabControl> tabs;
		if(SUCCEEDED(hit->QueryInterface(__uuidof(IQuadooTabControl),reinterpret_cast<PVOID*>(&tabs))))
		{
			HWND window; if(SUCCEEDED(tabs->GetHandle(&window)))
			{
				TCHITTESTINFO info; ZeroMemory(&info,sizeof(info)); info.pt = point; MapWindowPoints(m_canvas,window,&info.pt,1); INT header = TabCtrl_HitTest(window,&info);
				if(header >= 0)
				{
					UINT count;
					tabs->GetPageCount(&count);
					for(UINT n = 0; n < count; n++)
					{
						TStackRef<IQuadooTabPage> page;
						BOOL visible;
						if(SUCCEEDED(tabs->GetPage(n,&page)) && SUCCEEDED(page->GetTabVisible(&visible)) && visible && header-- == 0)
						{
							Select(page,add);
							return;
						}
					}
				}
			}
		}
		GUID id;
		if(FAILED(hit->GetID(&id)))
		{
			return;
		}
		if(!add && !m_readOnly && SelectionCount() > 1 && IsSelected(id))
		{
			// Keep the group until a gesture is known to be a drag. A plain click
			// still collapses it on button-up, as it does for any other control.
			if(FAILED(FinishInline(TRUE)))
			{
				return;
			}
			m_clickSelection = id;
			m_selected = id;
			Refresh(FALSE);
		}
		else if(Select(hit,add) != S_OK)
		{
			return;
		}
		if(m_readOnly)
			return;
		SelectionCapabilities(&flags);
		if(!(flags & (QuadooForm::DesignMoveX | QuadooForm::DesignMoveY)))
		{
			if(!IsEqualGUID(m_clickSelection,GUID_NULL))
			{
				SelectID(m_clickSelection);
			}
			return;
		}
	}
	TStackRef<IQuadooControl> selected;
	if(FAILED(Selected(&selected)) || FAILED(selected->GetBounds(&m_startBounds))) return;
	// Apply gesture deltas to requested bounds. Effective layout dimensions may
	// differ (docking, anchors, AutoSize); merely clicking must not persist them.
	m_pendingBounds = m_startBounds; m_dragging = TRUE; SetCapture(m_overlay); SelectionRect(&m_ghost);
}
static INT SnapValue(INT number) { return number >= 0 ? ((number + 4) / 8) * 8 : -(((-number + 4) / 8) * 8); }
VOID CDesignerEditor::MouseMove(POINT point)
{
	if(!m_dragging) { if(m_tool) RedrawWindow(m_canvas,NULL,NULL,RDW_INVALIDATE | RDW_ALLCHILDREN); return; }
	if(m_newControl) { SetRect(&m_ghost,min(point.x,m_start.x),min(point.y,m_start.y),max(point.x,m_start.x),max(point.y,m_start.y)); }
	else
	{
		TStackRef<IQuadooControl> selected; DWORD flags;
		if(FAILED(Selected(&selected)) || FAILED(SelectionCapabilities(&flags))) return;
		INT dx = MulDiv(point.x - m_start.x,96,MulDiv(m_dpi,m_zoom,100)), dy = MulDiv(point.y - m_start.y,96,MulDiv(m_dpi,m_zoom,100));
		m_pendingBounds = m_startBounds;
		if(m_grip < 0) { if(flags & QuadooForm::DesignMoveX) m_pendingBounds.Left += dx; if(flags & QuadooForm::DesignMoveY) m_pendingBounds.Top += dy; }
		else
		{
			BOOL left = m_grip == 0 || m_grip == 3 || m_grip == 5, right = m_grip == 2 || m_grip == 4 || m_grip == 7;
			BOOL top = m_grip <= 2, bottom = m_grip >= 5;
			if(left) { m_pendingBounds.Width -= dx; if(flags & QuadooForm::DesignMoveX) m_pendingBounds.Left += dx; }
			if(right) m_pendingBounds.Width += dx;
			if(top) { m_pendingBounds.Height -= dy; if(flags & QuadooForm::DesignMoveY) m_pendingBounds.Top += dy; }
			if(bottom) m_pendingBounds.Height += dy;
		}
		if(m_snap)
		{
			if(m_pendingBounds.Left != m_startBounds.Left) m_pendingBounds.Left = SnapValue(m_pendingBounds.Left);
			if(m_pendingBounds.Top != m_startBounds.Top) m_pendingBounds.Top = SnapValue(m_pendingBounds.Top);
			if(m_pendingBounds.Width != m_startBounds.Width) m_pendingBounds.Width = SnapValue(m_pendingBounds.Width);
			if(m_pendingBounds.Height != m_startBounds.Height) m_pendingBounds.Height = SnapValue(m_pendingBounds.Height);
		}
		if(m_grip >= 0)
		{
			INT minWidth = IntProperty(selected,L"MinWidth");
			INT minHeight = IntProperty(selected,L"MinHeight");
			INT maxWidth = IntProperty(selected,L"MaxWidth",1000000);
			INT maxHeight = IntProperty(selected,L"MaxHeight",1000000);
			if(!maxWidth)
			{
				maxWidth = 1000000;
			}
			if(!maxHeight)
			{
				maxHeight = 1000000;
			}
			m_pendingBounds.Width = max(minWidth,min(maxWidth,m_pendingBounds.Width));
			m_pendingBounds.Height = max(minHeight,min(maxHeight,m_pendingBounds.Height));
		}
		m_pendingBounds.Left = max(-1000000,min(1000000,m_pendingBounds.Left)); m_pendingBounds.Top = max(-1000000,min(1000000,m_pendingBounds.Top));
		RECT rect; if(SUCCEEDED(SelectionRect(&rect)))
		{
			UINT scale = MulDiv(m_dpi,m_zoom,100); INT x = MulDiv(m_pendingBounds.Left - m_startBounds.Left,scale,96), y = MulDiv(m_pendingBounds.Top - m_startBounds.Top,scale,96);
			m_ghost = rect; OffsetRect(&m_ghost,x,y); m_ghost.right += MulDiv(m_pendingBounds.Width - m_startBounds.Width,scale,96); m_ghost.bottom += MulDiv(m_pendingBounds.Height - m_startBounds.Height,scale,96);
		}
	}
	// Queue a single composited frame; do not expose successive background,
	// native-control and overlay paints on every mouse message.
	RedrawWindow(m_canvas,NULL,NULL,RDW_INVALIDATE | RDW_ALLCHILDREN);
}
VOID CDesignerEditor::MouseUp(POINT point, BOOL cancel)
{
	if(!m_dragging) return;
	MouseMove(point); m_dragging = FALSE; if(GetCapture() == m_overlay) ReleaseCapture();
	HRESULT hr = S_OK;
	if(!cancel)
	{
		if(m_newControl && m_tool > 0 && m_tool <= m_palette.Length())
		{
			TStackRef<IQuadooControlContainer> parent; TStackRef<IQuadooChildWindowHost> host; HWND window;
			hr = ParentAt(m_start,m_palette[m_tool - 1].name,&parent);
			if(SUCCEEDED(hr)) hr = parent->QueryInterface(__uuidof(IQuadooChildWindowHost),reinterpret_cast<PVOID*>(&host));
			if(SUCCEEDED(hr)) hr = host->GetChildHostHandle(&window);
			if(SUCCEEDED(hr))
			{
				POINT position = { m_ghost.left,m_ghost.top }; MapWindowPoints(m_canvas,window,&position,1); UINT scale = MulDiv(m_dpi,m_zoom,100);
				QuadooForm::Rect bounds = { MulDiv(position.x,96,scale),MulDiv(position.y,96,scale),0,0 }; GUID id;
				if(m_ghost.right - m_ghost.left > 4) bounds.Width = MulDiv(m_ghost.right - m_ghost.left,96,scale);
				if(m_ghost.bottom - m_ghost.top > 4) bounds.Height = MulDiv(m_ghost.bottom - m_ghost.top,96,scale);
				if(m_snap) { bounds.Left = SnapValue(bounds.Left); bounds.Top = SnapValue(bounds.Top); }
				hr = AddControl(m_palette[m_tool - 1].name,parent,&bounds,&id);
			}
			ResetTool();
		}
		else if(!IsEqualGUID(m_clickSelection,GUID_NULL) && !memcmp(&m_startBounds,&m_pendingBounds,sizeof(m_startBounds)))
		{
			hr = SelectID(m_clickSelection);
		}
		else
		{
			hr = SetSelectionBounds(&m_pendingBounds);
		}
	}
	SetRectEmpty(&m_ghost); ArrangeSurface(); if(FAILED(hr)) Error(hr,L"Place/resize control");
}
LRESULT CALLBACK CDesignerEditor::WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	CDesignerEditor* editor = reinterpret_cast<CDesignerEditor*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
	if(msg == WM_NCCREATE) { editor = static_cast<CDesignerEditor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); editor->AddRef(); editor->m_window = hwnd; SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(editor)); }
	LRESULT result = editor ? editor->Message(hwnd,msg,wp,lp,0) : DefWindowProcW(hwnd,msg,wp,lp);
	if(msg == WM_NCDESTROY && editor) { SetWindowLongPtrW(hwnd,GWLP_USERDATA,0); editor->m_window = NULL; editor->Release(); }
	return result;
}
LRESULT CALLBACK CDesignerEditor::CanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	CDesignerEditor* editor = reinterpret_cast<CDesignerEditor*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
	if(msg == WM_NCCREATE) { editor = static_cast<CDesignerEditor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(editor)); }
	return editor ? editor->Message(hwnd,msg,wp,lp,1) : DefWindowProcW(hwnd,msg,wp,lp);
}
LRESULT CALLBACK CDesignerEditor::OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	CDesignerEditor* editor = reinterpret_cast<CDesignerEditor*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
	if(msg == WM_NCCREATE) { editor = static_cast<CDesignerEditor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(editor)); }
	return editor ? editor->Message(hwnd,msg,wp,lp,2) : DefWindowProcW(hwnd,msg,wp,lp);
}
LRESULT CDesignerEditor::Message(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT surface)
{
	POINT point = { static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp)) };
	if(surface == 0)
	{
		switch(msg)
		{
		case WM_CONTEXTMENU:
			if(reinterpret_cast<HWND>(wp) == m_list && m_document.SourceSite() && m_events)
			{
				POINT screen = { GET_X_LPARAM(lp),GET_Y_LPARAM(lp) };
				if(screen.x == -1 && screen.y == -1) { RECT rect; INT row = ListView_GetNextItem(m_list,-1,LVNI_SELECTED); if(row < 0) return 0; ListView_GetItemRect(m_list,row,&rect,LVIR_BOUNDS); screen.x = rect.left; screen.y = rect.bottom; ClientToScreen(m_list,&screen); }
				else { LVHITTESTINFO hit = {0}; hit.pt = screen; ScreenToClient(m_list,&hit.pt); INT row = ListView_HitTest(m_list,&hit); if(row < 0) return 0; ListView_SetItemState(m_list,-1,0,LVIS_SELECTED | LVIS_FOCUSED); ListView_SetItemState(m_list,row,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED); }
				HMENU menu = CreatePopupMenu();
				if(menu) { AppendMenuW(menu,MF_STRING,CreateOpenHandler,L"Create/Open Handler"); UINT command = TrackPopupMenu(menu,TPM_RETURNCMD | TPM_RIGHTBUTTON,screen.x,screen.y,0,m_window,NULL); DestroyMenu(menu); if(command) { HRESULT hr = OpenSelectedHandler(); if(FAILED(hr)) Error(hr,L"Create/Open Handler"); } }
				return 0;
			}
			if(reinterpret_cast<HWND>(wp) == m_tree || reinterpret_cast<HWND>(wp) == m_tray) { POINT screen = { GET_X_LPARAM(lp),GET_Y_LPARAM(lp) }; ContextMenu(reinterpret_cast<HWND>(wp),screen); return 0; }
			break;
		case WM_SIZE: Layout(); return 0;
		case WM_ERASEBKGND: return 1;
		case WM_PAINT:
			{ PAINTSTRUCT paint; HDC dc = BeginPaint(window,&paint); PaintBuffered(dc,&paint.rcPaint,0); EndPaint(window,&paint); } return 0;
		case WM_PRINTCLIENT: { RECT client; GetClientRect(window,&client); PaintBuffered(reinterpret_cast<HDC>(wp),&client,0); } return 0;
		case WM_SYSCOLORCHANGE:
		case WM_THEMECHANGED:
			RedrawWindow(m_list,NULL,NULL,RDW_INVALIDATE | RDW_ERASE);
			ArrangeSurface();
			break;
		case RefreshMessage: m_refreshPosted = FALSE; Refresh(TRUE); return 0;
		case CommitInlineMessage: if(m_editing && GetFocus() != m_inline) FinishInline(TRUE); return 0;
		case FocusInlineMessage:
			if(m_editing)
			{
				SetFocus(m_inline);
				if(m_editDropdown)
				{
					SendMessageW(m_inline,CB_SHOWDROPDOWN,TRUE,0);
				}
				else
				{
					SendMessageW(m_inline,EM_SETSEL,0,-1);
				}
			}
			return 0;
		case WM_NOTIFY:
			{
				NMHDR* notification = reinterpret_cast<NMHDR*>(lp);
				if(notification->hwndFrom == m_list && notification->code == NM_CUSTOMDRAW)
					return DrawInspector(reinterpret_cast<NMLVCUSTOMDRAW*>(lp));
				if(notification->hwndFrom == m_tools && notification->code == LVN_ITEMCHANGED && !m_changeDepth && !m_paletteMouse)
				{
					NMLISTVIEW* change = reinterpret_cast<NMLISTVIEW*>(lp);
					if((change->uChanged & LVIF_STATE) && (change->uNewState & LVIS_SELECTED) && !(change->uOldState & LVIS_SELECTED)) ChooseTool(change->iItem);
				}
				if(notification->hwndFrom == m_tools && notification->code == NM_CLICK && !m_changeDepth)
					ChooseTool(reinterpret_cast<NMITEMACTIVATE*>(lp)->iItem);
				if(notification->hwndFrom == m_tray && notification->code == LVN_ITEMCHANGED && !m_changeDepth)
				{
					NMLISTVIEW* change = reinterpret_cast<NMLISTVIEW*>(lp);
					if((change->uChanged & LVIF_STATE) && (change->uNewState & LVIS_SELECTED) && !(change->uOldState & LVIS_SELECTED) && change->lParam >= 0 && change->lParam < m_nodes.Length()) SelectID(m_nodes[change->lParam].id);
				}
				if(notification->hwndFrom == m_tray && notification->code == NM_DBLCLK &&
					reinterpret_cast<NMITEMACTIVATE*>(lp)->iItem >= 0 && CanExecute(ImageListEditor))
				{
					HRESULT hr = EditImageList();
					if(FAILED(hr))
					{
						Error(hr,L"Image List Editor");
					}
				}
				if(notification->hwndFrom == m_tray && notification->code == NM_DBLCLK && CanExecute(MenuEditor))
				{
					TStackRef<IQuadooComponent> component;
					TStackRef<IQuadooMenuItems> menu;
					if(SUCCEEDED(SelectedComponent(&component)) && SUCCEEDED(component->QueryInterface(__uuidof(IQuadooMenuItems),reinterpret_cast<PVOID*>(&menu))))
					{
						HRESULT hr = EditMenu();
						if(FAILED(hr))
						{
							Error(hr,L"Menu Editor");
						}
					}
				}
				if(notification->hwndFrom == m_tray && notification->code == LVN_GETINFOTIPW)
				{
					NMLVGETINFOTIPW* tip = reinterpret_cast<NMLVGETINFOTIPW*>(lp);
					LVITEMW item; ZeroMemory(&item,sizeof(item)); item.mask = LVIF_PARAM; item.iItem = tip->iItem;
					if(ListView_GetItem(m_tray,&item) && item.lParam >= 0 && item.lParam < m_nodes.Length())
					{
						CRString type, detail; TStackRef<IQuadooDesignPlaceholder> placeholder;
						IQuadooComponent* component = m_nodes[item.lParam].component;
						if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder))))
						{ QuadooForm::PlaceholderReason reason; if(SUCCEEDED(placeholder->GetReason(&reason,&detail))) TStrCchCpy(tip->pszText,tip->cchTextMax,detail.c_wstr()); }
						else if(SUCCEEDED(component->GetTypeName(&type))) TStrCchCpy(tip->pszText,tip->cchTextMax,type.c_wstr());
					}
				}
				if(notification->hwndFrom == m_tree && notification->code == TVN_GETINFOTIPW)
				{
					NMTVGETINFOTIPW* tip = reinterpret_cast<NMTVGETINFOTIPW*>(lp); sysint index = tip->lParam;
					if(index >= 0 && index < m_nodes.Length()) { TStackRef<IQuadooDesignPlaceholder> placeholder; if(SUCCEEDED(m_nodes[index].component->QueryInterface(__uuidof(IQuadooDesignPlaceholder),reinterpret_cast<PVOID*>(&placeholder)))) { CRString detail; QuadooForm::PlaceholderReason reason; if(SUCCEEDED(placeholder->GetReason(&reason,&detail))) TStrCchCpy(tip->pszText,tip->cchTextMax,detail.c_wstr()); } }
				}
				if(notification->hwndFrom == m_tree && notification->code == TVN_SELCHANGEDW && !m_changeDepth)
				{
					sysint index = reinterpret_cast<NMTREEVIEWW*>(lp)->itemNew.lParam;
					if(index >= 0 && index < m_nodes.Length())
					{
						SelectID(m_nodes[index].id,(GetKeyState(VK_SHIFT) & 0x8000) != 0);
					}
				}
				if(notification->hwndFrom == m_tabs && notification->code == TCN_SELCHANGE)
				{
					if(SUCCEEDED(FinishInline(TRUE))) { m_events = TabCtrl_GetCurSel(m_tabs) != 0; Inspector(); }
					else TabCtrl_SetCurSel(m_tabs,m_events ? 1 : 0);
				}
				if(notification->hwndFrom == m_list && notification->code == NM_DBLCLK) { INT row = reinterpret_cast<NMITEMACTIVATE*>(lp)->iItem; if(row >= 0) EditRow(row); }
				if(notification->hwndFrom == m_list && notification->code == LVN_KEYDOWN)
				{ WORD key = reinterpret_cast<NMLVKEYDOWN*>(lp)->wVKey; if(key == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000) && m_document.SourceSite() && m_events) { HRESULT hr = OpenSelectedHandler(); if(FAILED(hr)) Error(hr,L"Create/Open Handler"); } else if(key == VK_F2 || key == VK_RETURN || key == VK_F4) { INT row = ListView_GetNextItem(m_list,-1,LVNI_FOCUSED); if(row >= 0) EditRow(row,key == VK_F4); } }
			}
			return 0;
		case WM_LBUTTONDOWN:
			{
				RECT client, canvas; GetClientRect(m_window,&client); GetWindowRect(m_canvas,&canvas); MapWindowPoints(NULL,m_window,reinterpret_cast<POINT*>(&canvas),2);
				if(!m_embedded && abs(point.x - canvas.left + 3) <= 4) m_splitter = 1;
				else if(abs(point.x - canvas.right - 3) <= 4) m_splitter = 2;
				else if(!m_embedded && point.x < m_leftWidth && point.y >= m_treeHeight && point.y < m_treeHeight + 8) m_splitter = 3;
				if(m_splitter) SetCapture(m_window);
			}
			return 0;
		case WM_MOUSEMOVE:
			if(m_splitter) { RECT client; GetClientRect(m_window,&client); if(m_splitter == 1) m_leftWidth = max(120,min(500,point.x)); else if(m_splitter == 2) m_rightWidth = max(220,min(600,client.right - point.x)); else m_treeHeight = max(100,min(client.bottom - 140,point.y)); Layout(); } return 0;
		case WM_LBUTTONUP: if(m_splitter) { m_splitter = 0; ReleaseCapture(); } return 0;
		case WM_DESTROY: CloseSession(); return 0;
		}
	}
	else
	{
		switch(msg)
		{
		case WM_ERASEBKGND: return 1;
		case WM_PAINT:
			{ PAINTSTRUCT paint; HDC dc = BeginPaint(window,&paint); if(surface == 1) PaintBuffered(dc,&paint.rcPaint,1); else PaintOverlay(dc); EndPaint(window,&paint); } return 0;
		case WM_PRINTCLIENT: if(surface == 1) PaintCanvas(reinterpret_cast<HDC>(wp)); else PaintOverlay(reinterpret_cast<HDC>(wp)); return 0;
		case WM_SIZE: if(surface == 1 && !m_layingOut) ArrangeSurface(); return 0;
		case WM_HSCROLL: case WM_VSCROLL:
			if(surface == 1)
			{
				SCROLLINFO info = { sizeof(info),SIF_ALL }; INT bar = msg == WM_HSCROLL ? SB_HORZ : SB_VERT; GetScrollInfo(window,bar,&info); INT& position = bar == SB_HORZ ? m_scroll.X : m_scroll.Y;
				switch(LOWORD(wp)) { case SB_LINEUP: position -= 24; break; case SB_LINEDOWN: position += 24; break; case SB_PAGEUP: position -= info.nPage; break; case SB_PAGEDOWN: position += info.nPage; break; case SB_THUMBTRACK: case SB_THUMBPOSITION: position = info.nTrackPos; break; case SB_TOP: position = 0; break; case SB_BOTTOM: position = info.nMax; break; }
				ArrangeSurface(); return 0;
			}
			break;
		case WM_CONTEXTMENU: { POINT screen = { GET_X_LPARAM(lp),GET_Y_LPARAM(lp) }; ContextMenu(window,screen); } return 0;
		case WM_MOUSEWHEEL: m_scroll.Y = max(0,m_scroll.Y - MulDiv(static_cast<short>(HIWORD(wp)),48,WHEEL_DELTA)); ArrangeSurface(); return 0;
		case WM_LBUTTONDBLCLK:
			if(surface == 2 && SelectMenuAt(point) == S_OK && !m_readOnly)
			{
				HRESULT hr = EditMenu();
				if(FAILED(hr))
				{
					Error(hr,L"Menu Editor");
				}
			}
			return 0;
		case WM_LBUTTONDOWN:
			if(surface == 2)
			{
				MouseDown(point,(wp & MK_SHIFT) != 0);
			}
			return 0;
		case WM_MOUSEMOVE: if(surface == 2) MouseMove(point); return 0;
		case WM_LBUTTONUP: if(surface == 2) MouseUp(point,FALSE); return 0;
		case WM_CAPTURECHANGED: if(m_dragging) { POINT cursor; GetCursorPos(&cursor); ScreenToClient(m_canvas,&cursor); MouseUp(cursor,TRUE); } return 0;
		case WM_KEYDOWN: if(wp == VK_ESCAPE && m_dragging) { MouseUp(m_start,TRUE); return 0; } break;
		case WM_SETCURSOR:
			if(surface == 2)
			{
				POINT cursor; GetCursorPos(&cursor); ScreenToClient(m_canvas,&cursor); DWORD flags; INT grip = HitGrip(cursor,&flags); PCWSTR shape = IDC_ARROW;
				if(!m_readOnly)
				{
					if(m_tool)
						shape = IDC_CROSS;
					else if(grip == 0 || grip == 7)
						shape = IDC_SIZENWSE;
					else if(grip == 2 || grip == 5)
						shape = IDC_SIZENESW;
					else if(grip == 1 || grip == 6)
						shape = IDC_SIZENS;
					else if(grip == 3 || grip == 4)
						shape = IDC_SIZEWE;
				}
				SetCursor(LoadCursor(NULL,shape)); return TRUE;
			}
			break;
		case WM_THEMECHANGED:
		case WM_SYSCOLORCHANGE:
		case WM_SETTINGCHANGE:
			ArrangeSurface();
			return 0;
		}
	}
	return DefWindowProcW(window,msg,wp,lp);
}
}
