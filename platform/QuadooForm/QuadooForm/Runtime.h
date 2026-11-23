#pragma once

#include <windows.h>
#include <commctrl.h>
#include "PaletteResources.h"
#include "Library\Core\BaseUnknown.h"
#include "Library\Core\Array.h"
#include "Library\Core\RStrMap.h"
#include "Library\Util\Formatting.h"
#include "..\Published\QuadooForm.h"
#include "DestroyedObservers.h"
#include "Published\SimbeyCore.h"
#include "..\Imaging\ImageResource.h"

extern HINSTANCE g_hInstance;

namespace QF
{
	// Metadata is composed through base descriptors. A factory owns the flattened
	// descriptor sequence, and instances allocate exactly that sequence of values.
	// order preserves published descriptor indexes when composing base metadata.
	struct EnumSpec { PCWSTR name; INT value; };
	// initialText initializes text properties, or supplies a Collection's default JSON.
	struct PropertySpec { PCWSTR name; QuadooProperty::Type type; DWORD flags; INT initial; UINT order; const EnumSpec* choices; UINT choiceCount; PCWSTR initialText; INT minimum, maximum; BOOL hasRange; };
	struct ArgumentSpec { PCWSTR name; QuadooForm::EventValueType type; BOOL writable; };
	struct EventSpec { PCWSTR name; DWORD flags; UINT count; const ArgumentSpec* args; UINT order; };
	struct TypeSpec
	{
		PCWSTR name;
		const TypeSpec* base;
		const PropertySpec* properties; UINT propertyCount;
		const EventSpec* events; UINT eventCount;
		UINT paletteIcon;
	};
	extern const TypeSpec FormType, FrameType, PanelType, ScrollingPanelType, LabelType, ButtonType, TextBoxType, MemoType;
	const UINT KeyboardRefreshMessage = WM_APP + 0x347;
	VOID RequestKeyboardRefresh(IQuadooControl* control);
	VOID NoteHandledKey(IQuadooControl* control, HWND target, LPARAM flags);
	BOOL KeyboardComposing(HWND window);
	interface __declspec(uuid("BEBAC027-72BA-418D-938B-4A12F4512712")) IKeyboardState : IUnknown
	{
		STDMETHOD(NoteHandled)(HWND target, UINT scan) = 0;
	};
	extern const TypeSpec CheckBoxType, RadioButtonType, GroupBoxType;
	extern const TypeSpec ComboBoxType, ListBoxType;
	extern const TypeSpec TabControlType, TabPageType;
	extern const TypeSpec ImageType, ColorSwatchType;
	HRESULT NormalizeItems(IJSONValue* value, IJSONValue** normalized);
	HRESULT NormalizeIcon(IJSONValue* value, IJSONValue** normalized);
	BOOL SameString(RSTRING a, RSTRING b);
	BOOL SameObject(IUnknown* a, IUnknown* b);
	UINT SystemDPI(HWND hwnd);
	UINT ControlDPI(IQuadooControl* control);
	BOOL AdjustForWindowDPI(RECT* rect, DWORD style, DWORD extended, HWND window);
	HRESULT CloneObject(IJSONObject* source, IJSONObject** result);
	HRESULT GuidString(REFGUID id, RSTRING* text);
	HRESULT PutInt(IJSONObject* object, PCWSTR name, INT value);
	HRESULT PutString(IJSONObject* object, PCWSTR name, RSTRING value);
	HRESULT PutObject(IJSONObject* object, PCWSTR name, IJSONObject* value);
	HRESULT ReadInt(IJSONObject* object, PCWSTR name, INT initial, INT* value);
	HRESULT ReadString(IJSONObject* object, PCWSTR name, PCWSTR initial, RSTRING* value);
	HRESULT DefaultValue(const PropertySpec* spec, IJSONValue** value);
	HRESULT NormalizeValue(const PropertySpec* spec, IJSONValue* value, IJSONValue** normalized);
	HRESULT GetNamespace(IQuadooControl* control, IQuadooControlContainer** root);
	HRESULT IndexTree(IQuadooControl* root, TRStrMap<IQuadooControl*>& names, TArray<GUID>& ids,
		IQuadooControl* substitute = NULL, RSTRING newName = NULL, BOOL isRoot = TRUE, UINT depth = 0);
	HRESULT ValidateCandidate(IQuadooControlContainer* parent, RSTRING name, REFGUID id, BOOL frame);
	HRESULT ValidateVisualComponentIdentity(IQuadooControlContainer* root, RSTRING name, REFGUID id, BOOL frameAlias = FALSE);
	HRESULT CreateDescriptors(const TArray<const PropertySpec*>& properties, const TArray<const EventSpec*>& events, IQuadooPropertyDescriptors** descriptors);
	HRESULT CreateEventDescriptors(const TArray<const EventSpec*>& events, IQuadooEventDescriptors** descriptors);
	HRESULT LoadPaletteResource(UINT resource, UINT cx, UINT cy, HICON* icon);
	HRESULT ComposeMetadata(const TypeSpec* type, TArray<const PropertySpec*>& properties, TArray<const EventSpec*>& events, BOOL visual = TRUE);
	HRESULT LayoutProperty(IQuadooControl* control, PCWSTR name, IJSONValue** value);
	INT LayoutInt(IQuadooControl* control, PCWSTR name, INT initial);
	HRESULT FlushTree(IQuadooControl* control);
	HRESULT ValidateDescriptors(IQuadooPropertyDescriptors* properties, IQuadooEventDescriptors* events = NULL, BOOL visual = TRUE);
	HRESULT ValidateComponentFactory(IQuadooComponentFactory* factory);
	HRESULT ValidateComponentMetadata(IQuadooComponent* component, IQuadooComponentFactory* factory = NULL);
	HRESULT CreateComponentCollection(IQuadooControlContainer* root, IQuadooComponentCollection** collection);
	HRESULT DestroyComponentCollection(IQuadooComponentCollection* collection, BOOL abort = FALSE);
	HRESULT CanDestroyComponentCollection(IQuadooComponentCollection* collection);
	HRESULT DispatchComponentMessage(IQuadooControlContainer* root, HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
	HRESULT RegisterTreeViewFactory(IQuadooControlRegistry* registry);
	HRESULT RegisterListViewFactory(IQuadooControlRegistry* registry);
	const TypeSpec* NativeWindowType();
	HRESULT RegisterNonvisualFactories(IQuadooComponentRegistry* registry);
	HRESULT RegisterTimerFactory(IQuadooComponentRegistry* registry);
	HRESULT RegisterMenuFactories(IQuadooComponentRegistry* registry);
	HRESULT ValidateUniqueComponent(IQuadooControlContainer* root, IQuadooComponent* candidate);
	HRESULT ValidateMenuGraph(IQuadooControl* root);
	HRESULT ValidateMenuReference(IQuadooControl* control, PCWSTR name, IJSONValue* value, BOOL resolve);
	HRESULT ShowControlPopup(IQuadooControl* control, POINT screen, BOOL* handled);
	class CNativeMenu;
	const UINT MenuRefreshMessage = WM_APP + 0x348;
	HRESULT BuildComponent(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IJSONObject* definition, IQuadooComponent** result);
	HRESULT CompleteComponent(IQuadooConstructionContext* context, IQuadooComponent* component);
	HRESULT CreateDesignPlaceholder(IQuadooConstructionContext* context, IQuadooControlContainer* parent, IQuadooComponentCollection* owner,
		IJSONObject* definition, IQuadooPropertyDescriptors* properties, IQuadooEventDescriptors* events, IUnknown* factory,
		QuadooForm::PlaceholderReason reason, IQuadooComponent** result);
	HRESULT ValidateFactory(IQuadooControlFactory* factory);
	HRESULT ValidateControlMetadata(IQuadooControl* control, IQuadooControlFactory* factory = NULL);
	template<class T> HRESULT QueryOptional(IUnknown* source, T** result)
	{
		HRESULT hr;
		CheckIf(!source || !result, E_POINTER); *result = NULL;
		hr = source->QueryInterface(__uuidof(T), reinterpret_cast<PVOID*>(result));
		if(hr == E_NOINTERFACE) hr = *result ? E_UNEXPECTED : S_OK;
		else if(hr == S_OK && !*result) hr = E_UNEXPECTED;
		else if(SUCCEEDED(hr) && hr != S_OK) hr = E_UNEXPECTED;
	Cleanup:
		return hr;
	}

	// Optional implementation services exchange values, never concrete pointers.
	// External controls need only the published interfaces; these are not required
	// by construction, registration, layout, events, or persistence.
	interface __declspec(uuid("BEBAC027-72BA-418D-938B-4A12F4512701")) IUpdateParticipant : IUnknown
	{
		STDMETHOD(IsUpdatePending)(BOOL* pending) = 0;
		STDMETHOD(FlushPending)(VOID) = 0;
		STDMETHOD(GetLayoutDPI)(UINT* dpi) = 0;
		STDMETHOD(RecordCallbackError)(HRESULT error) = 0;
		STDMETHOD(IsBatchPending)(BOOL* pending) = 0;
	};
	interface __declspec(uuid("BEBAC027-72BA-418D-938B-4A12F4512702")) IDefinitionSource : IUnknown
	{
		STDMETHOD(GetDefinitionTemplate)(IJSONObject** definition) = 0;
	};
	class CEventArgs : public CBaseUnknown, public IQuadooEventArgs, public IQuadooEventObjectArgs
	{
		struct Value
		{
			LONGLONG number;
			RSTRING text;
			IQuadooComponent* component;
			IUnknown* object;
		};
		const EventSpec* m_spec;
		IQuadooComponent* m_sender;
		TArray<Value> m_values;
		DWORD m_thread;
		HRESULT Validate(UINT index, QuadooForm::EventValueType type, BOOL write);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooEventArgs)
			UNK_INTERFACE(IQuadooEventObjectArgs)
		END_UNK_MAP
		CEventArgs(const EventSpec* spec, IQuadooComponent* sender);
		~CEventArgs();
		HRESULT Initialize();
		VOID SetNativeValue(UINT index, LONGLONG value);
		VOID SetNativeObject(UINT index, IUnknown* object);
		VOID SetNativeString(UINT index, RSTRING text);
		STDMETHOD(GetObject)(UINT index, IUnknown** object);
		STDMETHOD(SetObject)(UINT index, IUnknown* object);
		BOOL Cancelled() const;
		STDMETHOD(GetEventName)(RSTRING* name);
		STDMETHOD(GetSender)(IQuadooComponent** sender);
		STDMETHOD(GetArgumentCount)(UINT* count);
		STDMETHOD(GetArgumentName)(UINT index, RSTRING* name);
		STDMETHOD(GetArgumentType)(UINT index, QuadooForm::EventValueType* type);
		STDMETHOD(GetInt)(UINT index, INT* value);
		STDMETHOD(SetInt)(UINT index, INT value);
		STDMETHOD(GetLong)(UINT index, LONGLONG* value);
		STDMETHOD(SetLong)(UINT index, LONGLONG value);
		STDMETHOD(GetBool)(UINT index, BOOL* value);
		STDMETHOD(SetBool)(UINT index, BOOL value);
		STDMETHOD(GetString)(UINT index, RSTRING* value);
		STDMETHOD(SetString)(UINT index, RSTRING value);
		STDMETHOD(GetComponent)(UINT index, IQuadooComponent** value);
		STDMETHOD(SetComponent)(UINT index, IQuadooComponent* value);
	};


	class CFactory;
	class CControl : public CBaseUnknown, public IQuadooProperties, public IQuadooComponentLifecycle,
		public IQuadooPropertyNotifications, public IQuadooConstructionParticipant,
		public IQuadooLayoutControl, public IUpdateParticipant, public IDefinitionSource, public IQuadooDesignCapabilities
	{
	protected:
		struct PropertySlot { const PropertySpec* spec; IJSONValue* value; BOOL dirty; };
		struct Binding { const EventSpec* spec; IQuadooDelegate* delegate; };
		DWORD m_thread;
		GUID m_id;
		CRString m_name;
		IQuadooControlContainer* m_parent;
		IQuadooConstructionContext* m_context;
		CFactory* m_factory;
		IJSONObject* m_definition;
		IJSONObject* m_eventNames;
		TArray<PropertySlot> m_values;
		TArray<IQuadooPropertyObserver*> m_observers;
		CDestroyedObservers m_destroyedObservers;
		TArray<Binding> m_bindings;
		TArray<BOOL> m_eventDirty;
		QuadooForm::ComponentState m_state;
		QuadooForm::Rect m_bounds, m_effective;
		HFONT m_font;
		UINT m_dpi, m_updates;
		BOOL m_boundsDirty, m_layoutBusy, m_notifying, m_abort, m_autoDPI;
		HRESULT m_callbackError;
		HRESULT CheckThread() const;
		HRESULT CheckAlive() const;
		UINT FindProperty(PCWSTR name) const;
		HRESULT StoredValue(PCWSTR name, IJSONValue** value);
		HRESULT StoredString(PCWSTR name, RSTRING* value);
		HRESULT SetStoredValue(PCWSTR name, IJSONValue* value);
		HRESULT Dirty(PCWSTR name);
		HRESULT ResolvePropertyIndex(UINT index, UINT* property, const EventSpec** event) const;
		HRESULT Value(UINT index, QuadooProperty::Type type, IJSONValue** value);
		virtual HRESULT SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value);
		HRESULT SetNamedValue(PCWSTR name, QuadooProperty::Type type, IJSONValue* value);
		HRESULT LoadProperties(IJSONObject* definition);
		virtual HRESULT CreateNative();
		HRESULT RecreateFont();
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT PropertyChanged(PCWSTR name);
		virtual HRESULT DestroyChildren();
		virtual HRESULT BeforeDestroy() { return S_OK; }
		virtual HRESULT DestroyNative();
		virtual HRESULT ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed);
		virtual HRESULT MeasureContent(QuadooForm::Size* size);
		HRESULT QueueChange(PCWSTR name);
		HRESULT FlushNotifications();
		BOOL Updating(BOOL includeLayout = TRUE) const;
		HRESULT RequestLayout();
		HRESULT Dispatch(PCWSTR name, BOOL* cancelled = NULL, LONGLONG value = 0);
		VOID ClearBindings();
		VOID ReportCallback(HRESULT error, PCWSTR eventName);

	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooProperties)
			UNK_INTERFACE(IQuadooPropertyNotifications)
			UNK_INTERFACE(IQuadooComponentLifecycle)
			UNK_INTERFACE(IQuadooConstructionParticipant)
			UNK_INTERFACE(IQuadooLayoutControl)
			UNK_INTERFACE(IUpdateParticipant)
			UNK_INTERFACE(IDefinitionSource)
			UNK_INTERFACE(IQuadooDesignCapabilities)
		END_UNK_MAP

	public:
		CControl(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		virtual ~CControl();
		virtual IQuadooControl* Control() = 0;
		static BOOL AcceptParent(IQuadooControlContainer* parent);
		HRESULT Construct(IJSONObject* definition);
		INT Integer(PCWSTR name) const;
		BOOL Boolean(PCWSTR name) const;
		HRESULT Insets(PCWSTR name, QuadooForm::Insets* insets) const;
		STDMETHOD(GetID)(GUID* id);
		STDMETHOD(GetName)(RSTRING* name);
		STDMETHOD(SetName)(RSTRING name);
		STDMETHOD(GetTypeName)(RSTRING* name);
		STDMETHOD(GetProperties)(IQuadooProperties** properties);
		STDMETHOD(GetState)(QuadooForm::ComponentState* state);
		STDMETHOD(Destroy)(VOID);
		STDMETHOD(GetParent)(IQuadooControlContainer** parent);
		STDMETHOD(GetBounds)(QuadooForm::Rect* bounds);
		STDMETHOD(SetBounds)(const QuadooForm::Rect* bounds);
		STDMETHOD(GetEffectiveBounds)(QuadooForm::Rect* bounds);
		STDMETHOD(GetVisible)(BOOL* visible);
		STDMETHOD(SetVisible)(BOOL visible);
		STDMETHOD(GetEnabled)(BOOL* enabled);
		STDMETHOD(SetEnabled)(BOOL enabled);
		STDMETHOD(GetDescriptors)(IQuadooPropertyDescriptors** descriptors);
		STDMETHOD(GetValueInt)(UINT index, INT* value);
		STDMETHOD(GetValueLong)(UINT index, LONGLONG* value);
		STDMETHOD(GetValueString)(UINT index, RSTRING* value);
		STDMETHOD(GetValueBool)(UINT index, BOOL* value);
		STDMETHOD(GetValueFloat)(UINT index, DOUBLE* value);
		STDMETHOD(GetValueEvent)(UINT index, RSTRING* value);
		STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value);
		STDMETHOD(SetValueInt)(UINT index, INT value);
		STDMETHOD(SetValueLong)(UINT index, LONGLONG value);
		STDMETHOD(SetValueString)(UINT index, RSTRING value);
		STDMETHOD(SetValueBool)(UINT index, BOOL value);
		STDMETHOD(SetValueFloat)(UINT index, DOUBLE value);
		STDMETHOD(SetValueEvent)(UINT index, RSTRING value);
		STDMETHOD(SetValueJSON)(UINT index, IJSONValue* value);
		STDMETHOD(AddObserver)(IQuadooPropertyObserver* observer);
		STDMETHOD(RemoveObserver)(IQuadooPropertyObserver* observer);
		STDMETHOD(AddDestroyedObserver)(IQuadooComponentDestroyedObserver* observer);
		STDMETHOD(RemoveDestroyedObserver)(IQuadooComponentDestroyedObserver* observer);
		STDMETHOD(BindEvents)(IQuadooConstructionContext* context);
		STDMETHOD(InitializeControl)(VOID);
		STDMETHOD(ActivateControl)(VOID);
		STDMETHOD(AbortConstruction)(VOID);
		STDMETHOD(Measure)(QuadooForm::Size* size);
		STDMETHOD(Arrange)(const QuadooForm::Rect* bounds, UINT dpi);
		STDMETHOD(GetDesignCapabilities)(DWORD* capabilities);
		STDMETHOD(IsUpdatePending)(BOOL* pending);
		STDMETHOD(IsBatchPending)(BOOL* pending) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) { if(pending) *pending = m_updates && m_state == QuadooForm::Ready; else hr = E_POINTER; } return hr; }
		STDMETHOD(FlushPending)(VOID) { return FlushNotifications(); }
		STDMETHOD(GetLayoutDPI)(UINT* dpi) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) { if(dpi) *dpi = m_dpi; else hr = E_POINTER; } return hr; }
		STDMETHOD(RecordCallbackError)(HRESULT error) { HRESULT hr = CheckThread(); if(SUCCEEDED(hr)) m_callbackError = error; return hr; }
		STDMETHOD(GetDefinitionTemplate)(IJSONObject** definition);
	};

	class CWinControl : public CControl, public IQuadooMessageHandler
	{
	protected:
		struct WindowData { CWinControl* owner; BOOL content; WNDPROC original; };
		HWND m_hwnd;
		WindowData m_windowData;
		BOOL m_replacing;
		BOOL m_entered;
		UINT m_focusSequence;
		static CWinControl* FocusRoot(HWND window);
		static HRESULT CollectFocusControls(IQuadooControl* control, TArray<CWinControl*>& controls, UINT depth = 0);
		VOID NotifyFocus(HWND next);
		HRESULT ReplaceNative();
		virtual HRESULT CreateNative();
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT DestroyNative();
		virtual HRESULT ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed);
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT AdjustNativeRect(RECT* rect);
		virtual HRESULT NativeCreated();
		virtual VOID NativeDestroyed(BOOL content);
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
		virtual HRESULT Command(UINT notification);
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
		static HRESULT RegisterWindowClass();

	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooMessageHandler)
			UNK_CHAIN_TO(CControl)
		END_UNK_MAP

	public:
		STDMETHOD(HandleMessage)(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
		CWinControl(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		virtual ~CWinControl();
		STDMETHOD(GetHandle)(HWND* hwnd);
		STDMETHOD(SetFocus)(VOID);
		STDMETHOD(GetTabOrder)(INT* order);
		STDMETHOD(SetTabOrder)(INT order);
	};

	class CChildObserver;
	class CContainer :
		public CWinControl,
		public IQuadooChildWindowHost,
		public IQuadooPropertyObserver,
		public IQuadooContainerMessageTarget
	{
	protected:
		struct Child
		{
			IQuadooControl* control;
			IUnknown* identity; // Borrowed from control, which the entry retains.
			IQuadooPropertyNotifications* notifications;
			CChildObserver* observer;
			QuadooForm::Rect baseline, stored;
			QuadooForm::Size parentSize;
			INT anchors;
			BOOL anchored;
		};
		TArray<Child> m_children;
		TRStrMap<IQuadooControl*> m_names;
		QuadooForm::Size m_contentSize;
		INT m_viewportHeight;
		BOOL m_layoutDirty;
		BOOL m_registering;
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed);
		virtual HRESULT MeasureContent(QuadooForm::Size* size);
		virtual HRESULT DestroyChildren();
		virtual HRESULT BeforeDestroy() { return m_registering ? QuadooForm::EInvalidState : S_OK; }
		virtual HRESULT FinishLayout();
		virtual BOOL PaintChildren(BOOL content) const;
		virtual HRESULT ContentInsets(QuadooForm::Insets* insets);
		virtual VOID PaintBackground(HDC dc, const RECT* bounds);
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
		virtual HRESULT LayoutChildren();
		HRESULT Reindex(IQuadooControl* substitute = NULL, RSTRING name = NULL);

	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooChildWindowHost)
			UNK_INTERFACE(IQuadooPropertyObserver)
			UNK_INTERFACE(IQuadooContainerMessageTarget)
			UNK_CHAIN_TO(CWinControl)
		END_UNK_MAP

	public:
		CContainer(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		virtual ~CContainer();
		virtual IQuadooControlContainer* Container() = 0;
		STDMETHOD(GetChildCount)(UINT* count);
		STDMETHOD(GetChild)(UINT index, IQuadooControl** control);
		STDMETHOD(FindControl)(RSTRING name, IQuadooControl** control);
		STDMETHOD(RegisterControl)(IQuadooControl* control);
		STDMETHOD(UnregisterControl)(IQuadooControl* control);
		STDMETHOD(BeginUpdate)(VOID);
		STDMETHOD(EndUpdate)(VOID);
		STDMETHOD(UpdateLayout)(VOID);
		STDMETHOD(GetChildHostHandle)(HWND* hwnd);
		STDMETHOD(OnPropertyChanged)(IQuadooComponent* component, UINT index);
		STDMETHOD(OnBoundsChanged)(IQuadooControl* control, const QuadooForm::Rect* bounds) { return S_OK; }
	};

	// The adapters forward each public inheritance chain to a shared native base.
	// They do not add capabilities to that base; concrete classes choose their
	// actual primary interface and QueryInterface map.
	template <class TBase, class TInterface> class TControlInterface : public TBase, public TInterface
	{
	public:
		IMP_BASE_UNKNOWN
		TControlInterface(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TBase(factory, context, parent) {}
		virtual IQuadooControl* Control() { return static_cast<TInterface*>(this); }
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IQuadooComponent, TInterface)
			UNK_INTERFACE_PATH(IQuadooControl, TInterface)
			UNK_INTERFACE(TInterface)
			UNK_CHAIN_TO(TBase)
		END_UNK_MAP

	public:
		STDMETHOD(GetID)(GUID* id) { return TBase::GetID(id); }
		STDMETHOD(GetName)(RSTRING* name) { return TBase::GetName(name); }
		STDMETHOD(SetName)(RSTRING name) { return TBase::SetName(name); }
		STDMETHOD(GetTypeName)(RSTRING* name) { return TBase::GetTypeName(name); }
		STDMETHOD(GetProperties)(IQuadooProperties** properties) { return TBase::GetProperties(properties); }
		STDMETHOD(GetState)(QuadooForm::ComponentState* state) { return TBase::GetState(state); }
		STDMETHOD(Destroy)(VOID) { return TBase::Destroy(); }
		STDMETHOD(GetParent)(IQuadooControlContainer** parent) { return TBase::GetParent(parent); }
		STDMETHOD(GetBounds)(QuadooForm::Rect* bounds) { return TBase::GetBounds(bounds); }
		STDMETHOD(SetBounds)(const QuadooForm::Rect* bounds) { return TBase::SetBounds(bounds); }
		STDMETHOD(GetEffectiveBounds)(QuadooForm::Rect* bounds) { return TBase::GetEffectiveBounds(bounds); }
		STDMETHOD(GetVisible)(BOOL* visible) { return TBase::GetVisible(visible); }
		STDMETHOD(SetVisible)(BOOL visible) { return TBase::SetVisible(visible); }
		STDMETHOD(GetEnabled)(BOOL* enabled) { return TBase::GetEnabled(enabled); }
		STDMETHOD(SetEnabled)(BOOL enabled) { return TBase::SetEnabled(enabled); }
	};

	template <class TBase, class TInterface> class TWindowInterface : public TControlInterface<TBase, TInterface>
	{
		typedef TControlInterface<TBase, TInterface> Super;
	public:
		TWindowInterface(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : Super(factory, context, parent) {}
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IQuadooWinControl, TInterface)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP

	public:
		STDMETHOD(GetHandle)(HWND* hwnd) { return TBase::GetHandle(hwnd); }
		STDMETHOD(SetFocus)(VOID) { return TBase::SetFocus(); }
		STDMETHOD(GetTabOrder)(INT* order) { return TBase::GetTabOrder(order); }
		STDMETHOD(SetTabOrder)(INT order) { return TBase::SetTabOrder(order); }
	};

	template <class TBase, class TInterface> class TContainerInterface : public TWindowInterface<TBase, TInterface>
	{
		typedef TWindowInterface<TBase, TInterface> Super;
	public:
		TContainerInterface(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : Super(factory, context, parent) {}
		virtual IQuadooControlContainer* Container() { return static_cast<TInterface*>(this); }
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IQuadooControlContainer, TInterface)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP

	public:
		STDMETHOD(GetChildCount)(UINT* count) { return TBase::GetChildCount(count); }
		STDMETHOD(GetChild)(UINT index, IQuadooControl** control) { return TBase::GetChild(index, control); }
		STDMETHOD(FindControl)(RSTRING name, IQuadooControl** control) { return TBase::FindControl(name, control); }
		STDMETHOD(RegisterControl)(IQuadooControl* control) { return TBase::RegisterControl(control); }
		STDMETHOD(UnregisterControl)(IQuadooControl* control) { return TBase::UnregisterControl(control); }
		STDMETHOD(BeginUpdate)(VOID) { return TBase::BeginUpdate(); }
		STDMETHOD(EndUpdate)(VOID) { return TBase::EndUpdate(); }
		STDMETHOD(UpdateLayout)(VOID) { return TBase::UpdateLayout(); }
	};

	// One viewport/content implementation is shared by Forms and ScrollingPanels.
	class CScrollingContainer :
		public CContainer,
		public IQuadooScrollable
	{
	protected:
		HWND m_content;
		WindowData m_contentData;
		QuadooForm::Point m_scroll;
		HRESULT UpdateScrollbars();
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT NativeCreated();
		virtual HRESULT DestroyNative();
		virtual VOID NativeDestroyed(BOOL content);
		virtual HRESULT FinishLayout();
		virtual BOOL PaintChildren(BOOL content) const;
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooScrollable)
			UNK_CHAIN_TO(CContainer)
		END_UNK_MAP
		CScrollingContainer(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		STDMETHOD(GetScrollPosition)(QuadooForm::Point* position);
		STDMETHOD(SetScrollPosition)(const QuadooForm::Point* position);
		STDMETHOD(GetContentSize)(QuadooForm::Size* size);
		STDMETHOD(SetAutoScroll)(BOOL enabled);
		STDMETHOD(GetAutoScroll)(BOOL* enabled);
		STDMETHOD(GetChildHostHandle)(HWND* hwnd);
	};
	// Form/Frame roots alone own nonvisual components; the store borrows the root.
	template<class TBase, class TInterface> class TRootInterface : public TContainerInterface<TBase,TInterface>, public IQuadooComponentHost
	{
		typedef TContainerInterface<TBase,TInterface> Super;
		IQuadooComponentCollection* m_components;
	protected:
		virtual HRESULT BeforeDestroy()
		{
			HRESULT hr = Super::BeforeDestroy();
			if(SUCCEEDED(hr) && m_components) hr = CanDestroyComponentCollection(m_components);
			return hr;
		}
		virtual HRESULT DestroyChildren()
		{
			HRESULT hr = m_components ? DestroyComponentCollection(m_components,this->m_abort) : S_OK;
			HRESULT children = Super::DestroyChildren();
			return FAILED(hr) ? hr : children;
		}
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooComponentHost)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP
		TRootInterface(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : Super(factory,context,parent), m_components(NULL) {}
		virtual ~TRootInterface() { if(m_components) DestroyComponentCollection(m_components); SafeRelease(m_components); }
		STDMETHOD(GetComponents)(IQuadooComponentCollection** components)
		{
			HRESULT hr;
			CheckIf(!components,E_POINTER); *components = NULL;
			Check(this->CheckAlive());
			if(!m_components) { Check(CreateComponentCollection(this->Container(),&m_components)); }
			*components = m_components; (*components)->AddRef();
		Cleanup:
			return hr;
		}
	};

	class CForm :
		public TRootInterface<CScrollingContainer, IQuadooForm>,
		public IQuadooModalDialog,
		public IQuadooFormKeyboard,
		public IKeyboardState,
		public IQuadooFormMenu
	{
		typedef TRootInterface<CScrollingContainer, IQuadooForm> Super;
		HWND m_owner;
		BOOL m_closing, m_modal;
		HICON m_smallIcon, m_largeIcon;
		HRESULT ApplyWindowStyle();
		HRESULT ApplyIcons();
		CNativeMenu* m_menu;
		BOOL m_menuAdjusting, m_menuPending;
		UINT m_menuLoops;
		BOOL m_keyboardBusy;
		HWND m_suppressedTarget;
		UINT m_suppressedScan;
		HRESULT UpdateDefaultButton();
	protected:
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT AdjustNativeRect(RECT* rect);
		virtual HRESULT NativeCreated();
		virtual HRESULT ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed);
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooModalDialog)
			UNK_INTERFACE(IQuadooFormKeyboard)
			UNK_INTERFACE(IKeyboardState)
			UNK_INTERFACE(IQuadooFormMenu)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP
	protected:
		virtual HRESULT DestroyNative();
	public:
		static const TypeSpec* Type() { return &FormType; }
		static BOOL AcceptParent(IQuadooControlContainer* parent) { return parent == NULL; }
		CForm(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		virtual ~CForm();
		STDMETHOD(GetMainMenu)(IQuadooComponent** menu);
		STDMETHOD(SetMainMenu)(IQuadooComponent* menu);
		STDMETHOD(RefreshMenu)(VOID);
		BOOL TranslateMenu(MSG* message);
		STDMETHOD(ShowForm)(INT command);
		STDMETHOD(HideForm)(VOID);
		STDMETHOD(ShowModal)(LRESULT* result);
		STDMETHOD(Close)(VOID);
		STDMETHOD(CompleteModal)(LRESULT result);
		STDMETHOD(PreprocessMessage)(MSG* message, BOOL* handled);
		STDMETHOD(NoteHandled)(HWND target, UINT scan) { m_suppressedTarget = target; m_suppressedScan = scan; return S_OK; }
	};

	class CFrame : public TRootInterface<CContainer, IQuadooFrame>
	{
	protected:
		virtual HRESULT CreateNative();
	public:
		static const TypeSpec* Type() { return &FrameType; }
		static BOOL AcceptParent(IQuadooControlContainer* parent) { return !parent || CControl::AcceptParent(parent); }
		CFrame(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TRootInterface(factory, context, parent) {}
	};

	class CPanel : public TContainerInterface<CContainer, IQuadooPanel>
	{
	public:
		static const TypeSpec* Type() { return &PanelType; }
		CPanel(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TContainerInterface(factory, context, parent) {}
	};

	class CScrollingPanel : public TContainerInterface<CScrollingContainer, IQuadooScrollingPanel>
	{
		typedef TContainerInterface<CScrollingContainer, IQuadooScrollingPanel> Super;
	public:
		IMP_BASE_UNKNOWN
		static const TypeSpec* Type() { return &ScrollingPanelType; }
		CScrollingPanel(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : Super(factory,context,parent) {}
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IQuadooPanel, IQuadooScrollingPanel)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP

	};

	class CLabel : public TControlInterface<CControl, IQuadooStaticControl>, public IQuadooMessageHandler
	{
		typedef TControlInterface<CControl, IQuadooStaticControl> Super;
	protected:
		virtual HRESULT MeasureContent(QuadooForm::Size* size);
		virtual HRESULT ApplyNative(PCWSTR name);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooMessageHandler)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP

	public:
		STDMETHOD(HandleMessage)(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
		static const TypeSpec* Type() { return &LabelType; }
		CLabel(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TControlInterface(factory, context, parent) {}
		STDMETHOD(Paint)(HDC dc, const RECT* bounds);
		STDMETHOD(HitTest)(INT x, INT y, BOOL* hit);
	};

	class CImage : public TControlInterface<CControl,IQuadooStaticControl>, public IQuadooMessageHandler
	{
		typedef TControlInterface<CControl,IQuadooStaticControl> Super;
		QFI::CImageResource* m_picture;
		HRESULT Diagnostic(UINT index, IJSONValue** value);
	protected:
		virtual HRESULT CreateNative();
		virtual HRESULT DestroyNative();
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT MeasureContent(QuadooForm::Size* size);
		virtual HRESULT PropertyChanged(PCWSTR name);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooMessageHandler)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP
		static const TypeSpec* Type() { return &ImageType; }
		CImage(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : Super(factory,context,parent), m_picture(NULL) {}
		~CImage();
		STDMETHOD(Paint)(HDC dc, const RECT* bounds);
		STDMETHOD(HitTest)(INT x, INT y, BOOL* hit);
		STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
		STDMETHOD(GetValueJSON)(UINT index, IJSONValue** value);
		STDMETHOD(GetValueInt)(UINT index, INT* value);
		STDMETHOD(GetValueBool)(UINT index, BOOL* value);
		STDMETHOD(GetValueString)(UINT index, RSTRING* value);
	};

	class CColorSwatch : public TControlInterface<CControl,IQuadooStaticControl>, public IQuadooMessageHandler
	{
		typedef TControlInterface<CControl,IQuadooStaticControl> Super;
		BOOL m_choosing;
		ULONG m_revision;
		HRESULT Validate(IJSONValue* colors, IJSONValue* labels, INT mode, INT selected);
	protected:
		virtual HRESULT CreateNative();
		virtual HRESULT SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value);
		virtual HRESULT PropertyChanged(PCWSTR name);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooMessageHandler)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP
		static const TypeSpec* Type() { return &ColorSwatchType; }
		CColorSwatch(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : Super(factory,context,parent), m_choosing(FALSE), m_revision(0) {}
		STDMETHOD(Paint)(HDC dc, const RECT* bounds);
		STDMETHOD(HitTest)(INT x, INT y, BOOL* hit);
		STDMETHOD(HandleMessage)(HWND window, UINT message, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
	};

	HRESULT SetDefaultButtonAppearance(IUnknown* button, BOOL selected);
	HRESULT ForwardDrawItem(IQuadooControlContainer* parent, const DRAWITEMSTRUCT* item, WPARAM id, BOOL* handled, LRESULT* result);
	class CButton : public TWindowInterface<CWinControl, IQuadooWinControl>, public IQuadooButton,
		public IQuadooButtonPresentation
	{
		typedef TWindowInterface<CWinControl, IQuadooWinControl> Super;
	protected:
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT Command(UINT notification);
		virtual HRESULT ApplyNative(PCWSTR name);
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooButton)
			UNK_INTERFACE(IQuadooButtonPresentation)
			UNK_CHAIN_TO(Super)
		END_UNK_MAP
		static const TypeSpec* Type() { return &ButtonType; }
		CButton(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TWindowInterface(factory, context, parent) {}
		STDMETHOD(SetDefaultAppearance)(BOOL selected);
		STDMETHOD(Click)(VOID) { HRESULT hr; Check(CheckAlive()); CheckIf(m_state != QuadooForm::Ready,QuadooForm::EInvalidState); Check(Command(BN_CLICKED)); Cleanup: return hr; }
	};

	class CTextEdit : public CWinControl
	{
	protected:
		BOOL m_syncText;
		BOOL m_nativeTextChange;
		HRESULT WriteNativeText(RSTRING text);
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT Command(UINT notification);
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT PropertyChanged(PCWSTR name);
	public:
		CTextEdit(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : CWinControl(factory, context, parent), m_syncText(FALSE), m_nativeTextChange(FALSE) {}
	};
	class CTextBox : public TWindowInterface<CTextEdit, IQuadooWinControl>
	{
	protected:
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT ApplyNative(PCWSTR name);
	public:
		static const TypeSpec* Type() { return &TextBoxType; }
		CTextBox(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TWindowInterface(factory, context, parent) {}
	};
	class CMemo : public TWindowInterface<CTextEdit, IQuadooWinControl>
	{
	protected:
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT NativeCreated();
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		static const TypeSpec* Type() { return &MemoType; }
		CMemo(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TWindowInterface(factory, context, parent) {}
	};

	// Shared native BUTTON behavior for stateful buttons. Concrete controls own
	// their property contracts and selection policy.
	class CCheckedButton : public CWinControl
	{
		BOOL m_syncCheck;
	protected:
		virtual INT NativeCheckState() const = 0;
		virtual HRESULT ChangeCheckState(INT state) = 0;
		virtual HRESULT NativeCreated();
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT Command(UINT notification);
		virtual HRESULT Click() = 0;
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		CCheckedButton(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : CWinControl(factory, context, parent), m_syncCheck(FALSE) {}
	};

	class CCheckBox : public TWindowInterface<CCheckedButton, IQuadooWinControl>
	{
	protected:
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual INT NativeCheckState() const { return Integer(L"CheckState"); }
		virtual HRESULT ChangeCheckState(INT state);
		virtual HRESULT CreateNative();
		virtual HRESULT SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value);
		virtual HRESULT PropertyChanged(PCWSTR name);
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT Click();
	public:
		static const TypeSpec* Type() { return &CheckBoxType; }
		CCheckBox(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TWindowInterface(factory, context, parent) {}
	};

	class CRadioButton : public TWindowInterface<CCheckedButton, IQuadooRadioButton>
	{
		typedef TWindowInterface<CCheckedButton, IQuadooRadioButton> Super;
		HRESULT UpdateTabStops();
		HRESULT Navigate(BOOL forward);
	protected:
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual INT NativeCheckState() const { return Boolean(L"Checked") ? BST_CHECKED : BST_UNCHECKED; }
		virtual HRESULT ChangeCheckState(INT state);
		virtual HRESULT CreateNative();
		virtual HRESULT SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value);
		virtual HRESULT PropertyChanged(PCWSTR name);
		virtual HRESULT ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed);
		virtual HRESULT Click() { return SetChecked(TRUE); }
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		static const TypeSpec* Type() { return &RadioButtonType; }
		CRadioButton(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : Super(factory, context, parent) {}
		STDMETHOD(GetChecked)(BOOL* checked);
		STDMETHOD(SetChecked)(BOOL checked);
	};

	class CGroupBox : public TContainerInterface<CContainer, IQuadooControlContainer>
	{
	protected:
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT ContentInsets(QuadooForm::Insets* insets);
		virtual VOID PaintBackground(HDC dc, const RECT* bounds);
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		static const TypeSpec* Type() { return &GroupBoxType; }
		CGroupBox(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TContainerInterface(factory, context, parent) {}
	};

	// Shared string-item transactions. Native operations are supplied by each
	// concrete control; neither type tests nor specialized public interfaces live here.
	class CItemsControl : public CWinControl
	{
	protected:
		BOOL m_changing, m_syncItems, m_nativeChange;
		virtual UINT ResetMessage() const = 0;
		virtual UINT AddMessage() const = 0;
		virtual UINT SelectMessage() const = 0;
		virtual UINT SelectionMessage() const = 0;
		virtual BOOL HasText() const { return FALSE; }
		virtual BOOL Editable() const { return FALSE; }
		virtual HRESULT CreateNative();
		virtual HRESULT NativeCreated();
		virtual HRESULT SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value);
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
		HRESULT ChangeItems(IJSONValue* items, INT index, RSTRING text, IJSONValue* style = NULL);
		HRESULT SelectionChanged();
		HRESULT TextChanged();
		HRESULT SynchronizeSelection();
	public:
		CItemsControl(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : CWinControl(factory, context, parent), m_changing(FALSE), m_syncItems(FALSE), m_nativeChange(FALSE) {}
		STDMETHOD(GetItems)(IRStringArray** items);
		STDMETHOD(SetItems)(IRStringArray* items);
		STDMETHOD(GetItemIndex)(INT* index);
		STDMETHOD(SetItemIndex)(INT index);
	};

	class CComboBox : public TWindowInterface<CItemsControl, IQuadooComboBox>
	{
		static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data);
	protected:
		virtual UINT ResetMessage() const { return CB_RESETCONTENT; }
		virtual UINT AddMessage() const { return CB_ADDSTRING; }
		virtual UINT SelectMessage() const { return CB_SETCURSEL; }
		virtual UINT SelectionMessage() const { return CB_GETCURSEL; }
		virtual BOOL HasText() const { return TRUE; }
		virtual BOOL Editable() const { return Integer(L"Style") == QuadooForm::DropDown; }
		virtual HRESULT NativeCreated();
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT AdjustNativeRect(RECT* rect);
		virtual HRESULT ArrangeNative(const QuadooForm::Rect* bounds, BOOL changed);
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT Command(UINT notification);
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		static const TypeSpec* Type() { return &ComboBoxType; }
		CComboBox(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TWindowInterface(factory, context, parent) {}
		STDMETHOD(GetDesignCapabilities)(DWORD* capabilities) { HRESULT hr = CControl::GetDesignCapabilities(capabilities); if(SUCCEEDED(hr)) *capabilities &= ~QuadooForm::DesignResizeHeight; return hr; }
		STDMETHOD(GetItems)(IRStringArray** items) { return CItemsControl::GetItems(items); }
		STDMETHOD(SetItems)(IRStringArray* items) { return CItemsControl::SetItems(items); }
		STDMETHOD(GetItemIndex)(INT* index) { return CItemsControl::GetItemIndex(index); }
		STDMETHOD(SetItemIndex)(INT index) { return CItemsControl::SetItemIndex(index); }
	};

	class CListBox : public TWindowInterface<CItemsControl, IQuadooListBox>
	{
	protected:
		virtual UINT ResetMessage() const { return LB_RESETCONTENT; }
		virtual UINT AddMessage() const { return LB_ADDSTRING; }
		virtual UINT SelectMessage() const { return LB_SETCURSEL; }
		virtual UINT SelectionMessage() const { return LB_GETCURSEL; }
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT Command(UINT notification);
	public:
		static const TypeSpec* Type() { return &ListBoxType; }
		CListBox(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TWindowInterface(factory, context, parent) {}
		STDMETHOD(GetItems)(IRStringArray** items) { return CItemsControl::GetItems(items); }
		STDMETHOD(SetItems)(IRStringArray* items) { return CItemsControl::SetItems(items); }
		STDMETHOD(GetItemIndex)(INT* index) { return CItemsControl::GetItemIndex(index); }
		STDMETHOD(SetItemIndex)(INT index) { return CItemsControl::SetItemIndex(index); }
	};

	class CTabControl : public TContainerInterface<CContainer, IQuadooTabControl>
	{
		struct Page { IQuadooTabPage* page; RSTRING caption; BOOL tabVisible; HWND focus; };
		TArray<Page> m_pages;
		IQuadooTabPage* m_active; // Borrowed from m_pages.
		BOOL m_selecting, m_syncTabs, m_structure, m_autoSelect, m_nativeApproved;
		UINT m_pageGeneration;
		INT PageIndex(IQuadooTabPage* page) const;
		INT HeaderIndex(INT page) const;
		INT HeaderPage(INT header) const;
		INT NextPage(INT from, BOOL forward) const;
		HRESULT ReadPage(IQuadooControl* control, Page* page);
		HRESULT InsertHeader(INT header, const Page& page);
		HRESULT IndexValue(INT index, IJSONValue** value);
		HRESULT PageBounds(QuadooForm::Rect* bounds);
		HRESULT CommitIndex(IQuadooTabPage* page, IJSONValue* index);
		HRESULT SelectPage(INT index, BOOL user, BOOL approved = FALSE);
		HRESULT Navigate(BOOL forward, BOOL user);
		VOID SyncSelection();
		HRESULT ShowPages();
		VOID ClearPages();
	protected:
		virtual HRESULT CreateNative();
		virtual VOID NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended);
		virtual HRESULT LayoutChildren();
		virtual HRESULT SetValue(UINT index, QuadooProperty::Type type, IJSONValue* value);
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT DestroyChildren();
		virtual HRESULT BeforeDestroy() { return m_registering || m_structure ? QuadooForm::EInvalidState : S_OK; }
		virtual LRESULT Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled);
	public:
		static const TypeSpec* Type() { return &TabControlType; }
		CTabControl(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent);
		virtual ~CTabControl();
		STDMETHOD(BindEvents)(IQuadooConstructionContext* context);
		STDMETHOD(RegisterControl)(IQuadooControl* control);
		STDMETHOD(UnregisterControl)(IQuadooControl* control);
		STDMETHOD(OnPropertyChanged)(IQuadooComponent* component, UINT index);
		STDMETHOD(HandleMessage)(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL* handled, LRESULT* result);
		STDMETHOD(GetPageCount)(UINT* count);
		STDMETHOD(GetPage)(UINT index, IQuadooTabPage** page);
		STDMETHOD(GetActivePage)(IQuadooTabPage** page);
		STDMETHOD(SetActivePage)(IQuadooTabPage* page);
		STDMETHOD(GetActivePageIndex)(INT* index);
		STDMETHOD(SetActivePageIndex)(INT index);
		STDMETHOD(SelectNextPage)(BOOL forward) { return Navigate(forward,FALSE); }
		STDMETHOD(MovePage)(IQuadooTabPage* page, UINT index);
		STDMETHOD(UpdatePage)(IQuadooTabPage* page);
	};
	class CTabPage : public TContainerInterface<CContainer, IQuadooTabPage>
	{
	protected:
		virtual HRESULT ApplyNative(PCWSTR name);
		virtual HRESULT PropertyChanged(PCWSTR name);
	public:
		static const TypeSpec* Type() { return &TabPageType; }
		static BOOL AcceptParent(IQuadooControlContainer* parent);
		CTabPage(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) : TContainerInterface(factory,context,parent) {}
		STDMETHOD(GetDesignCapabilities)(DWORD* capabilities) { HRESULT hr = CControl::GetDesignCapabilities(capabilities); if(SUCCEEDED(hr)) *capabilities = 0; return hr; }
		STDMETHOD(GetPageIndex)(INT* index);
		STDMETHOD(SetPageIndex)(UINT index);
		STDMETHOD(GetTabVisible)(BOOL* visible);
		STDMETHOD(SetTabVisible)(BOOL visible);
	};

	class CFactory : public CBaseUnknown, public IQuadooControlFactory, public IQuadooFactoryLifetime
		, public IQuadooDesignFactory, public IQuadooPaletteIcon
	{
		DWORD m_thread;
		UINT m_instances;
		IQuadooPropertyDescriptors* m_properties;
		IQuadooEventDescriptors* m_events;
	protected:
		virtual CControl* Allocate(IQuadooConstructionContext* context, IQuadooControlContainer* parent) = 0;
		virtual BOOL AcceptParent(IQuadooControlContainer* parent) = 0;
	public:
		const TypeSpec* m_type;
		TArray<const PropertySpec*> m_specs;
		TArray<const EventSpec*> m_eventSpecs;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooControlFactory)
			UNK_INTERFACE(IQuadooFactoryLifetime)
			UNK_INTERFACE(IQuadooDesignFactory)
			UNK_INTERFACE(IQuadooPaletteIcon)
		END_UNK_MAP

	public:
		CFactory(const TypeSpec* type);
		virtual ~CFactory();
		HRESULT Initialize();
		VOID AddInstance() { m_instances++; }
		VOID RemoveInstance() { m_instances--; }
		STDMETHOD(GetTypeName)(RSTRING* name);
		STDMETHOD(GetProperties)(IQuadooPropertyDescriptors** properties);
		STDMETHOD(GetEvents)(IQuadooEventDescriptors** events);
		STDMETHOD(CreateControl)(IQuadooConstructionContext* context, IQuadooControlContainer* parent, IJSONObject* definition, IQuadooControl** control);
		STDMETHOD(GetLiveCount)(UINT* count);
		STDMETHOD(CanCreate)(IQuadooControlContainer* parent, BOOL* allowed);
		STDMETHOD(GetPaletteIcon)(UINT cx, UINT cy, HICON* icon);
	};
	template<class T> class TFactory : public CFactory
	{
	protected:
		virtual CControl* Allocate(IQuadooConstructionContext* context, IQuadooControlContainer* parent) { return __new T(this, context, parent); }
		virtual BOOL AcceptParent(IQuadooControlContainer* parent) { return T::AcceptParent(parent); }
	public:
		TFactory() : CFactory(T::Type()) {}
	};
}
