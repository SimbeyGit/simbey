#pragma once
#include "Runtime.h"

namespace QF
{
	class CTreeView;
	class CTreeLink : public CBaseUnknown
	{
		TStackRef<CFactory> m_factory;
	public:
		CTreeView* tree;
		DWORD thread;
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
		END_UNK_MAP
		CTreeLink(CFactory* factory);
		~CTreeLink();
		HRESULT Alive();
	};
	class CTreeNode : public CBaseUnknown, public IQuadooTreeNode
	{
	public:
		TStackRef<CTreeLink> link;
		GUID id;
		CRString text;
		CTreeNode* parent;
		TArray<CTreeNode*> children;
		HTREEITEM item;
		INT image;
		INT selectedImage;
		BOOL expanded;
		BOOL checked;
		BOOL attached;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooTreeNode)
		END_UNK_MAP
		CTreeNode(CTreeLink* owner);
		HRESULT Alive();
		STDMETHOD(GetID)(GUID* value);
		STDMETHOD(GetAttached)(BOOL* value);
		STDMETHOD(GetParent)(IQuadooTreeNode** value);
		STDMETHOD(GetIndex)(UINT* value);
		STDMETHOD(GetChildCount)(UINT* value);
		STDMETHOD(GetChild)(UINT index, IQuadooTreeNode** value);
		STDMETHOD(GetText)(RSTRING* value);
		STDMETHOD(SetText)(RSTRING value);
		STDMETHOD(GetImageIndex)(INT* value);
		STDMETHOD(SetImageIndex)(INT value);
		STDMETHOD(GetSelectedImageIndex)(INT* value);
		STDMETHOD(SetSelectedImageIndex)(INT value);
		STDMETHOD(GetExpanded)(BOOL* value);
		STDMETHOD(SetExpanded)(BOOL value);
		STDMETHOD(GetChecked)(BOOL* value);
		STDMETHOD(SetChecked)(BOOL value);
	};
	class CTreeNodes : public CBaseUnknown, public IQuadooTreeNodes
	{
		TStackRef<CTreeLink> m_link;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooTreeNodes)
		END_UNK_MAP
		CTreeNodes(CTreeLink* link);
		STDMETHOD(GetCount)(UINT* count);
		STDMETHOD(GetAt)(UINT index, IQuadooTreeNode** node);
		STDMETHOD(Find)(REFGUID id, IQuadooTreeNode** node);
		STDMETHOD(Insert)(IQuadooTreeNode* parent, UINT index, RSTRING text, IQuadooTreeNode** node);
		STDMETHOD(Delete)(IQuadooTreeNode* node);
		STDMETHOD(Move)(IQuadooTreeNode* node, IQuadooTreeNode* parent, UINT index);
		STDMETHOD(Clear)(VOID);
	};
	class CTreeImageObserver : public CBaseUnknown, public IQuadooImageListObserver
	{
	public:
		CTreeView* tree;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooImageListObserver)
		END_UNK_MAP
		CTreeImageObserver();
		STDMETHOD_(VOID, OnImageListChanged)(IQuadooImageList* images);
	};
	class CTreeView : public TWindowInterface<CWinControl,IQuadooWinControl>, public IQuadooTreeView
	{
		typedef TWindowInterface<CWinControl,IQuadooWinControl> Super;
		TStackRef<CTreeLink> m_link;
		TStackRef<CTreeNodes> m_collection;
		TStackRef<CTreeImageObserver> m_imageObserver;
		TStackRef<IQuadooComponent> m_images;
		TStackRef<IQuadooImageList> m_imageList;
		TStackRef<IQuadooImageListSnapshot> m_snapshot;
		CTreeNode* m_selected;
		UINT m_silent;
		UINT m_eventDepth;
		BOOL m_loaded;
		HRESULT LoadNodes(IJSONValue* value);
		HRESULT ParseNodes(IJSONArray* array, CTreeNode* parent, TArray<CTreeNode*>& all,
			TArray<CTreeNode*>& siblings, UINT depth);
		HRESULT WriteNodes(const TArray<CTreeNode*>& siblings, IJSONValue** value);
		HRESULT Populate(const TArray<CTreeNode*>& siblings, HTREEITEM parent);
		HRESULT Rebuild();
		HRESULT ExpandNode(CTreeNode* node, BOOL expanded);
		HRESULT Resolve(IQuadooTreeNode* node, CTreeNode** result);
		HRESULT Event(PCWSTR name, CTreeNode* node, CTreeNode* previous = NULL,
			BOOL* cancel = NULL, RSTRING* text = NULL);
		HRESULT ResolveImages();
		HRESULT AttachImages(IQuadooComponent* component);
		VOID DetachImages();
		VOID ReleaseNodes(TArray<CTreeNode*>& nodes);
		VOID RemoveBranch(CTreeNode* node);
		UINT Depth(CTreeNode* node);
		UINT Height(CTreeNode* node);
	protected:
		virtual HRESULT CreateNative();
		virtual VOID NativeStyle(PCWSTR* name, DWORD* style, DWORD* extended);
		virtual HRESULT NativeCreated();
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed);
		virtual HRESULT DestroyChildren();
		virtual LRESULT Message(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
		virtual HRESULT SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value);
	public:
		TArray<CTreeNode*> nodes;
		TArray<CTreeNode*> roots;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooTreeView)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP
		CTreeView(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		~CTreeView();
		static const TypeSpec* Type();
		HRESULT Mutable();
		HRESULT Changed();
		HRESULT EditNode(CTreeNode* node, UINT property, RSTRING text, INT number);
		HRESULT InsertNode(IQuadooTreeNode* parent, UINT index, RSTRING text, IQuadooTreeNode** node);
		HRESULT DeleteNode(IQuadooTreeNode* node);
		HRESULT MoveNode(IQuadooTreeNode* node, IQuadooTreeNode* parent, UINT index);
		HRESULT ClearNodes();
		HRESULT RefreshImages();
		STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value);
		STDMETHOD(GetValueString)(UINT index, RSTRING* value);
		STDMETHOD(BindEvents)(IQuadooConstructionContext* context);
		STDMETHOD(GetNodes)(IQuadooTreeNodes** value);
		STDMETHOD(GetSelected)(IQuadooTreeNode** node);
		STDMETHOD(SetSelected)(IQuadooTreeNode* node);
		STDMETHOD(GetImages)(IQuadooComponent** images);
		STDMETHOD(SetImages)(IQuadooComponent* images);
		STDMETHOD(BeginLabelEdit)(IQuadooTreeNode* node);
		STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
	};
}
