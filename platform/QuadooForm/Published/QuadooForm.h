#pragma once
// QuadooForm native runtime interface, version 1. See NativeRuntime.md for contracts.
// Target: native Win32, COM-style IUnknown interfaces; no COM registration required.
// VC9 / Windows SDK 6.0A compatible. No dependency on the QuadooScript VM.

#include <windows.h>
#include <commctrl.h>
#include <unknwn.h>
#include "Published\SIF.h"
#include "Published\JSON.h"

// Input RSTRINGs are borrowed. Returned RSTRINGs are released with RStrRelease.
// Returned JSON values are independent deep copies. Input definitions are never mutated.
// All out-interface pointers are returned AddRef'd; caller releases them.
// Components, factories, registries, contexts and mutable event values are
// UI-thread-affine. Immutable descriptor collections and IUnknown are thread safe.
// Interface pointers are not automatically marshalled across threads.

interface IQuadooComponent;
interface IQuadooControl;
interface IQuadooWinControl;
interface IQuadooControlContainer;
interface IQuadooProperties;
interface IQuadooPropertyDescriptor;
interface IQuadooPropertyDescriptors;
interface IQuadooPropertyObserver;
interface IQuadooPropertyNotifications;
interface IQuadooComponentDestroyedObserver;
interface IQuadooComponentLifecycle;
interface IQuadooEventArgs;
interface IQuadooEventDescriptor;
interface IQuadooEventDescriptors;
interface IQuadooDelegate;
interface IQuadooEventsBinder;
interface IQuadooControlFactory;
interface IQuadooControlRegistry;
interface IQuadooConstructionContext;
interface IQuadooFrame;
interface IQuadooForm;
interface IQuadooTabControl;
interface IQuadooTabPage;
interface IRStringArray;
interface IQuadooComponentCollection;
interface IQuadooComponentFactory;

namespace QuadooForm
{
	// Shared key for every non-popup IQuadooMenu implementation.
	static const GUID MainMenuUniquenessKey = {0x62a19821,0x1a94,0x4df2,{0xa7,0x40,0x11,0xc4,0x2b,0x30,0x70,0x01}};
	static const GUID StatusBarUniquenessKey = {0x62a19821,0x1a94,0x4df2,{0xa7,0x40,0x11,0xc4,0x2b,0x30,0x70,0x02}};
    enum ComponentState { Constructing = 0, Binding, Initializing, Ready, Destroying, Destroyed };
    enum ContextMode { Runtime = 0, Design, Preview };
    enum AlignMode { AlignNone = 0, AlignTop, AlignBottom, AlignLeft, AlignRight, AlignClient };
    enum ComboBoxStyle { DropDown = 0, DropDownList = 1 };
    enum FormBorderStyle { BorderNone = 0, BorderSingle, BorderSizeable, BorderDialog };
    enum FormBorderIcons { BorderSystemMenu = 1, BorderMinimize = 2, BorderMaximize = 4, BorderClose = 8 };
    enum FormWindowState { WindowNormal = 0, WindowMinimized, WindowMaximized };
    enum FormStyle { FormNormal = 0, FormStayOnTop };
    enum DesignCapability { DesignMoveX = 1, DesignMoveY = 2, DesignResizeWidth = 4, DesignResizeHeight = 8 };
	enum EventValueType
	{
		ValueEmpty = 0,
		ValueInt,
		ValueLong,
		ValueBool,
		ValueString,
		ValueComponent,
		ValueObject
	};
    enum PropertyFlags
    {
        PropertyReadOnly = 0x0001,
        PropertyDesignOnly = 0x0002,
        PropertyRuntimeOnly = 0x0004,
        PropertySerializable = 0x0008,
        PropertyAffectsLayout = 0x0010,
        PropertyHidden = 0x0020
    };
    enum EventFlags { EventCancellable = 0x0001, EventHasResult = 0x0002 };
    // A radio implementation holds this property on its immediate logical
    // parent's HWND while selecting. The value is an opaque non-NULL token.
    // Reject a nested TRUE selection if occupied; FALSE remains permitted.
    // Remove only the property whose token this operation installed.
    static const WCHAR RadioSelectionProperty[] = L"QuadooForm.RadioSelection.v1";
    // Rectangles, sizes, margins, and padding are 96-DPI logical units.
    struct Rect { INT Left, Top, Width, Height; };
    struct Insets { INT Left, Top, Right, Bottom; };
    struct Point { INT X, Y; };
    struct Size { INT Width, Height; };
}

namespace QuadooProperty
{
    // Preserve v1 numeric assignments for the original types.
    enum Type
    {
        Int = 0, Long, String, Boolean, List, Icon, Color, Event,
        Enum, Flags, Float, Rect, Point, Size, Font, ObjectReference, Collection,
        Memo, // Multiline text; JSON string and GetValueString/SetValueString semantics.
        Picture // Null or embedded PNG/ICO {format, encoding:"base64", data} resource.
    };
}

// Identity and lifecycle. IDs are immutable GUIDs serialized into JSON.
// Names may be edited but must be unique within the containing form/frame namespace.
// Destroy is idempotent (S_FALSE if already destroyed). Interface objects can
// outlive their HWNDs; HWND-dependent calls then fail with a documented HRESULT.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0001")) IQuadooComponent : IUnknown
{
    STDMETHOD(GetID)(__out GUID* pguid) = 0;
    STDMETHOD(GetName)(__out RSTRING* prstrNameW) = 0;
    STDMETHOD(SetName)(RSTRING rstrNameW) = 0;
    STDMETHOD(GetTypeName)(__out RSTRING* prstrTypeW) = 0;
    STDMETHOD(GetProperties)(__deref_out IQuadooProperties** ppProperties) = 0;
    STDMETHOD(GetState)(__out QuadooForm::ComponentState* peState) = 0;
    STDMETHOD(Destroy)(VOID) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0002")) IQuadooControl : IQuadooComponent
{
    // Logical parent; immutable, non-owning internally; returned pointer AddRef'd.
    STDMETHOD(GetParent)(__deref_out_opt IQuadooControlContainer** ppParent) = 0;
    STDMETHOD(GetBounds)(__out QuadooForm::Rect* pBounds) = 0;
    STDMETHOD(SetBounds)(__in const QuadooForm::Rect* pBounds) = 0;
    STDMETHOD(GetEffectiveBounds)(__out QuadooForm::Rect* pBounds) = 0;
    STDMETHOD(GetVisible)(__out BOOL* pfVisible) = 0;
    STDMETHOD(SetVisible)(BOOL fVisible) = 0;
    STDMETHOD(GetEnabled)(__out BOOL* pfEnabled) = 0;
    STDMETHOD(SetEnabled)(BOOL fEnabled) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0003")) IQuadooStaticControl : IQuadooControl
{
    // RECT is in physical device pixels for the current paint operation.
    STDMETHOD(Paint)(HDC hdc, __in const RECT* prcPhysicalBounds) = 0;
    STDMETHOD(HitTest)(INT xLogical, INT yLogical, __out BOOL* pfHit) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0004")) IQuadooWinControl : IQuadooControl
{
	// Built-in HWND controls expose OnEnter/OnLeave through event descriptors.
	// These track focus within the control's subtree, including internal HWNDs.
	// Containers enter before descendants and leave after descendants; common
	// ancestors remain entered during sibling transitions. OnFocus/OnBlur retain
	// their direct native focus-message semantics. No new vtable entries required.
    STDMETHOD(GetHandle)(__out HWND* phwnd) = 0;
    STDMETHOD(SetFocus)(VOID) = 0;
    STDMETHOD(GetTabOrder)(__out INT* pnOrder) = 0;
    STDMETHOD(SetTabOrder)(INT nOrder) = 0;
};

// Optional radio-group capability. Immediate logical siblings that expose this
// interface form one group, including controls supplied by external factories.
// Setting TRUE clears checked siblings; setting FALSE may leave the group empty.
// Implementations must permit FALSE during another sibling's selection. Nested
// TRUE selections that change the selected control during notification return
// EInvalidState. A request for the existing state returns S_FALSE. Native built-ins
// raise OnChange for a transition and OnClick only for user activation.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0020")) IQuadooRadioButton : IQuadooWinControl
{
    STDMETHOD(GetChecked)(__out BOOL* pfChecked) = 0;
    STDMETHOD(SetChecked)(BOOL fChecked) = 0;
};

// Text item controls use SimbeyCore's IRStringArray (Published\SimbeyCore.h).
// GetItems returns an independent snapshot; SetItems copies its input. Empty
// strings and duplicates retain their order. ItemIndex is -1 for no selection.
// Replacement retains a valid numeric index, otherwise clears it. OnSelectionChange
// fires once per index transition, including programmatic changes. ComboBox Text
// changes raise OnChange; selecting an item synchronizes Text before callbacks.
// Style is DropDown (editable) or DropDownList (selection only). With no selection,
// an editable combo retains its text and a selection-only combo has empty text.
// Reacquire GetHandle after Items or Style changes: the native window may change.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0021")) IQuadooComboBox : IQuadooWinControl
{
    STDMETHOD(GetItems)(__deref_out IRStringArray** ppItems) = 0;
    STDMETHOD(SetItems)(IRStringArray* pItems) = 0;
    STDMETHOD(GetItemIndex)(__out INT* pnIndex) = 0;
    STDMETHOD(SetItemIndex)(INT nIndex) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0022")) IQuadooListBox : IQuadooWinControl
{
    STDMETHOD(GetItems)(__deref_out IRStringArray** ppItems) = 0;
    STDMETHOD(SetItems)(IRStringArray* pItems) = 0;
    STDMETHOD(GetItemIndex)(__out INT* pnIndex) = 0;
    STDMETHOD(SetItemIndex)(INT nIndex) = 0;
};

// Public registration accepts already-constructed and natively attached controls.
// Registration validates parent identity, namespace uniqueness, and state, then
// AddRefs and inserts the child. No CreateControl method exists on containers.
// UnregisterControl removes the ownership reference; ordinary clients should
// use Destroy() rather than manually unregistering live controls.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0005")) IQuadooControlContainer : IQuadooWinControl
{
    STDMETHOD(GetChildCount)(__out UINT* pcChildren) = 0;
    STDMETHOD(GetChild)(UINT idxChild, __deref_out IQuadooControl** ppControl) = 0;
    // Recursive within this form/frame namespace; does not traverse frame instances.
    STDMETHOD(FindControl)(RSTRING rstrNameW, __deref_out IQuadooControl** ppControl) = 0;
    STDMETHOD(RegisterControl)(IQuadooControl* pControl) = 0;
    STDMETHOD(UnregisterControl)(IQuadooControl* pControl) = 0;
    STDMETHOD(BeginUpdate)(VOID) = 0;
    STDMETHOD(EndUpdate)(VOID) = 0;
    STDMETHOD(UpdateLayout)(VOID) = 0;
};

// A page control owns TabPage containers in Pages/JSON child order. Header indexes
// omit pages whose TabVisible is FALSE; ActivePageIndex always indexes all pages.
// -1 means no active page. Programmatic selection is silent; user navigation
// raises OnChanging(Cancel) before committing and OnChange afterwards.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0023")) IQuadooTabControl : IQuadooControlContainer
{
    STDMETHOD(GetPageCount)(__out UINT* pnCount) = 0;
    STDMETHOD(GetPage)(UINT nIndex, __deref_out IQuadooTabPage** ppPage) = 0;
    STDMETHOD(GetActivePage)(__deref_out_opt IQuadooTabPage** ppPage) = 0;
    STDMETHOD(SetActivePage)(__in_opt IQuadooTabPage* pPage) = 0;
    STDMETHOD(GetActivePageIndex)(__out INT* pnIndex) = 0;
    STDMETHOD(SetActivePageIndex)(INT nIndex) = 0;
    STDMETHOD(SelectNextPage)(BOOL fForward) = 0;
    STDMETHOD(MovePage)(IQuadooTabPage* pPage, UINT nIndex) = 0;
    // Page implementations call this before publishing Caption/TabVisible edits.
    // It validates and updates the native header atomically for that page.
    STDMETHOD(UpdatePage)(IQuadooTabPage* pPage) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0024")) IQuadooTabPage : IQuadooControlContainer
{
    STDMETHOD(GetPageIndex)(__out INT* pnIndex) = 0;
    STDMETHOD(SetPageIndex)(UINT nIndex) = 0;
    STDMETHOD(GetTabVisible)(__out BOOL* pfVisible) = 0;
    STDMETHOD(SetTabVisible)(BOOL fVisible) = 0;
};

// Optional capabilities, obtained by QueryInterface; no multiple-IUnknown diamond.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0006")) IQuadooScrollable : IUnknown
{
    STDMETHOD(GetScrollPosition)(__out QuadooForm::Point* pPosition) = 0;
    STDMETHOD(SetScrollPosition)(__in const QuadooForm::Point* pPosition) = 0;
    STDMETHOD(GetContentSize)(__out QuadooForm::Size* pSize) = 0;
    STDMETHOD(SetAutoScroll)(BOOL fAutoScroll) = 0;
    STDMETHOD(GetAutoScroll)(__out BOOL* pfAutoScroll) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0007")) IQuadooChildWindowHost : IUnknown
{
    // HWND used as the physical parent for native child windows. A scrolling
    // container returns its dedicated content HWND, not its viewport HWND.
    STDMETHOD(GetChildHostHandle)(__out HWND* phwnd) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0008")) IQuadooPanel : IQuadooControlContainer
{
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0009")) IQuadooScrollingPanel : IQuadooPanel
{
    // QueryInterface(IQuadooScrollable) for scrolling functionality.
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A000A")) IQuadooFrame : IQuadooControlContainer
{
    // Frames are always created with a parent and have their own event binder
    // and, when scripted, its own script object. Frames are runtime attachments;
    // definition child arrays never contain Frame or FrameInstance entries.
    // Destroy() destroys the frame subtree, not its parent.
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A000B")) IQuadooForm : IQuadooControlContainer
{
    STDMETHOD(ShowForm)(INT nCmdShow) = 0;
    STDMETHOD(HideForm)(VOID) = 0;
    STDMETHOD(ShowModal)(__out LRESULT* plResult) = 0;
    // Cancellable OnCloseQuery; accepted close destroys HWND and descendants.
    STDMETHOD(Close)(VOID) = 0;
};

// Metadata is shared by instances of a component type.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A000C")) IQuadooPropertyDescriptor : IUnknown
{
    STDMETHOD(GetName)(__out RSTRING* prstrNameW) = 0;
    STDMETHOD(GetDisplayName)(__out RSTRING* prstrNameW) = 0;
    STDMETHOD(GetCategory)(__out RSTRING* prstrCategoryW) = 0;
    STDMETHOD(GetDescription)(__out RSTRING* prstrDescriptionW) = 0;
    STDMETHOD(GetType)(__out QuadooProperty::Type* peType) = 0;
    STDMETHOD(GetFlags)(__out DWORD* pdwFlags) = 0;
    // Values are deep copied so callers cannot mutate shared defaults.
    STDMETHOD(GetDefaultValue)(__deref_out_opt IJSONValue** ppValue) = 0;
    STDMETHOD(GetEnumCount)(__out UINT* pcValues) = 0;
    STDMETHOD(GetEnumValue)(UINT idx, __out RSTRING* prstrNameW, __out LONGLONG* pllValue) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A000D")) IQuadooPropertyDescriptors : IUnknown
{
    STDMETHOD(GetCount)(__out UINT* pcProperties) = 0;
    STDMETHOD(GetAt)(UINT idx, __deref_out IQuadooPropertyDescriptor** ppDescriptor) = 0;
    STDMETHOD(Find)(RSTRING rstrNameW, __deref_out IQuadooPropertyDescriptor** ppDescriptor) = 0;
};

// Per-instance property values; indices follow immutable descriptor order.
// Event properties store callback method names (e.g. MyButton_OnClick).
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A000E")) IQuadooProperties : IUnknown
{
    STDMETHOD(GetDescriptors)(__deref_out IQuadooPropertyDescriptors** ppDescriptors) = 0;
    STDMETHOD(GetValueInt)(UINT idx, __out INT* pnValue) = 0;
    STDMETHOD(GetValueLong)(UINT idx, __out LONGLONG* pllValue) = 0;
    STDMETHOD(GetValueString)(UINT idx, __out RSTRING* prstrValueW) = 0;
    STDMETHOD(GetValueBool)(UINT idx, __out BOOL* pfValue) = 0;
    STDMETHOD(GetValueFloat)(UINT idx, __out DOUBLE* pdValue) = 0;
    STDMETHOD(GetValueEvent)(UINT idx, __out RSTRING* prstrHandlerW) = 0;
    STDMETHOD(GetValueJSON)(UINT idx, __deref_out_opt IJSONValue** ppValue) = 0;
    STDMETHOD(SetValueInt)(UINT idx, INT nValue) = 0;
    STDMETHOD(SetValueLong)(UINT idx, LONGLONG llValue) = 0;
    STDMETHOD(SetValueString)(UINT idx, RSTRING rstrValueW) = 0;
    STDMETHOD(SetValueBool)(UINT idx, BOOL fValue) = 0;
    STDMETHOD(SetValueFloat)(UINT idx, DOUBLE dValue) = 0;
    STDMETHOD(SetValueEvent)(UINT idx, RSTRING rstrHandlerW) = 0;
    STDMETHOD(SetValueJSON)(UINT idx, IJSONValue* pValue) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A000F")) IQuadooPropertyObserver : IUnknown
{
    STDMETHOD(OnPropertyChanged)(IQuadooComponent* pComponent, UINT idxProperty) = 0;
    STDMETHOD(OnBoundsChanged)(IQuadooControl* pControl, __in const QuadooForm::Rect* pBounds) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0010")) IQuadooPropertyNotifications : IUnknown
{
    // AddObserver holds a reference; RemoveObserver releases it.
    STDMETHOD(AddObserver)(IQuadooPropertyObserver* pObserver) = 0;
    STDMETHOD(RemoveObserver)(IQuadooPropertyObserver* pObserver) = 0;
};

// Optional final lifecycle notification, after state Destroyed and all native,
// child and context teardown. The component is borrowed and callback-protected.
// Add owns a reference; duplicates/removal of an absent observer return S_FALSE.
// Add during Destroying/Destroyed returns EDestroyed. Remove remains supported.
// Observers removed before their turn are not called; all are released afterward.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A004A")) IQuadooComponentLifecycle : IUnknown
{
	STDMETHOD(AddDestroyedObserver)(IQuadooComponentDestroyedObserver* observer) = 0;
	STDMETHOD(RemoveDestroyedObserver)(IQuadooComponentDestroyedObserver* observer) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A004B")) IQuadooComponentDestroyedObserver : IUnknown
{
	STDMETHOD_(VOID,OnComponentDestroyed)(IQuadooComponent* component) = 0;
};

// Event argument values are typed and mutable, allowing Cancel/Handled/Result.
// The descriptor defines argument names, types and permitted write access.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0011")) IQuadooEventArgs : IUnknown
{
    STDMETHOD(GetEventName)(__out RSTRING* prstrNameW) = 0;
    STDMETHOD(GetSender)(__deref_out IQuadooComponent** ppSender) = 0;
    STDMETHOD(GetArgumentCount)(__out UINT* pcArgs) = 0;
    STDMETHOD(GetArgumentName)(UINT idx, __out RSTRING* prstrNameW) = 0;
    STDMETHOD(GetArgumentType)(UINT idx, __out QuadooForm::EventValueType* peType) = 0;
    STDMETHOD(GetInt)(UINT idx, __out INT* pnValue) = 0;
    STDMETHOD(SetInt)(UINT idx, INT nValue) = 0;
    STDMETHOD(GetLong)(UINT idx, __out LONGLONG* pllValue) = 0;
    STDMETHOD(SetLong)(UINT idx, LONGLONG llValue) = 0;
    STDMETHOD(GetBool)(UINT idx, __out BOOL* pfValue) = 0;
    STDMETHOD(SetBool)(UINT idx, BOOL fValue) = 0;
    STDMETHOD(GetString)(UINT idx, __out RSTRING* prstrValueW) = 0;
    STDMETHOD(SetString)(UINT idx, RSTRING rstrValueW) = 0;
    STDMETHOD(GetComponent)(UINT idx, __deref_out IQuadooComponent** ppComponent) = 0;
    STDMETHOD(SetComponent)(UINT idx, IQuadooComponent* pComponent) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0012")) IQuadooEventDescriptor : IUnknown
{
    STDMETHOD(GetName)(__out RSTRING* prstrNameW) = 0;
    STDMETHOD(GetCategory)(__out RSTRING* prstrCategoryW) = 0;
    STDMETHOD(GetFlags)(__out DWORD* pdwFlags) = 0;
    STDMETHOD(GetArgumentCount)(__out UINT* pcArgs) = 0;
    STDMETHOD(GetArgument)(UINT idx, __out RSTRING* prstrNameW,
        __out QuadooForm::EventValueType* peType, __out BOOL* pfWritable) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0013")) IQuadooEventDescriptors : IUnknown
{
    STDMETHOD(GetCount)(__out UINT* pcEvents) = 0;
    STDMETHOD(GetAt)(UINT idx, __deref_out IQuadooEventDescriptor** ppDescriptor) = 0;
    STDMETHOD(Find)(RSTRING rstrNameW, __deref_out IQuadooEventDescriptor** ppDescriptor) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0014")) IQuadooDelegate : IUnknown
{
    STDMETHOD(Invoke)(IQuadooEventArgs* pArgs) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0015")) IQuadooEventsBinder : IUnknown
{
    // Resolve handler by NAME ONLY; eager resolution during form/frame creation.
    // S_OK + delegate: bound; S_FALSE + NULL: unbound/optional handler.
    // Nonempty unresolved handlers fail creation; PostBind is called once for
    // the completed root before OnCreate. No script adapter is supplied here.
    STDMETHOD(BindEvent)(RSTRING rstrNameW, __deref_out_opt IQuadooDelegate** ppDelegate) = 0;
    STDMETHOD(PostBind)(IQuadooControl* pControl) = 0;
};

// Optional native raw-message interception; logical script events use delegates.
// Native containers forward control WM_DRAWITEM by DRAWITEMSTRUCT.hwndItem to
// that registered child's handler, passing the child HWND as hwnd. Honor the
// returned handled/result values. ODT_MENU drawing remains the container's own
// responsibility. No private runtime class is required on either side.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0016")) IQuadooMessageHandler : IUnknown
{
    STDMETHOD(HandleMessage)(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
        __out BOOL* pfHandled, __out LRESULT* plResult) = 0;
};

// Optional container capability. The primary GetHandle HWND forwards the
// ContainerCallbackMessage and WM_NCDESTROY to its construction binder's
// IQuadooMessageHandler through the existing window procedure. See
// QuadooContainerMessages.h. No script types or new window subclasses are needed.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A004D")) IQuadooContainerMessageTarget : IUnknown
{
};

// Factories construct controls externally, create/attach HWNDs as needed, then
// call public IQuadooControlContainer::RegisterControl. Failure must roll back HWNDs
// and component references. No reparenting operation exists.
// Descriptor names are unique, nonempty wide strings (case sensitive). Every
// control advertises Name:String, Left/Top/Width/Height:Int, Visible:Boolean,
// and Enabled:Boolean. All seven are writable and PropertySerializable;
// bounds and Visible also have PropertyAffectsLayout. PropertyHidden is allowed.
// HWND controls additionally advertise TabOrder:Int with Serializable/AffectsLayout.
// Framework property names retain their documented types and flags. Defaults,
// descriptor order and metadata are immutable for a registered type and agree
// between factory and instances. Each event has a matching Event property.
// NativeRuntime.md defines the complete reserved-name and extension contract.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0017")) IQuadooControlFactory : IUnknown
{
    STDMETHOD(GetTypeName)(__out RSTRING* prstrTypeW) = 0;
    STDMETHOD(GetProperties)(__deref_out IQuadooPropertyDescriptors** ppDescriptors) = 0;
    STDMETHOD(GetEvents)(__deref_out IQuadooEventDescriptors** ppDescriptors) = 0;
    STDMETHOD(CreateControl)(IQuadooConstructionContext* pContext,
        __in_opt IQuadooControlContainer* pParent, IJSONObject* pDefinition,
        __deref_out IQuadooControl** ppControl) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0018")) IQuadooControlRegistry : IUnknown
{
    STDMETHOD(RegisterFactory)(IQuadooControlFactory* pFactory) = 0;
    STDMETHOD(UnregisterFactory)(RSTRING rstrTypeW) = 0;
    STDMETHOD(FindFactory)(RSTRING rstrTypeW,
        __deref_out IQuadooControlFactory** ppFactory) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0019")) IQuadooConstructionContext : IUnknown
{
    STDMETHOD(GetMode)(__out QuadooForm::ContextMode* peMode) = 0;
    STDMETHOD(GetRegistry)(__deref_out IQuadooControlRegistry** ppRegistry) = 0;
    // 0 selects the parent/window DPI dynamically; nonzero is an explicit scale.
    STDMETHOD(GetDPI)(__out UINT* pnDPI) = 0;
    STDMETHOD(GetBinder)(__deref_out_opt IQuadooEventsBinder** ppBinder) = 0;
    // Runtime/Preview: top-level owner; Design: optional child-window surface host.
    STDMETHOD(GetHostHandle)(__out HWND* phwnd) = 0;
    STDMETHOD(ReportError)(HRESULT hrError, RSTRING rstrPathW, RSTRING rstrDetailW) = 0;
};

// Obtain this service through QuadooGetFormSerializer. Returned definitions
// are independent deep copies; runtime frame attachments are omitted.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A001B")) IQuadooFormSerializer : IUnknown
{
    STDMETHOD(Serialize)(IQuadooComponent* pRoot,
        __deref_out IJSONObject** ppDefinition) = 0;
};


// Factory CreateControl returns a registered Constructing object, without
// application callbacks. QuadooCreateControl constructs children and completes
// this capability for every object. Third-party factories must implement it.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A001C")) IQuadooConstructionParticipant : IUnknown
{
    STDMETHOD(BindEvents)(IQuadooConstructionContext* pContext) = 0;
    STDMETHOD(InitializeControl)(VOID) = 0;
    // Called only after every object in the new subtree has initialized.
    STDMETHOD(ActivateControl)(VOID) = 0;
    STDMETHOD(AbortConstruction)(VOID) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A001D")) IQuadooConstructionReport : IUnknown
{
    STDMETHOD(OnError)(HRESULT hrError, RSTRING rstrPathW, RSTRING rstrDetailW) = 0;
};

// Registries reject unregistering factories without this lifetime capability.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A001E")) IQuadooFactoryLifetime : IUnknown
{
    STDMETHOD(GetLiveCount)(__out UINT* pcInstances) = 0;
};

// Optional native layout capability. All geometry remains in 96-DPI units.
// Custom controls can implement this to participate in dock/anchor layout.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A001F")) IQuadooLayoutControl : IUnknown
{
    STDMETHOD(Measure)(__out QuadooForm::Size* pSize) = 0;
    STDMETHOD(Arrange)(__in const QuadooForm::Rect* pBounds, UINT nDPI) = 0;
};

// Optional designer contracts. Unrelated external implementations may supply
// these without inheriting a native runtime class. Missing capabilities are
// treated conservatively by the editor; construction remains authoritative.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0025")) IQuadooDesignCapabilities : IUnknown
{
    STDMETHOD(GetDesignCapabilities)(__out DWORD* pdwCapabilities) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0026")) IQuadooModalDialog : IUnknown
{
    // A nonzero result closes the form, whether modal or modeless. S_FALSE for
    // zero, a close already in progress, or cancellation by OnCloseQuery.
    // If the form remains open, restore its previous pending result. Application
    // OnClick runs before this operation. ShowModal returns the committed result.
    STDMETHOD(CompleteModal)(LRESULT lResult) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0027")) IQuadooFactoryCatalog : IUnknown
{
    STDMETHOD(GetFactoryCount)(__out UINT* pnCount) = 0;
    STDMETHOD(GetFactory)(UINT nIndex, __deref_out IQuadooControlFactory** ppFactory) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0028")) IQuadooDesignFactory : IUnknown
{
    STDMETHOD(CanCreate)(__in_opt IQuadooControlContainer* pParent, __out BOOL* pfAllowed) = 0;
};

// Additive nonvisual contracts. Collections belong only to definition roots.
// A collection owns its entries; entries borrow their owner until Destroy().
// GetRoot/GetOwner and all returned interfaces are AddRef'd. An external retained
// collection fails with EDestroyed after its root is destroyed. No owner cycle.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0029")) IQuadooComponentHost : IUnknown
{
    STDMETHOD(GetComponents)(IQuadooComponentCollection** ppComponents) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A002A")) IQuadooComponentCollection : IUnknown
{
    STDMETHOD(GetRoot)(IQuadooControlContainer** ppRoot) = 0;
    STDMETHOD(GetCount)(UINT* pcComponents) = 0;
    STDMETHOD(GetAt)(UINT idx, IQuadooComponent** ppComponent) = 0;
    // Searches both visual and nonvisual members in this definition namespace.
    STDMETHOD(FindComponent)(RSTRING rstrNameW, IQuadooComponent** ppComponent) = 0;
    STDMETHOD(ValidateIdentity)(IQuadooComponent* pComponent, RSTRING rstrNameW, REFGUID id) = 0;
    STDMETHOD(RegisterComponent)(IQuadooComponent* pComponent) = 0;
    STDMETHOD(UnregisterComponent)(IQuadooComponent* pComponent) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A002B")) IQuadooNonvisualComponent : IUnknown
{
    STDMETHOD(GetOwner)(IQuadooComponentCollection** ppOwner) = 0;
};
// CreateComponent returns an owner-registered Constructing object. The loader
// completes the existing IQuadooConstructionParticipant protocol. Factories and
// their descriptors follow the same lifetime/thread rules as visual factories.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A002C")) IQuadooComponentFactory : IUnknown
{
    STDMETHOD(GetTypeName)(RSTRING* prstrTypeW) = 0;
    STDMETHOD(GetProperties)(IQuadooPropertyDescriptors** ppProperties) = 0;
    STDMETHOD(GetEvents)(IQuadooEventDescriptors** ppEvents) = 0;
    STDMETHOD(CreateComponent)(IQuadooConstructionContext* pContext, IQuadooComponentCollection* pOwner,
        IJSONObject* pDefinition, IQuadooComponent** ppComponent) = 0;
};
// Query the existing registry for this interface. Type names are unique across
// both registries. Enumeration classifies factories without constructing objects.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A002D")) IQuadooComponentRegistry : IUnknown
{
    STDMETHOD(RegisterComponentFactory)(IQuadooComponentFactory* pFactory) = 0;
    STDMETHOD(UnregisterComponentFactory)(RSTRING rstrTypeW) = 0;
    STDMETHOD(FindComponentFactory)(RSTRING rstrTypeW, IQuadooComponentFactory** ppFactory) = 0;
    STDMETHOD(GetComponentFactoryCount)(UINT* pcFactories) = 0;
    STDMETHOD(GetComponentFactory)(UINT idx, IQuadooComponentFactory** ppFactory) = 0;
};
// Optional lossless persistence for independently implemented components.
// Returns an independent, AddRef'd mutable copy; serialization patches that copy.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A002E")) IQuadooDefinitionSource : IUnknown
{
    STDMETHOD(GetDefinitionTemplate)(IJSONObject** ppDefinition) = 0;
};
namespace QuadooForm
{
    enum PlaceholderReason { MissingFactory = 1, DesignConstructionUnsupported, ParentDesignUnavailable };
}
// Designer representation only; never a runtime replacement or original type's
// capability. GetDefinitionTemplate returns original data plus explicit edits.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A002F")) IQuadooDesignPlaceholder : IUnknown
{
    STDMETHOD(GetReason)(QuadooForm::PlaceholderReason* peReason, RSTRING* prstrDetailW) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0030")) IQuadooFileDialog : IUnknown
{
    // NULL resolves the root's top-level window. S_OK/TRUE = accepted;
    // S_FALSE/FALSE = cancel. Errors and cancellation preserve FileName.
    // Selection never reads, creates or writes the selected file.
    STDMETHOD(Execute)(HWND hwndOwner, BOOL* pfAccepted) = 0;
};
// Native Quadoo color dialog. Color is a QuadooProperty::Color property.
// NULL owner resolves the component root. Accept commits Color; cancel and
// errors preserve it. Design/preview execution is prohibited.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0035")) IQuadooColorDialog : IUnknown
{
    STDMETHOD(Execute)(HWND hwndOwner, BOOL* pfAccepted) = 0;
};
// Factory-level icon provider, shared by visual and nonvisual factories.
// Dimensions are physical pixels (1..256); the normal request is 20x20 at 96 DPI.
// S_OK returns an independent HICON owned by the caller (DestroyIcon).
// S_FALSE returns NULL when the factory has no artwork. Never return LR_SHARED.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0034")) IQuadooPaletteIcon : IUnknown
{
    STDMETHOD(GetPaletteIcon)(UINT cx, UINT cy, HICON* phIcon) = 0;
};
// Call after host accelerators, before TranslateMessage/DispatchMessage. This
// includes IsDialogMessage; do not run another dialog translator on this message.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0032")) IQuadooFormKeyboard : IUnknown
{
    STDMETHOD(PreprocessMessage)(MSG* pMessage, BOOL* pfHandled) = 0;
};
// Optional control capability, queried on the registered wrapper owning the
// target HWND (including its internal child windows), before form shortcuts,
// Default/Cancel, KeyPreview and dialog translation. TRUE bypasses those paths;
// PreprocessMessage still returns pfHandled=FALSE for normal native dispatch.
// This is a UI-thread-affine, read-only query: do not dispatch, consume, modify
// or retain the borrowed message. A missing interface or FALSE preserves normal
// routing. A failed query returns an error rather than dispatching the message.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0048")) IQuadooKeyboardInput : IUnknown
{
	STDMETHOD(OwnsKeyboardMessage)(const MSG* pMessage, BOOL* pfOwns) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0033")) IQuadooButton : IUnknown
{
    // Same logical path as a native button click, including ModalResult.
    STDMETHOD(Click)(VOID) = 0;
};
// Optional visual default-button indication. This does not change the Default
// property or activation policy. Controls without it own their presentation.
// The form may call repeatedly; implementations must be idempotent and must not
// invoke application callbacks. All existing interface vtables are unchanged.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0049")) IQuadooButtonPresentation : IUnknown
{
	STDMETHOD(SetDefaultAppearance)(BOOL fDefault) = 0;
};
// Optional progress capability. These integer properties are also exposed by
// IQuadooProperties and scripts. Defaults: Min=0, Max=100, Position=0, Step=10.
// Min must not exceed Max. Position assignments and range changes clamp the
// position. StepIt wraps past either limit like PBM_STEPIT, including negative
// steps; a range with equal limits stays at that value.
// Optional collection outline. Entries have normal property interfaces, but are
// not registered components and never become generated script class members.
// IDs must be stable and unique within the owning form/frame, including the
// synthetic collection group. The owning component serializes these entries.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0050")) IQuadooComponentOutline : IUnknown
{
	STDMETHOD(GetOutlineParentID)(GUID* id) = 0;
	STDMETHOD(GetOutlineCount)(UINT* count) = 0;
	STDMETHOD(GetOutlineItem)(UINT index, IQuadooComponent** component) = 0;
};

// Reserve a strip at the viewport edge before ordinary docked controls. This
// strip ignores padding, margins, scroll offsets and creation order. Currently
// AlignBottom is supported. Height is in 96-DPI units from GetBounds().
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0051")) IQuadooViewportDock : IUnknown
{
	STDMETHOD(GetDockAlignment)(QuadooForm::AlignMode* alignment) = 0;
};

interface IQuadooStatusPanels;
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0052")) IQuadooStatusBar : IUnknown
{
	STDMETHOD(GetPanels)(IQuadooStatusPanels** panels) = 0;
	STDMETHOD(GetImages)(IQuadooComponent** images) = 0;
	STDMETHOD(SetImages)(IQuadooComponent* images) = 0;
	STDMETHOD(GetSizeGrip)(BOOL* value) = 0;
	STDMETHOD(SetSizeGrip)(BOOL value) = 0;
};

// Icon is an index into StatusBar.Images; -1 means no icon. The status bar owns
// a fresh HICON copied from the DPI-sized ImageList snapshot, never a borrowed
// handle. Width is in 96-DPI units. The last panel extends to the right edge.
// Query IQuadooComponent for identity/properties; panels are collection objects.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0053")) IQuadooStatusPanel : IUnknown
{
	STDMETHOD(GetIndex)(UINT* index) = 0;
	STDMETHOD(GetText)(RSTRING* value) = 0;
	STDMETHOD(SetText)(RSTRING value) = 0;
	STDMETHOD(GetWidth)(INT* value) = 0;
	STDMETHOD(SetWidth)(INT value) = 0;
	STDMETHOD(GetIcon)(INT* value) = 0;
	STDMETHOD(SetIcon)(INT value) = 0;
};

// Retained panel references survive insertion/reordering of siblings. Deleted
// panels retain their ID but other operations return EDestroyed. Max 255 panels
// (normal indexes 0..254; Win32 reserves part index 255 for simple mode).
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0054")) IQuadooStatusPanels : IUnknown
{
	STDMETHOD(GetCount)(UINT* count) = 0;
	STDMETHOD(GetAt)(UINT index, IQuadooStatusPanel** panel) = 0;
	STDMETHOD(Insert)(UINT index, IQuadooStatusPanel** panel) = 0;
	STDMETHOD(Delete)(IQuadooStatusPanel* panel) = 0;
	STDMETHOD(Move)(IQuadooStatusPanel* panel, UINT index) = 0;
	STDMETHOD(Clear)(VOID) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A004C")) IQuadooProgressBar : IUnknown
{
	STDMETHOD(GetPosition)(INT* pnPosition) = 0;
	STDMETHOD(SetPosition)(INT nPosition) = 0;
	STDMETHOD(GetMin)(INT* pnMin) = 0;
	STDMETHOD(SetMin)(INT nMin) = 0;
	STDMETHOD(GetMax)(INT* pnMax) = 0;
	STDMETHOD(SetMax)(INT nMax) = 0;
	STDMETHOD(GetStep)(INT* pnStep) = 0;
	STDMETHOD(SetStep)(INT nStep) = 0;
	STDMETHOD(StepIt)(VOID) = 0;
};

// Local wall-clock fields, without a time-zone conversion. DateTime remains
// available while unchecked. Properties serialize ISO text; MinDate/MaxDate are
// inclusive YYYY-MM-DD bounds. SetDateTime ignores wDayOfWeek and validates all
// other fields (years 1601..9999). OnChange fires once per logical transition.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0057")) IQuadooDateTimePicker : IUnknown
{
	STDMETHOD(GetDateTime)(SYSTEMTIME* value) = 0;
	STDMETHOD(SetDateTime)(const SYSTEMTIME* value) = 0;
	STDMETHOD(GetChecked)(BOOL* value) = 0;
	STDMETHOD(SetChecked)(BOOL value) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0031")) IQuadooTimer : IUnknown
{
    STDMETHOD(GetEnabled)(BOOL* pfEnabled) = 0;
    STDMETHOD(SetEnabled)(BOOL fEnabled) = 0;
    STDMETHOD(GetInterval)(UINT* pnMilliseconds) = 0;
    // 0 disables ticking without changing Enabled. Default = 1000, Enabled=TRUE.
    STDMETHOD(SetInterval)(UINT nMilliseconds) = 0;
};

// Optional per-definition uniqueness. Implement on either visual or nonvisual
// components. Equal non-null keys cannot coexist in the same form/frame namespace;
// nested frame instances have independent scopes. Registration is transactional.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0036")) IQuadooUniqueComponent : IUnknown
{
	STDMETHOD(GetUniquenessKey)(GUID* pKey) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0037")) IQuadooMenuItems : IUnknown
{
	STDMETHOD(GetItemCount)(UINT* pCount) = 0;
	STDMETHOD(GetItem)(UINT index, IQuadooComponent** pItem) = 0;
};
// MenuItem is nonvisual, owned by the definition's ordinary component collection.
// The serialized top-level menuParent GUID is immutable. Order sorts siblings;
// equal Order values retain collection order. Names and IDs share the root scope.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0038")) IQuadooMenuItem : IUnknown
{
	STDMETHOD(GetMenuParentID)(GUID* pParent) = 0;
	STDMETHOD(GetEnabled)(BOOL* pEnabled) = 0;
	STDMETHOD(SetEnabled)(BOOL enabled) = 0;
	STDMETHOD(GetChecked)(BOOL* pChecked) = 0;
	STDMETHOD(SetChecked)(BOOL checked) = 0;
	STDMETHOD(Toggle)(VOID) = 0;
	// Programmatic Click follows native eligibility and AutoCheck rules.
	STDMETHOD(Click)(VOID) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0039")) IQuadooMenu : IUnknown
{
	STDMETHOD(IsPopup)(BOOL* pPopup) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A003A")) IQuadooPopupMenu : IUnknown
{
	// Screen pixel coordinates. NULL owner uses this component's root HWND.
	// S_FALSE means dismissed; design mode never displays application menus.
	STDMETHOD(Popup)(HWND owner, INT x, INT y) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A003B")) IQuadooFormMenu : IUnknown
{
	STDMETHOD(GetMainMenu)(IQuadooComponent** pMenu) = 0;
	STDMETHOD(SetMainMenu)(IQuadooComponent* pMenu) = 0;
	// Menu implementations call this after model edits. Design mode updates the
	// model only; Runtime/Preview replace native handles while retaining client size.
	STDMETHOD(RefreshMenu)(VOID) = 0;
};


// Optional object arguments keep lightweight collection objects out of the
// component namespace. Existing IQuadooEventArgs vtables are unchanged.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A003C")) IQuadooEventObjectArgs : IUnknown
{
	STDMETHOD(GetObject)(UINT index, IUnknown** value) = 0;
	STDMETHOD(SetObject)(UINT index, IUnknown* value) = 0;
};
interface IQuadooImageList;
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A003D")) IQuadooImageListSnapshot : IUnknown
{
	// Borrowed, immutable handle. Never destroy or modify it. Retain this object
	// for as long as a native control uses the handle. Survives owner destruction.
	STDMETHOD(GetHandle)(HIMAGELIST* images) = 0;
	STDMETHOD(GetSize)(UINT* width, UINT* height, UINT* dpi) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A003E")) IQuadooImageListObserver : IUnknown
{
	// UI thread; acquire a replacement after edits. Acquire returns EDestroyed
	// during destruction, at which point consumers must detach their handle.
	// Acquire/unsubscribe are allowed here; recursive property edits are rejected.
	STDMETHOD_(VOID, OnImageListChanged)(IQuadooImageList* images) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A003F")) IQuadooImageList : IUnknown
{
	// Width/Height are logical 96-DPI pixels. DPI must be 48..768. Cached handles
	// are atomically rebuilt on edits. Existing retained snapshots stay valid.
	STDMETHOD(Acquire)(UINT dpi, IQuadooImageListSnapshot** snapshot) = 0;
	STDMETHOD(AddImageListObserver)(IQuadooImageListObserver* observer) = 0;
	STDMETHOD(RemoveImageListObserver)(IQuadooImageListObserver* observer) = 0;
};
interface IQuadooTreeNode;
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0040")) IQuadooTreeNodes : IUnknown
{
	// Count/GetAt address root nodes. Children are accessed through their parent.
	STDMETHOD(GetCount)(UINT* count) = 0;
	STDMETHOD(GetAt)(UINT index, IQuadooTreeNode** node) = 0;
	STDMETHOD(Find)(REFGUID id, IQuadooTreeNode** node) = 0;
	// NULL parent means root; index is the final sibling index (Count appends).
	STDMETHOD(Insert)(IQuadooTreeNode* parent, UINT index, RSTRING text, IQuadooTreeNode** node) = 0;
	STDMETHOD(Delete)(IQuadooTreeNode* node) = 0;
	STDMETHOD(Move)(IQuadooTreeNode* node, IQuadooTreeNode* parent, UINT index) = 0;
	STDMETHOD(Clear)(VOID) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0041")) IQuadooTreeNode : IUnknown
{
	// GUID and Attached remain readable after deletion/destruction. All other
	// methods return EDestroyed for detached nodes. References survive reordering.
	STDMETHOD(GetID)(GUID* id) = 0;
	STDMETHOD(GetAttached)(BOOL* attached) = 0;
	STDMETHOD(GetParent)(IQuadooTreeNode** parent) = 0;
	STDMETHOD(GetIndex)(UINT* index) = 0;
	STDMETHOD(GetChildCount)(UINT* count) = 0;
	STDMETHOD(GetChild)(UINT index, IQuadooTreeNode** node) = 0;
	STDMETHOD(GetText)(RSTRING* text) = 0;
	STDMETHOD(SetText)(RSTRING text) = 0;
	STDMETHOD(GetImageIndex)(INT* index) = 0;
	STDMETHOD(SetImageIndex)(INT index) = 0;
	STDMETHOD(GetSelectedImageIndex)(INT* index) = 0;
	STDMETHOD(SetSelectedImageIndex)(INT index) = 0;
	STDMETHOD(GetExpanded)(BOOL* expanded) = 0;
	STDMETHOD(SetExpanded)(BOOL expanded) = 0;
	STDMETHOD(GetChecked)(BOOL* checked) = 0;
	STDMETHOD(SetChecked)(BOOL checked) = 0;
};
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0042")) IQuadooTreeView : IUnknown
{
	STDMETHOD(GetNodes)(IQuadooTreeNodes** nodes) = 0;
	STDMETHOD(GetSelected)(IQuadooTreeNode** node) = 0;
	// NULL clears selection. S_FALSE means cancelled. Programmatic changes use
	// the same logical events as native interaction; construction remains silent.
	STDMETHOD(SetSelected)(IQuadooTreeNode* node) = 0;
	STDMETHOD(GetImages)(IQuadooComponent** images) = 0;
	STDMETHOD(SetImages)(IQuadooComponent* images) = 0;
	STDMETHOD(BeginLabelEdit)(IQuadooTreeNode* node) = 0;
};

// Canvas coordinates and SIZE/RECT results are physical client pixels.
// The optional drawing interface is independent from the script VM.
interface ISequentialStream;
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0056")) IQuadooCanvas : IUnknown
{
	STDMETHOD(Clear)(COLORREF color) = 0;
	STDMETHOD(SetPixel)(INT x, INT y, COLORREF color) = 0;
	STDMETHOD(GetPixel)(INT x, INT y, COLORREF* color) = 0;
	STDMETHOD(GetPixelSize)(SIZE* size) = 0;
	STDMETHOD(DrawLine)(INT x1, INT y1, INT x2, INT y2, COLORREF color) = 0;
	STDMETHOD(DrawRectangle)(const RECT* bounds, COLORREF outline, COLORREF fill, DWORD flags) = 0;
	STDMETHOD(DrawEllipse)(const RECT* bounds, COLORREF outline, COLORREF fill, DWORD flags) = 0;
	STDMETHOD(DrawTextString)(RSTRING text, INT x, INT y, COLORREF color) = 0;
	STDMETHOD(MeasureText)(RSTRING text, SIZE* size) = 0;
	STDMETHOD(SetFont)(RSTRING name, INT points, BOOL bold, BOOL italic) = 0;
	STDMETHOD(ResetFont)(VOID) = 0;
	STDMETHOD(DrawImageList)(IQuadooImageList* images, UINT index, INT x, INT y) = 0;
	// Independent PNG snapshot, AddRef'd stream at byte zero. NULL/0 on failure.
	STDMETHOD(ExportPng)(ISequentialStream** data, ULONG* count) = 0;
	STDMETHOD(ResolveColor)(COLORREF quadooColor, COLORREF* color) = 0;
	STDMETHOD(Invalidate)(VOID) = 0;
	// Borrowed top-down BGRA32 view. UI thread only; no callbacks/message pumping
	// between acquisition and the final pixel access. Never cache these outputs.
	// GetSurface flushes prior GDI writes. CPU writers keep alpha 255 and call
	// Invalidate after writing. Resize, destruction or mode changes end validity.
	// All three output pointers are required. Supplied outputs are NULL on failure.
	STDMETHOD(GetSurface)(__deref_out HDC* phdcSurface, __deref_out HBITMAP* phDIB, __deref_out SIF_SURFACE** ppsifSurface) = 0;
};

namespace QuadooForm
{
	enum CanvasShapeFlags
	{
		CanvasOutline = 1,
		CanvasFill = 2
	};
	enum CanvasScaleMode
	{
		CanvasPixels = 0
	};
}

namespace QuadooForm
{
    enum AnchorFlags { AnchorLeft = 1, AnchorTop = 2, AnchorRight = 4, AnchorBottom = 8 };
    const HRESULT EDestroyed = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x600);
    const HRESULT EWrongThread = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x601);
    const HRESULT EDuplicateName = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x602);
    const HRESULT EDuplicateID = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x603);
    const HRESULT EInvalidDefinition = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x604);
    const HRESULT EHandlerNotFound = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x605);
    const HRESULT EInvalidState = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x606);
    // Factories may return this only after complete rollback, without callbacks.
    // Design-mode construction alone may then substitute a lossless placeholder.
    const HRESULT EDesignUnsupported = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x607);
    struct ContextOptions
    {
        UINT cbSize;
        ContextMode Mode;
        UINT DPI; // 0 selects current host/system DPI.
        HWND hwndHost;
        IQuadooControlRegistry* pRegistry; // NULL creates a built-in registry.
        IQuadooEventsBinder* pBinder;
        IQuadooConstructionReport* pReport;
    };
}

// Public exports: form may be top-level; frame MUST have a parent container.
// Both accept versioned JSON definitions and eagerly bind event callback names.
// Exported through QuadooForm.def. Inputs are borrowed. The host owns its main
// message loop and process DPI policy. No QuadooScript adapter is included.
#ifdef __cplusplus
extern "C" {
#endif
// Standalone color picker; returns a raw Quadoo Color (including system tags).
// S_OK accepts, S_FALSE cancels; cancellation/errors leave *pSelected untouched.
HRESULT WINAPI ShowQuadooColorPicker(HWND hwndOwner, COLORREF initial, COLORREF* pSelected);
HRESULT WINAPI QuadooCreateForm(__in_opt IQuadooEventsBinder* pBinder,
    __in_opt IQuadooWinControl* pOwner, IJSONObject* pData,
    __deref_out IQuadooForm** ppForm);
HRESULT WINAPI QuadooCreateFrame(__in_opt IQuadooEventsBinder* pBinder,
    __in IQuadooControlContainer* pParent, IJSONObject* pData,
    __deref_out IQuadooFrame** ppFrame);
HRESULT WINAPI QuadooGetControlRegistry(
    __deref_out IQuadooControlRegistry** ppRegistry);
HRESULT WINAPI QuadooCreateConstructionContext(__in const QuadooForm::ContextOptions* pOptions,
    __deref_out IQuadooConstructionContext** ppContext);
HRESULT WINAPI QuadooCreateControl(IQuadooConstructionContext* pContext,
    __in_opt IQuadooControlContainer* pParent, IJSONObject* pDefinition,
    __deref_out IQuadooControl** ppControl);
HRESULT WINAPI QuadooGetFormSerializer(__deref_out IQuadooFormSerializer** ppSerializer);
// ICO values are bounded to 1 MiB/64 DIB images. Complete valid files are
// preserved; missing AND masks in complete 32-bit images are reconstructed
// in memory from alpha (all-zero alpha defaults to opaque). PNG/compressed
// ICO images are rejected for XP compatibility.
// Null clears an icon. Load returns an owned HICON (DestroyIcon), or NULL.
HRESULT WINAPI QuadooCreateIconValue(__in_bcount(cbICO) const BYTE* pbICO, UINT cbICO,
    __deref_out IJSONValue** ppValue);
HRESULT WINAPI QuadooLoadIconValue(IJSONValue* pValue, UINT cx, UINT cy, __out HICON* phIcon);
// Atomic name-validation helper, including frame attachment namespaces.
HRESULT WINAPI QuadooValidateControlName(IQuadooControl* pControl, RSTRING rstrNameW);
HRESULT WINAPI QuadooCreateComponent(IQuadooConstructionContext* pContext,
    IQuadooComponentCollection* pOwner, IJSONObject* pDefinition, IQuadooComponent** ppComponent);
// Colors retain their 32-bit value in properties/JSON (signed INT):
// 0x00BBGGRR is a COLORREF; 0x80000000 | COLOR_* is a GetSysColor index.
// Resolve only when a Win32 operation needs an actual COLORREF.
COLORREF WINAPI ResolveQuadooColor(COLORREF color);
#ifdef __cplusplus
}
#endif

// GetControlRegistry returns a new, UI-thread-affine registry with built-ins.
// Share it through ContextOptions to use registered third-party factories.

// ListView and lightweight collection capabilities.
namespace QuadooForm
{
	enum ListViewStyle
	{
		ListViewIcon = 0,
		ListViewSmallIcon = 1,
		ListViewList = 2,
		ListViewReport = 3
	};
	enum ListColumnAlignment
	{
		ListColumnLeft = 0,
		ListColumnCenter = 1,
		ListColumnRight = 2
	};
	enum ListImageRole
	{
		ListLargeImages = 0,
		ListSmallImages = 1,
		ListStateImages = 2
	};
	enum ListColumnAutoSize
	{
		ListSizeToHeader = 0,
		ListSizeToContent = 1
	};
	enum ListItemChange
	{
		ListChangeCaption = 0x01,
		ListChangeSubItems = 0x02,
		ListChangeImage = 0x04,
		ListChangeState = 0x08,
		ListChangeSelected = 0x10,
		ListChangeFocused = 0x20,
		ListChangeData = 0x40
	};
	// OnChanging OldState/NewState use native LVIS_SELECTED, LVIS_FOCUSED and
	// LVIS_STATEIMAGEMASK bits, preserving unaffected exposed state bits.
	// Checking: OnItemChecking, OnChanging, commit, OnChange, OnItemChecked.
	// Selection: OnChanging, commit, OnChange, OnSelectItem.
	// Construction/recreation are silent; unchanged/canceled writes are silent.
	enum ListDataField
	{
		ListDataCaption = 0x01,
		ListDataSubItems = 0x02,
		ListDataImage = 0x04,
		ListDataState = 0x08,
		ListDataPayload = 0x10
	};
	enum ListFindFlags
	{
		ListFindExact = 0x00,
		ListFindPartial = 0x01,
		ListFindWrap = 0x02,
		ListFindIgnoreCase = 0x04
	};
	const UINT ListViewMaximumVirtualCount = 100000000;
}

interface IQuadooListItem;
interface IQuadooListItems;
interface IQuadooListColumn;
interface IQuadooListColumns;

// All inputs are borrowed, all returned interfaces/RSTRINGs are retained.
// UI-thread affinity and normal HRESULT/out-pointer validation apply throughout.
// S_FALSE is a no-op/cancellation or no matching object; object outputs are NULL.
interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0043")) IQuadooListView : IUnknown
{
	STDMETHOD(GetItems)(IQuadooListItems** items) = 0;
	STDMETHOD(GetColumns)(IQuadooListColumns** columns) = 0;
	// Selected is focused-selected, otherwise the first selected item, or NULL.
	// SetSelected replaces the selection; NULL clears it. Focus is separate.
	STDMETHOD(GetSelected)(IQuadooListItem** item) = 0;
	STDMETHOD(SetSelected)(IQuadooListItem* item) = 0;
	STDMETHOD(GetFocused)(IQuadooListItem** item) = 0;
	STDMETHOD(SetFocused)(IQuadooListItem* item) = 0;
	STDMETHOD(GetSelectedCount)(UINT* count) = 0;
	// Index-based selection needs no caller snapshot or bound data provider.
	// Bound logical state events may request an Item snapshot for their arguments.
	STDMETHOD(SetItemSelected)(UINT itemIndex, BOOL selected) = 0;
	// afterIndex=-1 starts enumeration; S_FALSE/NULL marks the end.
	STDMETHOD(GetNextSelected)(INT afterIndex, IQuadooListItem** item) = 0;
	STDMETHOD(GetImages)(QuadooForm::ListImageRole role, IQuadooComponent** images) = 0;
	STDMETHOD(SetImages)(QuadooForm::ListImageRole role, IQuadooComponent* images) = 0;
	STDMETHOD(BeginLabelEdit)(IQuadooListItem* item) = 0;
	STDMETHOD(EnsureVisible)(IQuadooListItem* item, BOOL partialOK) = 0;
	STDMETHOD(BeginUpdate)(VOID) = 0;
	STDMETHOD(EndUpdate)(VOID) = 0;
	STDMETHOD(Sort)(UINT columnIndex, BOOL ascending) = 0;
	STDMETHOD(Refresh)(VOID) = 0;
	// Inclusive bounds; invalid ranges return E_INVALIDARG, not a partial refresh.
	STDMETHOD(RefreshRange)(UINT firstIndex, UINT lastIndex) = 0;
	STDMETHOD(GetDataGeneration)(ULONGLONG* generation) = 0;
	// OwnerData only: replace count, advance generation, clear selection/focus.
	STDMETHOD(ResetVirtualData)(UINT count) = 0;
	// Submit a proposal through OnDataUpdate; never mutate a frozen snapshot.
	// columnIndex=0 is Caption; positive indexes address SubItems[index-1].
	STDMETHOD(UpdateVirtualText)(UINT itemIndex, UINT columnIndex, RSTRING text) = 0;
	STDMETHOD(UpdateVirtualChecked)(UINT itemIndex, BOOL checked) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0044")) IQuadooListItems : IUnknown
{
	STDMETHOD(GetCount)(UINT* count) = 0;
	// OwnerData only. Changed count advances generation; valid selected indexes
	// are retained. ResetVirtualData explicitly clears selection and focus.
	STDMETHOD(SetCount)(UINT count) = 0;
	STDMETHOD(GetAt)(UINT index, IQuadooListItem** item) = 0;
	// Stored mode only below; OwnerData returns EInvalidState, including Clear.
	STDMETHOD(Find)(REFGUID id, IQuadooListItem** item) = 0;
	STDMETHOD(Insert)(UINT index, RSTRING caption, IQuadooListItem** item) = 0;
	STDMETHOD(Delete)(IQuadooListItem* item) = 0;
	STDMETHOD(Move)(IQuadooListItem* item, UINT finalIndex) = 0;
	STDMETHOD(Clear)(VOID) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0045")) IQuadooListItem : IUnknown
{
	// Stored: stable GUID/COM identity, changing ItemIndex; detached ID/Attached
	// remain readable and other calls return EDestroyed.
	// Virtual: GUID_NULL, fixed index/generation, captured fields remain readable;
	// fields are writable only during an OnData/OnDataUpdate request scope.
	// Virtual Attached means the source generation/index is still current.
	// Frozen virtual Selected/Focused are captured fields. Change live selection
	// through IQuadooListView, which validates current indexes/generation.
	STDMETHOD(GetID)(GUID* id) = 0;
	STDMETHOD(GetAttached)(BOOL* attached) = 0;
	STDMETHOD(GetIsVirtual)(BOOL* isVirtual) = 0;
	// Zero for stored items; positive for virtual request snapshots.
	STDMETHOD(GetDataGeneration)(ULONGLONG* generation) = 0;
	STDMETHOD(GetItemIndex)(UINT* index) = 0;
	STDMETHOD(GetCaption)(RSTRING* caption) = 0;
	STDMETHOD(SetCaption)(RSTRING caption) = 0;
	// Independent snapshot/copy-in, exactly as with existing string collections.
	STDMETHOD(GetSubItems)(IRStringArray** subItems) = 0;
	STDMETHOD(SetSubItems)(IRStringArray* subItems) = 0;
	STDMETHOD(GetSubItem)(UINT index, RSTRING* text) = 0;
	// Missing intervening subitems are filled with empty strings.
	STDMETHOD(SetSubItem)(UINT index, RSTRING text) = 0;
	STDMETHOD(GetImageIndex)(INT* index) = 0;
	STDMETHOD(SetImageIndex)(INT index) = 0;
	// -1=none; 0..14 map to native state images 1..15. Checkbox mode permits
	// only 0=unchecked and 1=checked; Checked aliases this state in that mode.
	STDMETHOD(GetStateIndex)(INT* index) = 0;
	STDMETHOD(SetStateIndex)(INT index) = 0;
	STDMETHOD(GetChecked)(BOOL* checked) = 0;
	STDMETHOD(SetChecked)(BOOL checked) = 0;
	// Runtime-only opaque retained payload. NULL clears; never native lParam.
	STDMETHOD(GetData)(IUnknown** data) = 0;
	STDMETHOD(SetData)(IUnknown* data) = 0;
	STDMETHOD(GetSelected)(BOOL* selected) = 0;
	STDMETHOD(SetSelected)(BOOL selected) = 0;
	STDMETHOD(GetFocused)(BOOL* focused) = 0;
	STDMETHOD(SetFocused)(BOOL focused) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0046")) IQuadooListColumns : IUnknown
{
	STDMETHOD(GetCount)(UINT* count) = 0;
	STDMETHOD(GetAt)(UINT logicalIndex, IQuadooListColumn** column) = 0;
	STDMETHOD(Find)(REFGUID id, IQuadooListColumn** column) = 0;
	STDMETHOD(Insert)(UINT logicalIndex, RSTRING caption, IQuadooListColumn** column) = 0;
	STDMETHOD(Delete)(IQuadooListColumn* column) = 0;
	STDMETHOD(Move)(IQuadooListColumn* column, UINT finalLogicalIndex) = 0;
	STDMETHOD(Clear)(VOID) = 0;
};

interface __declspec(uuid("D25A0D6A-34B4-48EA-AB90-3C8AB72A0047")) IQuadooListColumn : IUnknown
{
	// Stable identity; only ID/Attached remain readable after detachment.
	STDMETHOD(GetID)(GUID* id) = 0;
	STDMETHOD(GetAttached)(BOOL* attached) = 0;
	STDMETHOD(GetIndex)(UINT* logicalIndex) = 0;
	STDMETHOD(GetCaption)(RSTRING* caption) = 0;
	STDMETHOD(SetCaption)(RSTRING caption) = 0;
	// Nonnegative logical pixels at 96 DPI. No Win32 autosize sentinels.
	STDMETHOD(GetWidth)(INT* width) = 0;
	STDMETHOD(SetWidth)(INT width) = 0;
	STDMETHOD(GetAlignment)(QuadooForm::ListColumnAlignment* alignment) = 0;
	STDMETHOD(SetAlignment)(QuadooForm::ListColumnAlignment alignment) = 0;
	STDMETHOD(GetDisplayIndex)(UINT* displayIndex) = 0;
	STDMETHOD(SetDisplayIndex)(UINT displayIndex) = 0;
	// Content mode measures visible rows only when OwnerData is enabled.
	STDMETHOD(AutoSize)(QuadooForm::ListColumnAutoSize mode) = 0;
};
