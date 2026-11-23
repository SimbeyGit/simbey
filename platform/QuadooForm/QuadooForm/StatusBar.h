#pragma once
#include "Runtime.h"

namespace QF
{
	// Part index 255 is SB_SIMPLEID, so normal panels use indexes 0..254.
	const UINT StatusPanelLimit = 255;
	class CStatusBar;
	class CStatusLink : public CBaseUnknown
	{
		TStackRef<CFactory> m_factory;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IUnknown)
		END_UNK_MAP
		CStatusLink(CFactory* factory);
		~CStatusLink();
		DWORD thread;
		CStatusBar* bar;
		HRESULT Alive();
	};

	// Collection entries participate in the inspector/outline, not in the
	// component namespace or generated script member declarations.
	class CStatusEntry : public CBaseUnknown, public IQuadooComponent,
		public IQuadooProperties, public IQuadooComponentOutline
	{
		friend class CStatusPanels;
	protected:
		TStackRef<CStatusLink> m_link;
		GUID m_id;
		BOOL m_attached;
		virtual RSTRING EntryType() const = 0;
		virtual HRESULT EntryDescriptors(IQuadooPropertyDescriptors** descriptors) = 0;
		virtual HRESULT Read(UINT index, IJSONValue** value) = 0;
		virtual HRESULT Write(UINT index, IJSONValue* value) = 0;
		HRESULT Alive();
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooComponent)
			UNK_INTERFACE(IQuadooProperties)
			UNK_INTERFACE(IQuadooComponentOutline)
		END_UNK_MAP
		CStatusEntry(CStatusLink* link);
		VOID Detach();
		STDMETHOD(GetID)(GUID* id);
		STDMETHOD(GetName)(RSTRING* name);
		STDMETHOD(SetName)(RSTRING name);
		STDMETHOD(GetTypeName)(RSTRING* name);
		STDMETHOD(GetProperties)(IQuadooProperties** properties);
		STDMETHOD(GetState)(QuadooForm::ComponentState* state);
		STDMETHOD(Destroy)(VOID);
		STDMETHOD(GetDescriptors)(IQuadooPropertyDescriptors** descriptors);
		STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value);
		STDMETHOD(SetValueJSON)(UINT index, IJSONValue* value);
		STDMETHOD(GetValueInt)(UINT index, INT* value);
		STDMETHOD(GetValueLong)(UINT index, LONGLONG* value);
		STDMETHOD(GetValueString)(UINT index, RSTRING* value);
		STDMETHOD(GetValueBool)(UINT index, BOOL* value);
		STDMETHOD(GetValueFloat)(UINT index, DOUBLE* value);
		STDMETHOD(GetValueEvent)(UINT index, RSTRING* value);
		STDMETHOD(SetValueInt)(UINT index, INT value);
		STDMETHOD(SetValueLong)(UINT index, LONGLONG value);
		STDMETHOD(SetValueString)(UINT index, RSTRING value);
		STDMETHOD(SetValueBool)(UINT index, BOOL value);
		STDMETHOD(SetValueFloat)(UINT index, DOUBLE value);
		STDMETHOD(SetValueEvent)(UINT index, RSTRING value);
	};

	class CStatusPanel : public CStatusEntry, public IQuadooStatusPanel
	{
	protected:
		virtual RSTRING EntryType() const;
		virtual HRESULT EntryDescriptors(IQuadooPropertyDescriptors** descriptors);
		virtual HRESULT Read(UINT index, IJSONValue** value);
		virtual HRESULT Write(UINT index, IJSONValue* value);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooStatusPanel)
			UNK_CHAIN_TO(CStatusEntry)
		END_UNK_MAP
		CStatusPanel(CStatusLink* link);
		CRString text;
		INT width, image;
		HRESULT Initialize(IJSONObject* definition);
		HRESULT Serialize(IJSONObject** definition);
		STDMETHOD(GetName)(RSTRING* name);
		STDMETHOD(GetIndex)(UINT* index);
		STDMETHOD(GetText)(RSTRING* value);
		STDMETHOD(SetText)(RSTRING value);
		STDMETHOD(GetWidth)(INT* value);
		STDMETHOD(SetWidth)(INT value);
		STDMETHOD(GetIcon)(INT* value);
		STDMETHOD(SetIcon)(INT value);
		STDMETHOD(GetOutlineParentID)(GUID* id);
		STDMETHOD(GetOutlineCount)(UINT* count);
		STDMETHOD(GetOutlineItem)(UINT index, IQuadooComponent** component);
	};

	class CStatusPanels : public CStatusEntry, public IQuadooStatusPanels
	{
	protected:
		virtual RSTRING EntryType() const;
		virtual HRESULT EntryDescriptors(IQuadooPropertyDescriptors** descriptors);
		virtual HRESULT Read(UINT index, IJSONValue** value);
		virtual HRESULT Write(UINT index, IJSONValue* value);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooStatusPanels)
			UNK_CHAIN_TO(CStatusEntry)
		END_UNK_MAP
		CStatusPanels(CStatusLink* link, REFGUID owner);
		STDMETHOD(GetCount)(UINT* count);
		STDMETHOD(GetAt)(UINT index, IQuadooStatusPanel** panel);
		STDMETHOD(Insert)(UINT index, IQuadooStatusPanel** panel);
		STDMETHOD(Delete)(IQuadooStatusPanel* panel);
		STDMETHOD(Move)(IQuadooStatusPanel* panel, UINT index);
		STDMETHOD(Clear)(VOID);
		STDMETHOD(GetOutlineParentID)(GUID* id);
		STDMETHOD(GetOutlineCount)(UINT* count);
		STDMETHOD(GetOutlineItem)(UINT index, IQuadooComponent** component);
	};

	class CStatusImageObserver : public CBaseUnknown, public IQuadooImageListObserver
	{
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooImageListObserver)
		END_UNK_MAP
		CStatusImageObserver();
		CStatusBar* bar;
		STDMETHOD_(VOID, OnImageListChanged)(IQuadooImageList* images);
	};

	class CStatusBar : public TWindowInterface<CWinControl,IQuadooWinControl>,
		public IQuadooStatusBar, public IQuadooUniqueComponent,
		public IQuadooViewportDock, public IQuadooComponentOutline
	{
		typedef TWindowInterface<CWinControl,IQuadooWinControl> Super;
		friend class CStatusEntry;
		friend class CStatusPanel;
		friend class CStatusPanels;
		TStackRef<CStatusLink> m_link;
		TStackRef<CStatusPanels> m_panels;
		TArray<CStatusPanel*> m_items;
		TArray<HICON> m_icons;
		TStackRef<IQuadooComponent> m_images;
		TStackRef<IQuadooImageList> m_imageList;
		TStackRef<CStatusImageObserver> m_imageObserver;
		BOOL m_imageBusy;
		HRESULT ReadPanels(IJSONValue** value);
		HRESULT LoadPanels(IJSONValue* value);
		HRESULT ChangedPanels(BOOL* committed = NULL);
		HRESULT RebuildPanels();
		HRESULT ResolveImages();
		VOID DetachImages();
		VOID ClearPanels();
		HRESULT Locate(IQuadooStatusPanel* panel, UINT* index);
	protected:
		virtual HRESULT CreateNative();
		virtual VOID NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended);
		virtual HRESULT NativeCreated();
		virtual HRESULT BeforeDestroy();
		virtual HRESULT DestroyChildren();
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed);
		virtual HRESULT SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value);
		virtual LRESULT Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooStatusBar)
			UNK_INTERFACE(IQuadooUniqueComponent)
			UNK_INTERFACE(IQuadooViewportDock)
			UNK_INTERFACE(IQuadooComponentOutline)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP
		CStatusBar(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		~CStatusBar();
		static const TypeSpec* Type();
		static BOOL AcceptParent(IQuadooControlContainer* parent);
		HRESULT RefreshImages();
		STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value);
		STDMETHOD(BindEvents)(IQuadooConstructionContext* context);
		STDMETHOD(GetDesignCapabilities)(DWORD* capabilities);
		STDMETHOD(GetUniquenessKey)(GUID* key);
		STDMETHOD(GetDockAlignment)(QuadooForm::AlignMode* alignment);
		STDMETHOD(GetPanels)(IQuadooStatusPanels** panels);
		STDMETHOD(GetImages)(IQuadooComponent** images);
		STDMETHOD(SetImages)(IQuadooComponent* images);
		STDMETHOD(GetSizeGrip)(BOOL* value);
		STDMETHOD(SetSizeGrip)(BOOL value);
		STDMETHOD(GetOutlineParentID)(GUID* id);
		STDMETHOD(GetOutlineCount)(UINT* count);
		STDMETHOD(GetOutlineItem)(UINT index, IQuadooComponent** component);
	};
	HRESULT RegisterStatusBarFactory(IQuadooControlRegistry* registry);
}
