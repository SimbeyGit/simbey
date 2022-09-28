#pragma once

#define UI_MAKEAPPMODE(x) (1 << (x))

#ifndef	PROPERTYKEY_DEFINED
#define	PROPERTYKEY_DEFINED

typedef struct _tagpropertykey
{
	GUID fmtid;
	DWORD pid;
} PROPERTYKEY;

#endif

#ifndef	_REFPROPVARIANT_DEFINED
#define	_REFPROPVARIANT_DEFINED

#define	REFPROPVARIANT const PROPVARIANT&

#endif

#ifndef TUIPROPERTYKEYDEFINED

	// A version of PROPERTYKEY whose VARTYPE can be tested at compile time.
	#include <pshpack8.h>

	template<VARTYPE T>
	struct TUIPROPERTYKEY
	{
		GUID fmtid;
		DWORD pid;
		inline operator const PROPERTYKEY&() const { return reinterpret_cast<const PROPERTYKEY&>(*this); }
		inline const PROPERTYKEY* operator&() const { return reinterpret_cast<const PROPERTYKEY*>(this); }
	};

	#include <poppack.h>

	C_ASSERT(sizeof(TUIPROPERTYKEY<VT_BOOL>) == sizeof(PROPERTYKEY));

	#define TUIPROPERTYKEYDEFINED

#endif // TUIPROPERTYKEYDEFINED

#define DEFINE_UIPROPERTYKEY(name, type, index) \
	extern const TUIPROPERTYKEY<type> DECLSPEC_SELECTANY name = { { 0x00000000 + index, 0x7363, 0x696e, { 0x84, 0x41, 0x79, 0x8a, 0xcf, 0x5a, 0xeb, 0xb7 } }, type };

#define	REFPROPERTYKEY		const PROPERTYKEY&

typedef enum
{
	UI_VIEWTYPE_RIBBON = 1
} UI_VIEWTYPE;

typedef enum
{
	UI_VIEWVERB_CREATE = 0,
	UI_VIEWVERB_DESTROY = 1,
	UI_VIEWVERB_SIZE = 2,
	UI_VIEWVERB_ERROR = 3
} UI_VIEWVERB;

typedef enum
{
	UI_COMMANDTYPE_UNKNOWN = 0,
	UI_COMMANDTYPE_GROUP = 1,
	UI_COMMANDTYPE_ACTION = 2,
	UI_COMMANDTYPE_ANCHOR = 3,
	UI_COMMANDTYPE_CONTEXT = 4,
	UI_COMMANDTYPE_COLLECTION = 5,
	UI_COMMANDTYPE_COMMANDCOLLECTION = 6,
	UI_COMMANDTYPE_DECIMAL = 7,
	UI_COMMANDTYPE_BOOLEAN = 8,
	UI_COMMANDTYPE_FONT = 9,
	UI_COMMANDTYPE_RECENTITEMS = 10,
	UI_COMMANDTYPE_COLORANCHOR = 11,
	UI_COMMANDTYPE_COLORCOLLECTION = 12
} UI_COMMANDTYPE;

typedef enum
{
	UI_EXECUTIONVERB_EXECUTE = 0,
	UI_EXECUTIONVERB_PREVIEW = 1,
	UI_EXECUTIONVERB_CANCELPREVIEW = 2
} UI_EXECUTIONVERB;

typedef enum
{
	UI_INVALIDATIONS_STATE = 0x1,
	UI_INVALIDATIONS_VALUE = 0x2,
	UI_INVALIDATIONS_PROPERTY = 0x4,
	UI_INVALIDATIONS_ALLPROPERTIES = 0x8
} UI_INVALIDATIONS;

typedef enum
{
	UI_OWNERSHIP_TRANSFER = 0,
	UI_OWNERSHIP_COPY = 1
} UI_OWNERSHIP;

interface __declspec(uuid("23c8c838-4de6-436b-ab01-5554bb7c30dd")) IUIImage : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetBitmap (HBITMAP* bitmap) = 0;
};

interface __declspec(uuid("18aba7f3-4c1c-4ba2-bf6c-f5c3326fa816")) IUIImageFromBitmap : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE CreateImage (HBITMAP bitmap, UI_OWNERSHIP options, IUIImage** image) = 0;
};

interface __declspec(uuid("c205bb48-5b1c-4219-a106-15bd0a5f24e2")) IUISimplePropertySet : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetValue (REFPROPERTYKEY key, PROPVARIANT* value) = 0;
};

interface __declspec(uuid("75ae0a2d-dc03-4c9f-8883-069660d0beb6")) IUICommandHandler : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Execute (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY* key, const PROPVARIANT* currentValue, IUISimplePropertySet* commandExecutionProperties) = 0;
	virtual HRESULT STDMETHODCALLTYPE UpdateProperty (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT* currentValue, PROPVARIANT* newValue) = 0;
};

interface __declspec(uuid("D428903C-729A-491d-910D-682A08FF2522")) IUIApplication : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnViewChanged (UINT32 viewId, UI_VIEWTYPE typeID, IUnknown* view, UI_VIEWVERB verb, INT32 uReasonCode) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnCreateUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler** commandHandler) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnDestroyUICommand (UINT32 commandId, UI_COMMANDTYPE typeID, IUICommandHandler* commandHandler) = 0;
};

interface __declspec(uuid("F4F0385D-6872-43a8-AD09-4C339CB3F5C5")) IUIFramework : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Initialize (HWND frameWnd, IUIApplication* application) = 0;
	virtual HRESULT STDMETHODCALLTYPE Destroy (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE LoadUI (HINSTANCE instance, LPCWSTR resourceName) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetView (UINT32 viewId, REFIID riid, PVOID* ppv) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetUICommandProperty (UINT32 commandId, REFPROPERTYKEY key, PROPVARIANT* value) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetUICommandProperty (UINT32 commandId, REFPROPERTYKEY key, REFPROPVARIANT value) = 0;
	virtual HRESULT STDMETHODCALLTYPE InvalidateUICommand (UINT32 commandId, UI_INVALIDATIONS flags, const PROPERTYKEY* key) = 0;
	virtual HRESULT STDMETHODCALLTYPE FlushPendingInvalidations (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetModes (INT32 iModes) = 0;
};

interface __declspec(uuid("DF4F45BF-6F9D-4dd7-9D68-D8F9CD18C4DB")) IUICollection : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetCount (__out UINT32* pcItems) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetItem (UINT32 index, __deref_out_opt IUnknown** item) = 0;
	virtual HRESULT STDMETHODCALLTYPE Add (IUnknown* item) = 0;
	virtual HRESULT STDMETHODCALLTYPE Insert (UINT32 index, IUnknown* item) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveAt (UINT32 index) = 0;
	virtual HRESULT STDMETHODCALLTYPE Replace (UINT32 indexReplaced, IUnknown* itemReplaceWith) = 0;
	virtual HRESULT STDMETHODCALLTYPE Clear (VOID) = 0;
};

interface __declspec(uuid("803982ab-370a-4f7e-a9e7-8784036a6e26")) IUIRibbon : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetHeight (UINT32* cy) = 0;
	virtual HRESULT STDMETHODCALLTYPE LoadSettingsFromStream (IStream* pStream) = 0;
	virtual HRESULT STDMETHODCALLTYPE SaveSettingsToStream (IStream* pStream) = 0;
};

#define	IsEqualPropertyKey(a, b)	(((a).pid == (b).pid) && IsEqualIID((a).fmtid, (b).fmtid) )

__inline int operator== (REFPROPERTYKEY pkeyOne, REFPROPERTYKEY pkeyOther) { return IsEqualPropertyKey(pkeyOne, pkeyOther); }
__inline int operator!= (REFPROPERTYKEY pkeyOne, REFPROPERTYKEY pkeyOther) { return !(pkeyOne == pkeyOther); }

DEFINE_UIPROPERTYKEY(UI_PKEY_Enabled,                      VT_BOOL,                1); 
DEFINE_UIPROPERTYKEY(UI_PKEY_LabelDescription,             VT_LPWSTR,              2); 
DEFINE_UIPROPERTYKEY(UI_PKEY_Keytip,                       VT_LPWSTR,              3); 
DEFINE_UIPROPERTYKEY(UI_PKEY_Label,                        VT_LPWSTR,              4); 
DEFINE_UIPROPERTYKEY(UI_PKEY_TooltipDescription,           VT_LPWSTR,              5); 
DEFINE_UIPROPERTYKEY(UI_PKEY_TooltipTitle,                 VT_LPWSTR,              6); 
DEFINE_UIPROPERTYKEY(UI_PKEY_LargeImage,                   VT_UNKNOWN,             7); 
DEFINE_UIPROPERTYKEY(UI_PKEY_LargeHighContrastImage,       VT_UNKNOWN,             8); 
DEFINE_UIPROPERTYKEY(UI_PKEY_SmallImage,                   VT_UNKNOWN,             9); 
DEFINE_UIPROPERTYKEY(UI_PKEY_SmallHighContrastImage,       VT_UNKNOWN,             10); 
DEFINE_UIPROPERTYKEY(UI_PKEY_CommandId,                    VT_UI4,                 100); 
DEFINE_UIPROPERTYKEY(UI_PKEY_ItemsSource,                  VT_UNKNOWN,             101); 
DEFINE_UIPROPERTYKEY(UI_PKEY_Categories,                   VT_UNKNOWN,             102); 
DEFINE_UIPROPERTYKEY(UI_PKEY_CategoryId,                   VT_UI4,                 103); 
DEFINE_UIPROPERTYKEY(UI_PKEY_SelectedItem,                 VT_UI4,                 104); 
DEFINE_UIPROPERTYKEY(UI_PKEY_CommandType,                  VT_UI4,                 105); 
DEFINE_UIPROPERTYKEY(UI_PKEY_ItemImage,                    VT_UNKNOWN,             106); 
DEFINE_UIPROPERTYKEY(UI_PKEY_BooleanValue,                 VT_BOOL,                200); 
DEFINE_UIPROPERTYKEY(UI_PKEY_DecimalValue,                 VT_DECIMAL,             201); 
DEFINE_UIPROPERTYKEY(UI_PKEY_StringValue,                  VT_LPWSTR,              202); 
DEFINE_UIPROPERTYKEY(UI_PKEY_MaxValue,                     VT_DECIMAL,             203); 
DEFINE_UIPROPERTYKEY(UI_PKEY_MinValue,                     VT_DECIMAL,             204); 
DEFINE_UIPROPERTYKEY(UI_PKEY_Increment,                    VT_DECIMAL,             205); 
DEFINE_UIPROPERTYKEY(UI_PKEY_DecimalPlaces,                VT_UI4,                 206); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FormatString,                 VT_LPWSTR,              207); 
DEFINE_UIPROPERTYKEY(UI_PKEY_RepresentativeString,         VT_LPWSTR,              208); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties,                     VT_UNKNOWN,             300); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_Family,              VT_LPWSTR,              301); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_Size,                VT_DECIMAL,             302); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_Bold,                VT_UI4,                 303); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_Italic,              VT_UI4,                 304); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_Underline,           VT_UI4,                 305); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_Strikethrough,       VT_UI4,                 306); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_VerticalPositioning, VT_UI4,                 307); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_ForegroundColor,     VT_UI4,                 308); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_BackgroundColor,     VT_UI4,                 309); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_ForegroundColorType, VT_UI4,                 310); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_BackgroundColorType, VT_UI4,                 311); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_ChangedProperties,   VT_UNKNOWN,             312); 
DEFINE_UIPROPERTYKEY(UI_PKEY_FontProperties_DeltaSize,           VT_UI4,                 313); 
DEFINE_UIPROPERTYKEY(UI_PKEY_RecentItems,                  VT_ARRAY|VT_UNKNOWN,    350); 
DEFINE_UIPROPERTYKEY(UI_PKEY_Pinned,                       VT_BOOL,                351); 
DEFINE_UIPROPERTYKEY(UI_PKEY_Color,                        VT_UI4,                 400); 
DEFINE_UIPROPERTYKEY(UI_PKEY_ColorType,                    VT_UI4,                 401); 
DEFINE_UIPROPERTYKEY(UI_PKEY_ColorMode,                    VT_UI4,                 402); 
DEFINE_UIPROPERTYKEY(UI_PKEY_ThemeColorsCategoryLabel,     VT_LPWSTR,              403); 
DEFINE_UIPROPERTYKEY(UI_PKEY_StandardColorsCategoryLabel,  VT_LPWSTR,              404); 
DEFINE_UIPROPERTYKEY(UI_PKEY_RecentColorsCategoryLabel,    VT_LPWSTR,              405); 
DEFINE_UIPROPERTYKEY(UI_PKEY_AutomaticColorLabel,          VT_LPWSTR,              406); 
DEFINE_UIPROPERTYKEY(UI_PKEY_NoColorLabel,                 VT_LPWSTR,              407); 
DEFINE_UIPROPERTYKEY(UI_PKEY_MoreColorsLabel,              VT_LPWSTR,              408); 
DEFINE_UIPROPERTYKEY(UI_PKEY_ThemeColors,                  VT_VECTOR|VT_UI4,       409); 
DEFINE_UIPROPERTYKEY(UI_PKEY_StandardColors,               VT_VECTOR|VT_UI4,       410); 
DEFINE_UIPROPERTYKEY(UI_PKEY_ThemeColorsTooltips,          VT_VECTOR|VT_LPWSTR,    411); 
DEFINE_UIPROPERTYKEY(UI_PKEY_StandardColorsTooltips,       VT_VECTOR|VT_LPWSTR,    412); 
DEFINE_UIPROPERTYKEY(UI_PKEY_Viewable,                     VT_BOOL,                1000); 
DEFINE_UIPROPERTYKEY(UI_PKEY_Minimized,                    VT_BOOL,                1001); 
DEFINE_UIPROPERTYKEY(UI_PKEY_QuickAccessToolbarDock,       VT_UI4,                 1002); 
DEFINE_UIPROPERTYKEY(UI_PKEY_ContextAvailable,             VT_UI4,                 1100); 
DEFINE_UIPROPERTYKEY(UI_PKEY_GlobalBackgroundColor,        VT_UI4,                 2000); 
DEFINE_UIPROPERTYKEY(UI_PKEY_GlobalHighlightColor,         VT_UI4,                 2001); 
DEFINE_UIPROPERTYKEY(UI_PKEY_GlobalTextColor,              VT_UI4,                 2002); 

DEFINE_GUID(CLSID_UIRibbonFramework, 0x926749fa, 0x2615, 0x4987, 0x88, 0x45, 0xc3, 0x3e, 0x65, 0xf2, 0xb9, 0x57);
DEFINE_GUID(CLSID_UIRibbonImageFromBitmapFactory, 0x0f7434b6, 0x59b6, 0x4250, 0x99, 0x9e, 0xd1, 0x68, 0xd6, 0xae, 0x42, 0x93);
