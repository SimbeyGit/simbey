#include "TreeView.h"
#include "MenuRuntime.h"

namespace QF
{
	static const DWORD Stored = QuadooForm::PropertySerializable;
	static const PropertySpec treeProperties[] =
	{
		{L"Width",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,200,3},
		{L"Height",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,150,4},
		{L"BackColor",QuadooProperty::Color,Stored,static_cast<INT>(0x80000000 | COLOR_WINDOW),21},
		{L"Items",QuadooProperty::Collection,Stored,0,24},
		{L"Images",QuadooProperty::ObjectReference,Stored,0,25},
		{L"SelectedNode",QuadooProperty::String,Stored | QuadooForm::PropertyHidden,0,26},
		{L"CheckBoxes",QuadooProperty::Boolean,Stored,0,27},
		{L"ReadOnly",QuadooProperty::Boolean,Stored,1,28},
		{L"ShowLines",QuadooProperty::Boolean,Stored,1,29},
		{L"ShowButtons",QuadooProperty::Boolean,Stored,1,30},
		{L"ShowRoot",QuadooProperty::Boolean,Stored,1,31},
		{L"HideSelection",QuadooProperty::Boolean,Stored,0,32}
	};
	static const ArgumentSpec changingArgs[] =
	{
		{L"Cancel",QuadooForm::ValueBool,TRUE},
		{L"Node",QuadooForm::ValueObject,FALSE},
		{L"OldNode",QuadooForm::ValueObject,FALSE}
	};
	static const ArgumentSpec changeArgs[] =
	{
		{L"Node",QuadooForm::ValueObject,FALSE},
		{L"OldNode",QuadooForm::ValueObject,FALSE}
	};
	static const ArgumentSpec editArgs[] =
	{
		{L"Cancel",QuadooForm::ValueBool,TRUE},
		{L"Node",QuadooForm::ValueObject,FALSE},
		{L"Text",QuadooForm::ValueString,TRUE}
	};
	static const EventSpec treeEvents[] =
	{
		{L"OnChanging",QuadooForm::EventCancellable,3,changingArgs,20},
		{L"OnChange",0,2,changeArgs,21},
		{L"OnExpanding",QuadooForm::EventCancellable,2,changingArgs,22},
		{L"OnCollapsing",QuadooForm::EventCancellable,2,changingArgs,23},
		{L"OnExpanded",0,1,changeArgs,24},
		{L"OnCollapsed",0,1,changeArgs,25},
		{L"OnEditing",QuadooForm::EventCancellable,2,changingArgs,26},
		{L"OnEdited",QuadooForm::EventCancellable,3,editArgs,27},
		{L"OnCheck",0,1,changeArgs,28}
	};
	const TypeSpec* CTreeView::Type()
	{
		static const TypeSpec type = {L"TreeView",NativeWindowType(),treeProperties,
			ARRAYSIZE(treeProperties),treeEvents,ARRAYSIZE(treeEvents),IDI_PALETTE_TREEVIEW};
		return &type;
	}
	CTreeView::CTreeView(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		Super(factory,context,parent), m_selected(NULL), m_silent(0), m_eventDepth(0), m_loaded(FALSE)
	{
	}
	CTreeView::~CTreeView()
	{
		DetachImages();
		if(m_link)
		{
			m_link->tree = NULL;
		}
		ReleaseNodes(nodes);
	}
	HRESULT CTreeView::CreateNative()
	{
		HRESULT hr;
		INITCOMMONCONTROLSEX controls = {sizeof(controls),ICC_TREEVIEW_CLASSES};
		CheckIf(!InitCommonControlsEx(&controls),HrEnsureLastError());
		Check(Super::CreateNative());
	Cleanup:
		return hr;
	}
	VOID CTreeView::NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended)
	{
		Super::NativeStyle(name,style,extended);
		*name = WC_TREEVIEWW;
		*style |= WS_TABSTOP;
		*extended |= WS_EX_CLIENTEDGE;
		if(Boolean(L"ShowLines"))
		{
			*style |= TVS_HASLINES;
		}
		if(Boolean(L"ShowButtons"))
		{
			*style |= TVS_HASBUTTONS;
		}
		if(Boolean(L"ShowRoot"))
		{
			*style |= TVS_LINESATROOT;
		}
		if(!Boolean(L"HideSelection"))
		{
			*style |= TVS_SHOWSELALWAYS;
		}
		if(!Boolean(L"ReadOnly"))
		{
			*style |= TVS_EDITLABELS;
		}
	}
	HRESULT CTreeView::Populate(const TArray<CTreeNode*>& siblings, HTREEITEM parent)
	{
		HRESULT hr = S_OK;
		for(sysint n = 0; n < siblings.Length(); n++)
		{
			CTreeNode* node = siblings[n];
			TVINSERTSTRUCTW insert = {0};
			insert.hParent = parent;
			insert.hInsertAfter = TVI_LAST;
			insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_STATE;
			insert.item.pszText = const_cast<PWSTR>(node->text.c_wstr());
			insert.item.lParam = reinterpret_cast<LPARAM>(node);
			insert.item.iImage = node->image < 0 || node->image >= 0xfffe ? I_IMAGENONE : node->image;
			insert.item.iSelectedImage = node->selectedImage < 0 ? insert.item.iImage : node->selectedImage >= 0xfffe ? I_IMAGENONE : node->selectedImage;
			insert.item.stateMask = TVIS_STATEIMAGEMASK;
			insert.item.state = Boolean(L"CheckBoxes") ? INDEXTOSTATEIMAGEMASK(node->checked ? 2 : 1) : 0;
			node->item = reinterpret_cast<HTREEITEM>(SendMessageW(m_hwnd,TVM_INSERTITEMW,0,reinterpret_cast<LPARAM>(&insert)));
			CheckIf(!node->item,E_OUTOFMEMORY);
			Check(Populate(node->children,node->item));
			if(node->expanded)
			{
				TreeView_Expand(m_hwnd,node->item,TVE_EXPAND);
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::NativeCreated()
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		BOOL silent = FALSE;
		Check(Super::NativeCreated());
		TreeView_SetUnicodeFormat(m_hwnd,TRUE);
		if(Boolean(L"CheckBoxes"))
		{
			// Common controls require this after HWND creation, before items.
			SetLastError(ERROR_SUCCESS);
			LONG previous = SetWindowLongW(m_hwnd,GWL_STYLE,GetWindowLongW(m_hwnd,GWL_STYLE) | TVS_CHECKBOXES);
			CheckIf(!previous && GetLastError() != ERROR_SUCCESS,HrEnsureLastError());
			CheckIf(!TreeView_GetImageList(m_hwnd,TVSIL_STATE),E_OUTOFMEMORY);
		}
		if(!m_loaded)
		{
			m_link.Attach(__new CTreeLink(m_factory));
			CheckAlloc(m_link);
			m_link->tree = this;
			m_collection.Attach(__new CTreeNodes(m_link));
			CheckAlloc(m_collection);
			Check(StoredValue(L"Items",&value));
			// Parse before setting m_loaded; no replacement HWND during initial load.
			TStackRef<IJSONArray> array;
			Check(value->GetArray(&array));
			Check(ParseNodes(array,NULL,nodes,roots,1));
			m_loaded = TRUE;
			CRString selected;
			Check(StoredString(L"SelectedNode",&selected));
			if(RStrLen(selected))
			{
				GUID id;
				TStackRef<IQuadooTreeNode> node;
				value.Release();
				Check(JSONCreateString(selected,&value));
				Check(value->GetStringAsGuid(id));
				Check(m_collection->Find(id,&node));
				Check(Resolve(node,&m_selected));
			}
		}
		m_silent++;
		silent = TRUE;
		SendMessageW(m_hwnd,TVM_SETBKCOLOR,0,ResolveQuadooColor(Integer(L"BackColor")));
		SendMessageW(m_hwnd,TVM_SETTEXTCOLOR,0,ResolveQuadooColor(Integer(L"ForeColor")));
		Check(RefreshImages());
		for(CTreeNode* parent = m_selected ? m_selected->parent : NULL; parent; parent = parent->parent)
		{
			parent->expanded = TRUE;
		}
		Check(Populate(roots,TVI_ROOT));
		TreeView_SelectItem(m_hwnd,m_selected ? m_selected->item : NULL);
	Cleanup:
		if(silent)
		{
			m_silent--;
		}
		return hr;
	}
	HRESULT CTreeView::Rebuild()
	{
		HRESULT hr;
		TStackRef<CTreeView> keepAlive(this);
		TArray<HTREEITEM> handles;
		TArray<BOOL> expanded;
		TStackRef<IQuadooImageListSnapshot> oldImages(m_snapshot);
		BOOL silent = FALSE;
		for(sysint n = 0; n < nodes.Length(); n++)
		{
			Check(handles.Append(nodes[n]->item));
			Check(expanded.Append(nodes[n]->expanded));
		}
		m_silent++;
		silent = TRUE;
		hr = ReplaceNative();
		if(FAILED(hr))
		{
			for(sysint n = 0; n < nodes.Length(); n++)
			{
				nodes[n]->item = handles[n];
				nodes[n]->expanded = expanded[n];
			}
		}
	Cleanup:
		if(silent)
		{
			m_silent--;
		}
		return hr;
	}
	HRESULT CTreeView::Event(PCWSTR name, CTreeNode* node, CTreeNode* previous, BOOL* cancel, RSTRING* text)
	{
		HRESULT hr = S_OK;
		TStackRef<CTreeView> keepAlive(this);
		TStackRef<CTreeNode> keepNode(node);
		TStackRef<CTreeNode> keepPrevious(previous);
		BOOL entered = FALSE;
		if(cancel)
		{
			*cancel = FALSE;
		}
		CheckIfIgnore(m_silent || m_state != QuadooForm::Ready,S_OK);
		m_eventDepth++;
		entered = TRUE;
		for(sysint n = 0; n < m_bindings.Length(); n++)
		{
			if(!lstrcmpW(m_bindings[n].spec->name,name) && m_bindings[n].delegate)
			{
				TStackRef<IQuadooDelegate> delegate(m_bindings[n].delegate);
				TStackRef<CEventArgs> args;
				args.Attach(__new CEventArgs(m_bindings[n].spec,Control()));
				CheckAlloc(args);
				Check(args->Initialize());
				UINT offset = (m_bindings[n].spec->flags & QuadooForm::EventCancellable) ? 1 : 0;
				args->SetNativeObject(offset,static_cast<IQuadooTreeNode*>(node));
				if(text)
				{
					args->SetNativeString(2,*text);
				}
				else
				{
					args->SetNativeObject(offset + 1,static_cast<IQuadooTreeNode*>(previous));
				}
				Check(delegate->Invoke(args));
				if(cancel)
				{
					*cancel = args->Cancelled();
				}
				if(text)
				{
					RStrRelease(*text);
					*text = NULL;
					Check(args->GetString(2,text));
				}
				Check(CheckAlive());
				break;
			}
		}
	Cleanup:
		if(entered)
		{
			m_eventDepth--;
		}
		return hr;
	}
	STDMETHODIMP CTreeView::GetNodes(IQuadooTreeNodes** value)
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
	STDMETHODIMP CTreeView::GetSelected(IQuadooTreeNode** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckAlive());
		*value = m_selected;
		if(*value)
		{
			(*value)->AddRef();
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeView::SetSelected(IQuadooTreeNode* value)
	{
		HRESULT hr;
		TStackRef<CTreeView> keepAlive(this);
		TStackRef<CTreeNode> previous(m_selected);
		TStackRef<CTreeNode> retained;
		CTreeNode* node;
		TArray<CTreeNode*> ancestors;
		BOOL cancel;
		Check(Mutable());
		Check(Resolve(value,&node));
		CheckIfIgnore(node == m_selected,S_FALSE);
		retained = node;
		Check(Event(L"OnChanging",node,previous,&cancel));
		CheckIfIgnore(cancel,S_FALSE);
		Check(CheckAlive());
		for(CTreeNode* parent = node ? node->parent : NULL; parent; parent = parent->parent)
		{
			Check(ancestors.Append(parent));
		}
		for(sysint n = ancestors.Length(); n > 0; n--)
		{
			if(!ancestors[n - 1]->expanded)
			{
				hr = ExpandNode(ancestors[n - 1],TRUE);
				Check(hr);
				CheckIfIgnore(hr == S_FALSE,S_FALSE);
			}
		}
		m_silent++;
		BOOL selected = TreeView_SelectItem(m_hwnd,node ? node->item : NULL);
		m_silent--;
		CheckIf(!selected,E_FAIL);
		m_selected = node;
		Check(Changed());
		Check(Event(L"OnChange",node,previous));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeView::GetValueJSON(UINT index, IJSONValue** value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckThread());
		if(index == FindProperty(L"Items") && m_loaded)
		{
			Check(WriteNodes(roots,value));
		}
		else if(index == FindProperty(L"SelectedNode") && m_loaded)
		{
			CRString id;
			if(m_selected)
			{
				Check(GuidString(m_selected->id,&id));
			}
			else
			{
				Check(RStrCreateW(LSP(L""),&id));
			}
			Check(JSONCreateString(id,value));
		}
		else
		{
			Check(Super::GetValueJSON(index,value));
		}
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeView::GetValueString(UINT index, RSTRING* value)
	{
		HRESULT hr;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(CheckThread());
		if(index == FindProperty(L"SelectedNode") && m_loaded)
		{
			if(m_selected)
			{
				Check(GuidString(m_selected->id,value));
			}
			else
			{
				Check(RStrCreateW(LSP(L""),value));
			}
		}
		else
		{
			Check(Super::GetValueString(index,value));
		}
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value)
	{
		HRESULT hr;
		Check(Mutable());
		if(index == FindProperty(L"Items"))
		{
			CheckIf(type != QuadooProperty::Collection,DISP_E_TYPEMISMATCH);
			TStackRef<CTreeNode> previous(m_selected);
			Check(LoadNodes(value));
			Check(Changed());
			if(previous)
			{
				Check(Event(L"OnChange",NULL,previous));
			}
		}
		else if(index == FindProperty(L"SelectedNode"))
		{
			GUID id;
			CRString text;
			TStackRef<IQuadooTreeNode> node;
			CheckIf(!value,E_INVALIDARG);
			Check(value->GetString(&text));
			if(RStrLen(text))
			{
				Check(value->GetStringAsGuid(id));
				Check(m_collection->Find(id,&node));
			}
			Check(SetSelected(node));
		}
		else
		{
			Check(Super::SetValue(index,type,value));
		}
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::ApplyNative(PCWSTR name)
	{
		HRESULT hr;
		if(!lstrcmpW(name,L"Images"))
		{
			Check(ResolveImages());
		}
		else if(!lstrcmpW(name,L"CheckBoxes") || !lstrcmpW(name,L"ReadOnly") ||
			!lstrcmpW(name,L"ShowLines") || !lstrcmpW(name,L"ShowButtons") ||
			!lstrcmpW(name,L"ShowRoot") || !lstrcmpW(name,L"HideSelection"))
		{
			Check(Rebuild());
		}
		else
		{
			Check(Super::ApplyNative(name));
			if(!lstrcmpW(name,L"BackColor"))
			{
				SendMessageW(m_hwnd,TVM_SETBKCOLOR,0,ResolveQuadooColor(Integer(name)));
			}
			else if(!lstrcmpW(name,L"ForeColor"))
			{
				SendMessageW(m_hwnd,TVM_SETTEXTCOLOR,0,ResolveQuadooColor(Integer(name)));
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed)
	{
		HRESULT hr;
		Check(Super::ArrangeNative(bounds,changed));
		Check(RefreshImages());
	Cleanup:
		return hr;
	}
	HRESULT CTreeView::DestroyChildren()
	{
		DetachImages();
		if(m_link)
		{
			m_link->tree = NULL;
		}
		m_selected = NULL;
		roots.Clear();
		ReleaseNodes(nodes);
		return S_OK;
	}
	STDMETHODIMP CTreeView::BindEvents(IQuadooConstructionContext* context)
	{
		HRESULT hr;
		Check(ResolveImages());
		Check(Super::BindEvents(context));
	Cleanup:
		return hr;
	}
	STDMETHODIMP CTreeView::BeginLabelEdit(IQuadooTreeNode* value)
	{
		HRESULT hr;
		CTreeNode* node;
		QuadooForm::ContextMode mode;
		Check(Mutable());
		Check(m_context->GetMode(&mode));
		CheckIf(mode == QuadooForm::Design || Boolean(L"ReadOnly"),E_ACCESSDENIED);
		Check(Resolve(value,&node));
		CheckIf(!node,E_INVALIDARG);
		::SetFocus(m_hwnd);
		CheckIf(!SendMessageW(m_hwnd,TVM_EDITLABELW,0,reinterpret_cast<LPARAM>(node->item)),S_FALSE);
		hr = S_OK;
	Cleanup:
		return hr;
	}
	HRESULT RegisterTreeViewFactory(IQuadooControlRegistry* registry)
	{
		HRESULT hr;
		TStackRef<CFactory> factory;
		factory.Attach(__new TFactory<CTreeView>);
		CheckAlloc(factory);
		Check(factory->Initialize());
		Check(registry->RegisterFactory(factory));
	Cleanup:
		return hr;
	}
}
