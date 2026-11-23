#pragma once
#include "..\Designer\Designer.h"
#include "PackageServices.h"
#include "Library\Util\TextHelpers.h"

namespace QFP
{
const UINT PackagesCommand = 41900;
const UINT RefreshPackagesMessage = WM_APP + 0x139;
const UINT MaxPackageBytes = 1024 * 1024;
const UINT MaxIconBytes = 256 * 1024;
class CDefinition;
class CCatalog;
inline HRESULT CopyString(RSTRING input, RSTRING* output) { if(!output) return E_POINTER; *output=input; RStrAddRef(input); return S_OK; }
struct Property;
struct Event;

// Shared native host contract, not part of QuadooForm.dll's published ABI.
struct IDefinitionVisitor
{
	virtual HRESULT Visit(IJSONObject* definition, RSTRING path) = 0;
	virtual HRESULT Unavailable(RSTRING, HRESULT error) { return error; }
};
struct IHost
{
	virtual HRESULT VisitDefinitions(IDefinitionVisitor* visitor) = 0;
	virtual HRESULT GetEditors(TArray<QFD::CDesignerEditor*>& editors) = 0;
};
struct Choice { CRString name; LONGLONG value; };
struct Property
{
	CRString name, display, category, description;
	QuadooProperty::Type type;
	DWORD flags;
	IJSONValue* initial;
	TArray<Choice*> choices;
	Property() : type(QuadooProperty::String), flags(QuadooForm::PropertySerializable), initial(NULL) {}
	~Property();
};
struct Argument { CRString name; QuadooForm::EventValueType type; BOOL writable; };
struct Event
{
	CRString name, category;
	DWORD flags;
	TArray<Argument*> arguments;
	Event() : flags(0) {}
	~Event();
};
class CDefinition : public CBaseUnknown
{
public:
	GUID id;
	CRString name, type, description;
	BOOL visual, container;
	TArray<Property*> properties;
	TArray<Event*> events;
	TStackRef<IJSONObject> json;
	TStackRef<IJSONValue> icon;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IUnknown)
	END_UNK_MAP
	CDefinition() : visual(FALSE), container(FALSE) { ZeroMemory(&id,sizeof(id)); }
	~CDefinition();
	HRESULT Initialize(IJSONObject* object);
	UINT Find(PCWSTR name) const;
	HRESULT GetProperties(IQuadooPropertyDescriptors** descriptors);
	HRESULT GetEvents(IQuadooEventDescriptors** descriptors);
	HRESULT GetIcon(UINT cx, UINT cy, HICON* result);
};
HRESULT Normalize(const Property* property, IJSONValue* input, IJSONValue** output);
HRESULT ReadString(IJSONObject* object, PCWSTR name, CRString& value, PCWSTR fallback = NULL);
HRESULT ReadInt(IJSONObject* object, PCWSTR name, INT* value, INT fallback = 0);
HRESULT ReadBool(IJSONObject* object, PCWSTR name, BOOL* value, BOOL fallback = FALSE);
HRESULT Fields(IJSONObject* object, PCWSTR names);
HRESULT LoadJSON(PCWSTR path, IJSONObject** object);
HRESULT ParseJSON(PCWSTR text, UINT length, IJSONObject** object);
HRESULT LoadPackage(PCWSTR path, BOOL sidecar, CDefinition** definition);
HRESULT CreateFactory(CDefinition* definition, PCWSTR usageDirectory, IUnknown** factory);
VOID RetireFactory(IUnknown* factory);
HRESULT AcquireUsage(PCWSTR directory, RSTRING type, BOOL removing, HANDLE* handle);
HRESULT AcquirePackageUsage(PCWSTR directory, CDefinition* definition, HANDLE* handle);
// Deterministic transaction-boundary fault injection; -1 disables it.
VOID FailAfter(LONG checkpoints);
HRESULT Checkpoint();

struct Entry
{
	CRString path, name, type, diagnostic;
	CDefinition* definition;
	IUnknown* factory;
	HRESULT status;
	Entry() : definition(NULL), factory(NULL), status(S_OK) {}
	~Entry() { SafeRelease(factory); SafeRelease(definition); }
};
class CCatalog : public CBaseUnknown, public IQuadooControlRegistry,
	public IQuadooComponentRegistry, public IQuadooFactoryCatalog, public IQuadooDesignUsageFactory
{
	DWORD m_thread;
	BOOL m_mutating;
	CRString m_directory, m_usageDirectory;
	HANDLE m_mutex, m_unknown;
	IQuadooControlRegistry* m_registry;
	TArray<Entry*> m_entries;
	TArray<HANDLE> m_leases;
	HRESULT Build(TArray<Entry*>& entries, IQuadooControlRegistry** registry);
	VOID Commit(TArray<Entry*>& entries, IQuadooControlRegistry* registry);
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooControlRegistry)
		UNK_INTERFACE(IQuadooComponentRegistry)
		UNK_INTERFACE(IQuadooFactoryCatalog)
		UNK_INTERFACE(IQuadooDesignUsageFactory)
	END_UNK_MAP
	CCatalog();
	~CCatalog();
	HRESULT Initialize(PCWSTR directory = NULL);
	HRESULT Refresh(IHost* host);
	HRESULT Install(PCWSTR source, IHost* host);
	HRESULT Remove(UINT index, IHost* host);
	HRESULT CanRemove(UINT index, IHost* host, CRString& detail);
	HRESULT UpdateUsage(IHost* host);
	HRESULT ProtectUnknown();
	HRESULT Lock();
	VOID Unlock();
	UINT Count() const { return static_cast<UINT>(m_entries.Length()); }
	const Entry* At(UINT index) const { return index < Count() ? m_entries[index] : NULL; }
	PCWSTR Directory() const { return m_directory.c_wstr(); }
	PCWSTR UsageDirectory() const { return m_usageDirectory.c_wstr(); }
	IQuadooControlRegistry* Registry() { return this; }
	STDMETHOD(RegisterFactory)(IQuadooControlFactory* factory);
	STDMETHOD(UnregisterFactory)(RSTRING type);
	STDMETHOD(FindFactory)(RSTRING type,IQuadooControlFactory** factory);
	STDMETHOD(GetFactoryCount)(UINT* count);
	STDMETHOD(GetFactory)(UINT index,IQuadooControlFactory** factory);
	STDMETHOD(RegisterComponentFactory)(IQuadooComponentFactory* factory);
	STDMETHOD(UnregisterComponentFactory)(RSTRING type);
	STDMETHOD(FindComponentFactory)(RSTRING type,IQuadooComponentFactory** factory);
	STDMETHOD(GetComponentFactoryCount)(UINT* count);
	STDMETHOD(GetComponentFactory)(UINT index,IQuadooComponentFactory** factory);
	STDMETHOD(CreateUsage)(IJSONObject* definition,IQuadooDesignUsage** usage);
};
HRESULT ShowPackages(HWND owner, HINSTANCE instance, CCatalog* catalog, IHost* host);
UINT CatalogChangedMessage();
}
