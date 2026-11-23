#include "StatusBar.h"
#include "MenuRuntime.h"

namespace QF
{
	static const DWORD Stored = QuadooForm::PropertySerializable;
	static const DWORD Layout = Stored | QuadooForm::PropertyAffectsLayout;
	static const PropertySpec properties[] =
	{
		{L"Height",QuadooProperty::Int,Layout,22,4},
		{L"Align",QuadooProperty::Enum,Layout,QuadooForm::AlignBottom,7},
		{L"Panels",QuadooProperty::Collection,Stored,0,24,NULL,0,L"[]"},
		{L"Images",QuadooProperty::ObjectReference,Stored,0,25},
		{L"SizeGrip",QuadooProperty::Boolean,Stored,1,26}
	};
	const TypeSpec* CStatusBar::Type()
	{
		static const TypeSpec type = {L"StatusBar",NativeWindowType(),properties,ARRAYSIZE(properties),NULL,0,IDI_PALETTE_STATUSBAR};
		return &type;
	}
	HRESULT RegisterStatusBarFactory(IQuadooControlRegistry* registry)
	{
		HRESULT hr;
		TStackRef<CFactory> factory;
		factory.Attach(__new TFactory<CStatusBar>);
		CheckAlloc(factory);
		Check(factory->Initialize());
		Check(registry->RegisterFactory(factory));
	Cleanup:
		return hr;
	}
	BOOL CStatusBar::AcceptParent(IQuadooControlContainer* parent)
	{
		TStackRef<IQuadooForm> form;
		TStackRef<IQuadooFrame> frame;
		return parent && (SUCCEEDED(parent->QueryInterface(&form)) || SUCCEEDED(parent->QueryInterface(&frame)));
	}
	CStatusBar::CStatusBar(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		Super(factory,context,parent), m_imageBusy(FALSE)
	{
	}
	CStatusBar::~CStatusBar()
	{
		DetachImages();
		ClearPanels();
		if(m_link)
		{
			m_link->bar = NULL;
		}
	}
	HRESULT CStatusBar::CreateNative()
	{
		HRESULT hr;
		INITCOMMONCONTROLSEX controls = {sizeof(controls),ICC_BAR_CLASSES};
		CheckIf(!InitCommonControlsEx(&controls),HrEnsureLastError());
		// Docking is an invariant, including definitions supplied outside Studio.
		CheckIf(Integer(L"Align") != QuadooForm::AlignBottom,E_INVALIDARG);
		Check(Super::CreateNative());
	Cleanup:
		return hr;
	}
	VOID CStatusBar::NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended)
	{
		Super::NativeStyle(name,style,extended);
		*name = STATUSCLASSNAMEW;
		*style |= CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER;
		*style &= ~WS_TABSTOP;
		if(Boolean(L"SizeGrip"))
		{
			*style |= SBARS_SIZEGRIP;
		}
	}
	HRESULT CStatusBar::NativeCreated()
	{
		HRESULT hr;
		Check(Super::NativeCreated());
		if(!m_link)
		{
			TStackRef<IJSONValue> value;
			m_link.Attach(__new CStatusLink(m_factory));
			CheckAlloc(m_link);
			m_link->bar = this;
			m_panels.Attach(__new CStatusPanels(m_link,m_id));
			CheckAlloc(m_panels);
			Check(StoredValue(L"Panels",&value));
			Check(LoadPanels(value));
		}
		Check(RebuildPanels());
	Cleanup:
		return hr;
	}
	VOID CStatusBar::ClearPanels()
	{
		for(sysint n = 0; n < m_items.Length(); n++)
		{
			m_items[n]->Detach();
			m_items[n]->Release();
		}
		m_items.Clear();
		for(sysint n = 0; n < m_icons.Length(); n++)
		{
			if(m_icons[n])
			{
				DestroyIcon(m_icons[n]);
			}
		}
		m_icons.Clear();
	}
	HRESULT CStatusBar::BeforeDestroy()
	{
		return m_replacing ? QuadooForm::EInvalidState : S_OK;
	}
	HRESULT CStatusBar::DestroyChildren()
	{
		// CControl has entered Destroying before calling this hook. External
		// image sources may reenter during observer removal; they must see a
		// non-mutable owner and must not recursively begin another teardown.
		DetachImages();
		// The HWND is still alive. Detach every copied icon before freeing it.
		for(sysint n = 0; m_hwnd && n < m_icons.Length(); n++)
		{
			SendMessageW(m_hwnd,SB_SETICON,n,0);
		}
		ClearPanels();
		if(m_panels)
		{
			m_panels->Detach();
		}
		if(m_link)
		{
			m_link->bar = NULL;
		}
		return S_OK;
	}
	HRESULT CStatusBar::ReadPanels(IJSONValue** value)
	{
		HRESULT hr;
		TStackRef<IJSONArray> array;
		Check(JSONCreateArray(&array));
		for(sysint n = 0; n < m_items.Length(); n++)
		{
			TStackRef<IJSONObject> object;
			TStackRef<IJSONValue> entry;
			Check(m_items[n]->Serialize(&object));
			Check(JSONWrapObject(object,&entry));
			Check(array->Add(entry));
		}
		Check(JSONWrapArray(array,value));
	Cleanup:
		return hr;
	}
	HRESULT CStatusBar::LoadPanels(IJSONValue* value)
	{
		HRESULT hr = S_OK;
		TStackRef<IJSONArray> array;
		TStackRef<IJSONArray> serialized;
		TStackRef<IJSONValue> normalized;
		TArray<CStatusPanel*> candidate;
		CheckIf(m_imageBusy,QuadooForm::EInvalidState);
		CheckIf(!value,E_INVALIDARG);
		Check(JSONCreateArray(&serialized));
		if(value)
		{
			Check(value->GetArray(&array));
			CheckIf(array->Count() > StatusPanelLimit,E_INVALIDARG);
			for(sysint n = 0; n < array->Count(); n++)
			{
				TStackRef<IJSONValue> entry;
				TStackRef<IJSONObject> object;
				TStackRef<CStatusPanel> panel;
				GUID id;
				GUID group;
				Check(array->GetValue(n,&entry));
				CheckIf(!entry,E_INVALIDARG);
				Check(entry->GetObject(&object));
				panel.Attach(__new CStatusPanel(m_link));
				CheckAlloc(panel);
				Check(panel->Initialize(object));
				Check(panel->GetID(&id));
				Check(m_panels->GetID(&group));
				CheckIf(IsEqualGUID(id,m_id) || IsEqualGUID(id,group),QuadooForm::EInvalidDefinition);
				for(sysint p = 0; p < candidate.Length(); p++)
				{
					GUID previous;
					Check(candidate[p]->GetID(&previous));
					CheckIf(IsEqualGUID(previous,id),QuadooForm::EInvalidDefinition);
				}
				object.Release();
				entry.Release();
				Check(panel->Serialize(&object));
				Check(JSONWrapObject(object,&entry));
				Check(serialized->Add(entry));
				Check(candidate.Append(panel));
				panel.Detach();
			}
		}
		Check(JSONWrapArray(serialized,&normalized));
		m_items.Swap(candidate);
		hr = RebuildPanels();
		if(FAILED(hr) && m_state < QuadooForm::Destroying)
		{
			m_items.Swap(candidate);
			RebuildPanels();
		}
		Check(hr);
		Check(SetStoredValue(L"Panels",normalized));
	Cleanup:
		for(sysint n = 0; n < candidate.Length(); n++)
		{
			candidate[n]->Detach();
			candidate[n]->Release();
		}
		return hr;
	}
	HRESULT CStatusBar::ChangedPanels(BOOL* committed)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		TStackRef<CStatusBar> keepAlive(this);
		if(committed)
		{
			*committed = FALSE;
		}
		Check(CheckAlive());
		Check(ReadPanels(&value));
		Check(RebuildPanels());
		Check(SetStoredValue(L"Panels",value));
		Check(Dirty(L"Panels"));
		if(committed)
		{
			*committed = TRUE;
		}
		Check(FlushNotifications());
	Cleanup:
		return hr;
	}
	HRESULT CStatusBar::SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value)
	{
		HRESULT hr;
		TStackRef<CStatusBar> keepAlive(this);
		Check(CheckAlive());
		CheckIf(m_imageBusy,QuadooForm::EInvalidState);
		if(index == FindProperty(L"Panels"))
		{
			TStackRef<IJSONValue> current;
			CheckIf(type != QuadooProperty::Collection,DISP_E_TYPEMISMATCH);
			Check(ReadPanels(&current));
			CheckIfIgnore(!JSONCompare(current,value),S_FALSE);
			Check(LoadPanels(value));
			Check(Dirty(L"Panels"));
			Check(FlushNotifications());
		}
		else
		{
			if(index == FindProperty(L"Align"))
			{
				INT alignment;
				CheckIf(!value,E_INVALIDARG);
				Check(value->GetInteger(&alignment));
				CheckIf(alignment != QuadooForm::AlignBottom,E_INVALIDARG);
			}
			Check(Super::SetValue(index,type,value));
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::GetValueJSON(UINT index, IJSONValue** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckThread());
		if(index == FindProperty(L"Panels") && m_panels)
		{
			Check(ReadPanels(value));
		}
		else
		{
			Check(Super::GetValueJSON(index,value));
		}
	Cleanup:
		return hr;
	}
	HRESULT CStatusBar::ApplyNative(PCWSTR name)
	{
		HRESULT hr;
		if(!lstrcmpW(name,L"SizeGrip") && m_hwnd)
		{
			TArray<HICON> previous;
			// Both HWNDs can paint during replacement. Keep the previous HWND's
			// icons alive until ReplaceNative has destroyed it or restored it.
			m_icons.Swap(previous);
			hr = ReplaceNative();
			if(FAILED(hr))
			{
				m_icons.Swap(previous);
			}
			for(sysint n = 0; n < previous.Length(); n++)
			{
				if(previous[n])
				{
					DestroyIcon(previous[n]);
				}
			}
			Check(hr);
		}
		else if(!lstrcmpW(name,L"Images") && m_state >= QuadooForm::Binding)
		{
			Check(ResolveImages());
		}
		else
		{
			Check(Super::ApplyNative(name));
		}
	Cleanup:
		return hr;
	}
	HRESULT CStatusBar::ArrangeNative(const QuadooForm::Rect* bounds, BOOL)
	{
		HRESULT hr = S_OK;
		if(m_hwnd)
		{
			RECT viewport;
			RECT current;
			HWND parent = ::GetParent(m_hwnd);
			CheckIf(!GetClientRect(parent,&viewport),HrEnsureLastError());
			viewport.top = max(0,viewport.bottom - MulDiv(bounds->Height,m_dpi,96));
			CheckIf(!GetWindowRect(m_hwnd,&current),HrEnsureLastError());
			MapWindowPoints(NULL,parent,reinterpret_cast<POINT*>(&current),2);
			SendMessageW(m_hwnd,WM_SETFONT,reinterpret_cast<WPARAM>(m_font),TRUE);
			// Avoid a one-pixel gap caused by converting a scrollbar-reduced
			// client rectangle to logical units and back at fractional DPI.
			if(!EqualRect(&viewport,&current))
			{
				CheckIf(!SetWindowPos(m_hwnd,HWND_TOP,0,viewport.top,viewport.right,
					viewport.bottom - viewport.top,SWP_NOACTIVATE),HrEnsureLastError());
			}
		}
		Check(RebuildPanels());
	Cleanup:
		return hr;
	}
	LRESULT CStatusBar::Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
	{
		if(message == WM_NCHITTEST || message == WM_NCLBUTTONDOWN ||
			message == WM_NCLBUTTONDBLCLK || message == WM_SYSCOMMAND)
		{
			HWND parent = ::GetParent(window);
			DWORD style = static_cast<DWORD>(GetWindowLongW(parent,GWL_STYLE));
			// A native grip can otherwise start sizing the status HWND itself.
			// Test the current parent style so BorderStyle changes take effect
			// immediately. Frames and the designer's child form cannot be sized.
			if(!(style & WS_THICKFRAME) || (style & (WS_CHILD | WS_MAXIMIZE)))
			{
				if(message == WM_NCHITTEST)
				{
					*handled = TRUE;
					return HTCLIENT;
				}
				if((message == WM_SYSCOMMAND && (wp & 0xfff0) == SC_SIZE) ||
					((message == WM_NCLBUTTONDOWN || message == WM_NCLBUTTONDBLCLK) &&
					(wp == HTBOTTOMRIGHT || wp == HTBOTTOMLEFT)))
				{
					*handled = TRUE;
					return 0;
				}
			}
		}
		if(message == WM_SIZE && m_panels && !m_imageBusy)
		{
			ReportCallback(RebuildPanels(),L"Panels");
		}
		return Super::Message(window,message,wp,lp,content,handled);
	}
	STDMETHODIMP CStatusBar::BindEvents(IQuadooConstructionContext* context)
	{
		HRESULT hr;
		Check(ResolveImages());
		Check(Super::BindEvents(context));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::GetDesignCapabilities(DWORD* capabilities)
	{
		HRESULT hr;
		CheckIf(!capabilities,E_POINTER);
		*capabilities = 0;
		Check(CheckAlive());
		*capabilities = QuadooForm::DesignResizeHeight;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::GetUniquenessKey(GUID* key)
	{
		HRESULT hr = S_OK;
		CheckIf(!key,E_POINTER);
		*key = QuadooForm::StatusBarUniquenessKey;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::GetDockAlignment(QuadooForm::AlignMode* alignment)
	{
		HRESULT hr;
		CheckIf(!alignment,E_POINTER);
		*alignment = QuadooForm::AlignBottom;
		Check(CheckAlive());
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::GetPanels(IQuadooStatusPanels** panels)
	{
		HRESULT hr;
		CheckIf(!panels,E_POINTER);
		*panels = NULL;
		Check(CheckAlive());
		CheckIf(!m_panels,QuadooForm::EInvalidState);
		*panels = m_panels;
		(*panels)->AddRef();
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::GetSizeGrip(BOOL* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = FALSE;
		Check(CheckAlive());
		*value = Boolean(L"SizeGrip");
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::SetSizeGrip(BOOL value)
	{
		HRESULT hr;
		TStackRef<IJSONValue> json;
		Check(JSONCreateBool(value != FALSE,&json));
		Check(SetNamedValue(L"SizeGrip",QuadooProperty::Boolean,json));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::GetOutlineParentID(GUID* id)
	{
		HRESULT hr;
		CheckIf(!id,E_POINTER);
		*id = GUID_NULL;
		Check(CheckAlive());
		CheckIf(!m_parent,QuadooForm::EInvalidState);
		Check(m_parent->GetID(id));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::GetOutlineCount(UINT* count)
	{
		HRESULT hr;
		CheckIf(!count,E_POINTER);
		*count = 0;
		Check(CheckAlive());
		*count = 1;
	Cleanup:
		return hr;
	}
	STDMETHODIMP CStatusBar::GetOutlineItem(UINT index, IQuadooComponent** component)
	{
		HRESULT hr;
		CheckIf(!component,E_POINTER);
		*component = NULL;
		Check(CheckAlive());
		CheckIf(index,DISP_E_BADINDEX);
		Check(m_panels->QueryInterface(__uuidof(IQuadooComponent),reinterpret_cast<PVOID*>(component)));
	Cleanup:
		return hr;
	}
}
