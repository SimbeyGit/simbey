#pragma once
#include "Runtime.h"
#include "Library\Core\Map.h"

namespace QF
{
	class CListView;
	class CListItem;
	class CListRequest;
	HRESULT ValidateListText(RSTRING text);
	class CListLink : public CBaseUnknown
	{
		TStackRef<CFactory> m_factory;
	public:
		CListView* view;
		DWORD thread;
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
		END_UNK_MAP
		CListLink(CFactory* factory);
		~CListLink();
		HRESULT Alive();
	};
	class CListColumn : public CBaseUnknown, public IQuadooListColumn
	{
	public:
		TStackRef<CListLink> link;
		GUID id;
		CRString caption;
		INT width;
		QuadooForm::ListColumnAlignment alignment;
		UINT display;
		BOOL attached;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooListColumn)
		END_UNK_MAP
		CListColumn(CListLink* owner);
		HRESULT Alive();
		STDMETHOD(GetID)(GUID* value);
		STDMETHOD(GetAttached)(BOOL* value);
		STDMETHOD(GetIndex)(UINT* value);
		STDMETHOD(GetCaption)(RSTRING* value);
		STDMETHOD(SetCaption)(RSTRING value);
		STDMETHOD(GetWidth)(INT* value);
		STDMETHOD(SetWidth)(INT value);
		STDMETHOD(GetAlignment)(QuadooForm::ListColumnAlignment* value);
		STDMETHOD(SetAlignment)(QuadooForm::ListColumnAlignment value);
		STDMETHOD(GetDisplayIndex)(UINT* value);
		STDMETHOD(SetDisplayIndex)(UINT value);
		STDMETHOD(AutoSize)(QuadooForm::ListColumnAutoSize mode);
	};
	class CListColumns : public CBaseUnknown, public IQuadooListColumns
	{
		TStackRef<CListLink> m_link;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooListColumns)
		END_UNK_MAP
		CListColumns(CListLink* link);
		STDMETHOD(GetCount)(UINT* count);
		STDMETHOD(GetAt)(UINT index, IQuadooListColumn** column);
		STDMETHOD(Find)(REFGUID id, IQuadooListColumn** column);
		STDMETHOD(Insert)(UINT index, RSTRING caption, IQuadooListColumn** column);
		STDMETHOD(Delete)(IQuadooListColumn* column);
		STDMETHOD(Move)(IQuadooListColumn* column, UINT index);
		STDMETHOD(Clear)(VOID);
	};
	class CListItem : public CBaseUnknown, public IQuadooListItem
	{
	public:
		TStackRef<CListLink> link;
		GUID id;
		CRString caption;
		TStackRef<IRStringArray> strings;
		TStackRef<IUnknown> data;
		INT image;
		INT state;
		UINT index;
		BOOL attached;
		BOOL selected;
		BOOL focused;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooListItem)
		END_UNK_MAP
		CListItem(CListLink* owner);
		HRESULT Alive();
		STDMETHOD(GetID)(GUID* value);
		STDMETHOD(GetAttached)(BOOL* value);
		STDMETHOD(GetIsVirtual)(BOOL* value);
		STDMETHOD(GetDataGeneration)(ULONGLONG* value);
		STDMETHOD(GetItemIndex)(UINT* value);
		STDMETHOD(GetCaption)(RSTRING* value);
		STDMETHOD(SetCaption)(RSTRING value);
		STDMETHOD(GetSubItems)(IRStringArray** value);
		STDMETHOD(SetSubItems)(IRStringArray* value);
		STDMETHOD(GetSubItem)(UINT index, RSTRING* value);
		STDMETHOD(SetSubItem)(UINT index, RSTRING value);
		STDMETHOD(GetImageIndex)(INT* value);
		STDMETHOD(SetImageIndex)(INT value);
		STDMETHOD(GetStateIndex)(INT* value);
		STDMETHOD(SetStateIndex)(INT value);
		STDMETHOD(GetChecked)(BOOL* value);
		STDMETHOD(SetChecked)(BOOL value);
		STDMETHOD(GetData)(IUnknown** value);
		STDMETHOD(SetData)(IUnknown* value);
		STDMETHOD(GetSelected)(BOOL* value);
		STDMETHOD(SetSelected)(BOOL value);
		STDMETHOD(GetFocused)(BOOL* value);
		STDMETHOD(SetFocused)(BOOL value);
	};
	class CListItems : public CBaseUnknown, public IQuadooListItems
	{
		TStackRef<CListLink> m_link;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooListItems)
		END_UNK_MAP
		CListItems(CListLink* link);
		STDMETHOD(GetCount)(UINT* value);
		STDMETHOD(SetCount)(UINT value);
		STDMETHOD(GetAt)(UINT index, IQuadooListItem** value);
		STDMETHOD(Find)(REFGUID id, IQuadooListItem** value);
		STDMETHOD(Insert)(UINT index, RSTRING caption, IQuadooListItem** value);
		STDMETHOD(Delete)(IQuadooListItem* value);
		STDMETHOD(Move)(IQuadooListItem* value, UINT index);
		STDMETHOD(Clear)(VOID);
	};
	class CListRequest : public CBaseUnknown, public IQuadooListItem
	{
	public:
		TStackRef<CListLink> link;
		UINT index;
		ULONGLONG generation;
		CRString caption;
		TStackRef<IRStringArray> strings;
		TStackRef<IUnknown> data;
		INT image;
		INT state;
		BOOL selected;
		BOOL focused;
		BOOL checkbox;
		BOOL writable;
		IUnknown* identity;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooListItem)
		END_UNK_MAP
		CListRequest(CListLink* owner, UINT row, ULONGLONG epoch, BOOL checks);
		~CListRequest();
		HRESULT Readable();
		HRESULT Writable();
		HRESULT Initialize();
		STDMETHOD(GetID)(GUID* value);
		STDMETHOD(GetAttached)(BOOL* value);
		STDMETHOD(GetIsVirtual)(BOOL* value);
		STDMETHOD(GetDataGeneration)(ULONGLONG* value);
		STDMETHOD(GetItemIndex)(UINT* value);
		STDMETHOD(GetCaption)(RSTRING* value);
		STDMETHOD(SetCaption)(RSTRING value);
		STDMETHOD(GetSubItems)(IRStringArray** value);
		STDMETHOD(SetSubItems)(IRStringArray* value);
		STDMETHOD(GetSubItem)(UINT index, RSTRING* value);
		STDMETHOD(SetSubItem)(UINT index, RSTRING value);
		STDMETHOD(GetImageIndex)(INT* value);
		STDMETHOD(SetImageIndex)(INT value);
		STDMETHOD(GetStateIndex)(INT* value);
		STDMETHOD(SetStateIndex)(INT value);
		STDMETHOD(GetChecked)(BOOL* value);
		STDMETHOD(SetChecked)(BOOL value);
		STDMETHOD(GetData)(IUnknown** value);
		STDMETHOD(SetData)(IUnknown* value);
		STDMETHOD(GetSelected)(BOOL* value);
		STDMETHOD(SetSelected)(BOOL value);
		STDMETHOD(GetFocused)(BOOL* value);
		STDMETHOD(SetFocused)(BOOL value);
	};
	class CListImageObserver : public CBaseUnknown, public IQuadooImageListObserver
	{
	public:
		CListView* view;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooImageListObserver)
		END_UNK_MAP
		CListImageObserver();
		STDMETHOD_(VOID, OnImageListChanged)(IQuadooImageList* images);
	};
	class CListView : public TWindowInterface<CWinControl,IQuadooWinControl>, public IQuadooListView,
		public IQuadooKeyboardInput
	{
		typedef TWindowInterface<CWinControl,IQuadooWinControl> Super;
		struct ImageBinding
		{
			TStackRef<IQuadooComponent> component;
			TStackRef<IQuadooImageList> source;
			TStackRef<IQuadooImageListSnapshot> snapshot;
			TStackRef<CListImageObserver> observer;
		};
		TStackRef<CListLink> m_link;
		TStackRef<CListColumns> m_collection;
		TStackRef<CListItems> m_items;
		UINT m_nativeSync;
		BOOL m_editing;
		BOOL m_redrawBlocked;
		ImageBinding m_images[3];
		UINT m_nativeDPI;
		UINT m_busy;
		UINT m_events;
		BOOL m_loaded;
		BOOL m_headerPending;
		BOOL m_imageChanging;
		BOOL m_imageRefreshing;
		UINT m_virtualCount;
		ULONGLONG m_generation;
		TArray<UINT> m_requests;
		TStackRef<IQuadooImageListSnapshot> m_virtualImages[3];
		UINT m_virtualNativeStyle;
		INT m_virtualEdit;
		ULONGLONG m_editGeneration;
		TMap<IUnknown*,BOOL> m_snapshots;
		struct VirtualState
		{
			UINT first;
			UINT last;
			UINT previous;
			UINT next;
		};
		TArray<VirtualState> m_virtualStates;
		HRESULT m_stateError;
		HRESULT NewRequest(UINT index, CListRequest** result);
		HRESULT DrainVirtualStates();
		HRESULT LoadColumns(IJSONValue* value);
		HRESULT ParseColumns(IJSONValue* value, TArray<CListColumn*>& columns);
		HRESULT WriteColumns(IJSONValue** value);
		VOID ReleaseColumns(TArray<CListColumn*>& columns);
		HRESULT ResolveColumn(IQuadooListColumn* column, UINT* index);
		HRESULT Rebuild();
		HRESULT ReplaceListWindow();
		HRESULT PopulateColumns();
		HRESULT ReadNativeColumns();
		HRESULT ResolveImages(UINT role);
		HRESULT AttachImages(UINT role, IQuadooComponent* component);
		VOID DetachImages(UINT role);
		HRESULT ApplyImageHandles();
		HRESULT ColumnClick(UINT index);
		HRESULT ContextPopup(POINT screen, HWND origin);
		HRESULT NotifyColumns();
		HRESULT UnsupportedRows();
		HRESULT LoadItems(IJSONValue* value);
		HRESULT ParseItems(IJSONValue* value, TArray<CListItem*>& result);
		HRESULT WriteItems(IJSONValue** value);
		VOID ReleaseItems(TArray<CListItem*>& values);
		HRESULT PopulateItems();
		HRESULT NotifyItems();
		HRESULT NewEvent(PCWSTR name, CEventArgs** args);
		HRESULT InvokeEvent(CEventArgs* args);
		HRESULT ItemEvent(PCWSTR name, CListItem* item, UINT mask = 0);
		HRESULT Changing(CListItem* item, UINT mask, UINT oldState, UINT newState, BOOL* cancel);
		HRESULT CheckProposal(CListItem* item, INT* state, BOOL* cancel);
		HRESULT SelectStates(CListItem* item, BOOL value, BOOL focus, BOOL exclusive);
		HRESULT ApplyStates(CListItem* item);
		HRESULT CheckItems(CListItem* focused);
		HRESULT ApplyOrder(TArray<CListItem*>& order);
		HRESULT CompareItems(CListItem* a, CListItem* b, UINT column, INT* result);
		LRESULT ItemNotification(NMHDR* header, BOOL* handled);
		LRESULT VirtualNotification(NMHDR* header, BOOL* handled);
		HRESULT VirtualNative();
		HRESULT VirtualWidth(UINT column, INT* width);
		HRESULT PopulateVirtual();
		HRESULT PrepareVirtualImages(IQuadooImageListSnapshot** source, IQuadooImageListSnapshot** result);
		HRESULT ImageHandles(IQuadooImageListSnapshot** source, IQuadooImageListSnapshot** derived, HIMAGELIST* handles);
		HRESULT CommitVirtualImages(IQuadooImageListSnapshot** images, HIMAGELIST* handles = NULL);
		HRESULT RefreshVirtualImages();
		INT VirtualImageIndex(INT image, BOOL checked);
		HRESULT VirtualCheckGesture(INT index, BOOL multiple);
		BOOL VirtualCheckHit(POINT point, INT* index);
		HRESULT VirtualUpdate(UINT index, UINT column, RSTRING text, BOOL checked, UINT mask);
		HRESULT VirtualSelection(INT index, BOOL value, BOOL focus, BOOL exclusive);
		HRESULT VirtualItemIndex(IQuadooListItem* item, UINT* index);
		HRESULT VirtualItemEvent(PCWSTR name, IQuadooListItem* item, UINT number = 0);
		VOID ReportVirtualError(HRESULT error, PCWSTR name);
		BOOL Bound(PCWSTR name);
	protected:
		virtual HRESULT CreateNative();
		virtual VOID NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended);
		virtual HRESULT NativeCreated();
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed);
		virtual HRESULT DestroyChildren();
		virtual HRESULT BeforeDestroy();
		virtual LRESULT Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
		virtual HRESULT SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value);
	public:
		TArray<CListColumn*> columns;
		TArray<CListItem*> rows;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooListView)
			UNK_INTERFACE(IQuadooKeyboardInput)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP
		CListView(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		~CListView();
		static const TypeSpec* Type();
		HRESULT Mutable();
		HRESULT Stored();
		HRESULT CheckboxMode();
		BOOL IsVirtual();
		BOOL VirtualAttached(UINT index, ULONGLONG generation);
		HRESULT ItemCount(UINT* count);
		HRESULT VirtualCount(UINT count, BOOL reset);
		HRESULT FetchVirtual(UINT index, UINT fields, UINT column, CListRequest** result);
		VOID ForgetRequest(IUnknown* identity);
		HRESULT ResolveItem(IQuadooListItem* item, UINT* index);
		HRESULT InsertItem(UINT index, RSTRING text, IQuadooListItem** item);
		HRESULT DeleteItem(IQuadooListItem* item);
		HRESULT MoveItem(IQuadooListItem* item, UINT index);
		HRESULT ClearItems();
		HRESULT EditItem(CListItem* item, UINT mask, RSTRING text, IRStringArray* strings, INT number, IUnknown* data);
		HRESULT RemapCells(UINT operation, UINT from, UINT to, TArray<IRStringArray*>& previous);
		VOID FinishCells(TArray<IRStringArray*>& previous, BOOL restore);
		HRESULT SyncHeader();
		HRESULT EditColumn(CListColumn* column, UINT field, RSTRING text, INT number);
		HRESULT InsertColumn(UINT index, RSTRING caption, IQuadooListColumn** column);
		HRESULT DeleteColumn(IQuadooListColumn* column);
		HRESULT MoveColumn(IQuadooListColumn* column, UINT index);
		HRESULT ClearColumns();
		HRESULT AutoSizeColumn(CListColumn* column, QuadooForm::ListColumnAutoSize mode);
		HRESULT RefreshImages();
		VOID ImageSourceChanged();
		STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value);
		STDMETHOD(BindEvents)(IQuadooConstructionContext* context);
		STDMETHOD(Arrange)(const QuadooForm::Rect* bounds, UINT dpi);
		STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
		STDMETHOD(OwnsKeyboardMessage)(const MSG* message, BOOL* owns);
		STDMETHOD(GetItems)(IQuadooListItems** items);
		STDMETHOD(GetColumns)(IQuadooListColumns** value);
		STDMETHOD(GetSelected)(IQuadooListItem** item);
		STDMETHOD(SetSelected)(IQuadooListItem* item);
		STDMETHOD(GetFocused)(IQuadooListItem** item);
		STDMETHOD(SetFocused)(IQuadooListItem* item);
		STDMETHOD(GetSelectedCount)(UINT* count);
		STDMETHOD(SetItemSelected)(UINT index, BOOL selected);
		STDMETHOD(GetNextSelected)(INT afterIndex, IQuadooListItem** item);
		STDMETHOD(GetImages)(QuadooForm::ListImageRole role, IQuadooComponent** images);
		STDMETHOD(SetImages)(QuadooForm::ListImageRole role, IQuadooComponent* images);
		STDMETHOD(BeginLabelEdit)(IQuadooListItem* item);
		STDMETHOD(EnsureVisible)(IQuadooListItem* item, BOOL partialOK);
		STDMETHOD(BeginUpdate)(VOID);
		STDMETHOD(EndUpdate)(VOID);
		STDMETHOD(Sort)(UINT columnIndex, BOOL ascending);
		STDMETHOD(Refresh)(VOID);
		STDMETHOD(RefreshRange)(UINT firstIndex, UINT lastIndex);
		STDMETHOD(GetDataGeneration)(ULONGLONG* generation);
		STDMETHOD(ResetVirtualData)(UINT count);
		STDMETHOD(UpdateVirtualText)(UINT index, UINT columnIndex, RSTRING text);
		STDMETHOD(UpdateVirtualChecked)(UINT index, BOOL checked);
	};
}
