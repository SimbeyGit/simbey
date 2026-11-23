#include "Packages.h"
#include <shlobj.h>
#include <wincrypt.h>
#pragma comment(lib,"advapi32.lib")
#pragma comment(lib,"shell32.lib")

namespace QFP
{
static volatile LONG s_failure=-1;
VOID FailAfter(LONG count) { InterlockedExchange(&s_failure,count); }
HRESULT Checkpoint()
{
	LONG count=InterlockedCompareExchange(&s_failure,-1,-1);
	if(count<0) return S_OK;
	if(!count) { InterlockedExchange(&s_failure,-1); return E_OUTOFMEMORY; }
	InterlockedDecrement(&s_failure); return S_OK;
}
static HRESULT Hash(RSTRING value,CRString& result)
{
	HRESULT hr=S_OK; HCRYPTPROV provider=0; HCRYPTHASH hash=0; BYTE bytes[20]; DWORD size=sizeof(bytes); WCHAR text[41];
	CheckIf(!CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT),HrEnsureLastError());
	CheckIf(!CryptCreateHash(provider,CALG_SHA1,0,0,&hash),HrEnsureLastError());
	CheckIf(!CryptHashData(hash,reinterpret_cast<const BYTE*>(RStrToWide(value)),RStrLen(value)*sizeof(WCHAR),0),HrEnsureLastError());
	CheckIf(!CryptGetHashParam(hash,HP_HASHVAL,bytes,&size,0),HrEnsureLastError());
	for(UINT n=0;n<20;n++) { text[n*2]=L"0123456789abcdef"[bytes[n]>>4]; text[n*2+1]=L"0123456789abcdef"[bytes[n]&15]; } text[40]=0;
	Check(RStrCreateW(40,text,&result));
Cleanup: if(hash) CryptDestroyHash(hash); if(provider) CryptReleaseContext(provider,0); return hr;
}
static HRESULT OpenMutex(PCWSTR directory,HANDLE* result)
{
	HRESULT hr; CRString hash,name;
	Check(Hash(RSTRING_CAST(directory),hash)); Check(RStrFormatW(&name,L"Local\\QuadooForms.DesignTime.%ls",hash.c_wstr()));
	*result=CreateMutexW(NULL,FALSE,name.c_wstr()); CheckIf(!*result,HrEnsureLastError());
Cleanup: return hr;
}
static HRESULT WaitMutex(HANDLE mutex)
{
	DWORD wait=WaitForSingleObject(mutex,5000);
	return wait==WAIT_OBJECT_0 || wait==WAIT_ABANDONED ? S_OK : wait==WAIT_TIMEOUT ? HRESULT_FROM_WIN32(ERROR_BUSY) : HrEnsureLastError();
}
HRESULT AcquireUsage(PCWSTR directory,RSTRING type,BOOL removing,HANDLE* result)
{
	HRESULT hr; CRString hash,path; HANDLE file=INVALID_HANDLE_VALUE; DWORD written=0; BOOL created=FALSE;
	CheckIf(!result,E_POINTER); *result=NULL; CheckIf(!directory || !RStrIsWide(type),E_INVALIDARG);
	Check(Hash(type,hash)); Check(RStrFormatW(&path,L"%ls\\%ls.use",directory,hash.c_wstr()));
	// Initialize immutable marker content once, then share it read-only. A DELETE
	// access probe conflicts with all active read leases, including other EXEs.
	file=CreateFileW(path.c_wstr(),GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
	if(file!=INVALID_HANDLE_VALUE)
	{
		created=TRUE;
		if(!WriteFile(file,RStrToWide(type),RStrLen(type)*sizeof(WCHAR),&written,NULL) || written!=static_cast<DWORD>(RStrLen(type)*sizeof(WCHAR))) hr=HrEnsureLastError(); else hr=S_OK;
		CloseHandle(file); file=INVALID_HANDLE_VALUE; Check(hr);
	}
	else { DWORD error=GetLastError(); CheckIf(error!=ERROR_FILE_EXISTS && error!=ERROR_ALREADY_EXISTS && error!=ERROR_SHARING_VIOLATION,HRESULT_FROM_WIN32(error)); }
	file=CreateFileW(path.c_wstr(),GENERIC_READ | (removing ? DELETE : 0),FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); CheckIf(file==INVALID_HANDLE_VALUE,HrEnsureLastError());
	{
		WCHAR name[256]; DWORD read=0,size=GetFileSize(file,NULL);
		CheckIf(size!=static_cast<DWORD>(RStrLen(type)*sizeof(WCHAR)) || size>sizeof(name),E_INVALIDARG);
		CheckIf(!ReadFile(file,name,size,&read,NULL) || read!=size,HrEnsureLastError()); CheckIf(memcmp(name,RStrToWide(type),size),E_INVALIDARG);
	}
	*result=file; file=INVALID_HANDLE_VALUE;
Cleanup: if(file!=INVALID_HANDLE_VALUE) CloseHandle(file); UNREFERENCED_PARAMETER(created); return hr;
}
HRESULT AcquirePackageUsage(PCWSTR directory,CDefinition* definition,HANDLE* result)
{
	HRESULT hr; HANDLE mutex=NULL; BOOL locked=FALSE; CRString path; WCHAR guid[40];
	CheckIf(!result,E_POINTER); *result=NULL;
	Check(OpenMutex(directory,&mutex)); Check(WaitMutex(mutex)); locked=TRUE;
	StringFromGUID2(definition->id,guid,ARRAYSIZE(guid)); guid[37]=0;
	Check(RStrFormatW(&path,L"%ls\\..\\%ls.json",directory,guid+1));
	CheckIf(GetFileAttributesW(path.c_wstr())==INVALID_FILE_ATTRIBUTES,HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	Check(AcquireUsage(directory,definition->type,FALSE,result));
Cleanup: if(locked) ReleaseMutex(mutex); if(mutex) CloseHandle(mutex); return hr;
}
UINT CatalogChangedMessage() { static UINT message=RegisterWindowMessageW(L"QuadooForms.DesignTime.CatalogChanged.1"); return message; }
static VOID FreeEntries(TArray<Entry*>& entries) { for(sysint n=0;n<entries.Length();n++) delete entries[n]; entries.Clear(); }
static HRESULT CopyEntries(const TArray<Entry*>& source,TArray<Entry*>& target)
{
	HRESULT hr=S_OK; Entry* entry=NULL;
	for(sysint n=0;n<source.Length();n++)
	{
		entry=__new Entry; CheckAlloc(entry); entry->path=source[n]->path; entry->name=source[n]->name; entry->type=source[n]->type; entry->diagnostic=source[n]->diagnostic; entry->status=source[n]->status;
		SetInterface(entry->definition,source[n]->definition); SetInterface(entry->factory,source[n]->factory);
		Check(target.Append(entry)); entry=NULL;
	}
Cleanup: delete entry; return hr;
}
struct Prepared { QFD::CDesignerEditor* editor; QFD::CDesignerEditor::PaletteUpdate* update; };
class CPrepared
{
	TArray<Prepared> m_updates;
public:
	~CPrepared() { for(sysint n=0;n<m_updates.Length();n++) QFD::CDesignerEditor::DiscardPalette(m_updates[n].update); }
	HRESULT Stage(IHost* host,IQuadooControlRegistry* registry)
	{
		HRESULT hr=S_OK; TArray<QFD::CDesignerEditor*> editors; Prepared entry; entry.update=NULL;
		if(host) { Check(host->GetEditors(editors)); }
		for(sysint n=0;n<editors.Length();n++)
		{
			entry.editor=editors[n]; Check(entry.editor->PreparePalette(registry,&entry.update)); Check(m_updates.Append(entry)); entry.update=NULL;
		}
	Cleanup: QFD::CDesignerEditor::DiscardPalette(entry.update); return hr;
	}
	VOID Commit() { for(sysint n=0;n<m_updates.Length();n++) { m_updates[n].editor->CommitPalette(m_updates[n].update); m_updates[n].update=NULL; } }
};
CCatalog::CCatalog() : m_thread(GetCurrentThreadId()),m_mutating(FALSE),m_mutex(NULL),m_unknown(NULL),m_registry(NULL) {}
CCatalog::~CCatalog()
{
	for(sysint n=0;n<m_leases.Length();n++) CloseHandle(m_leases[n]);
	if(m_unknown) CloseHandle(m_unknown); FreeEntries(m_entries); SafeRelease(m_registry); if(m_mutex) CloseHandle(m_mutex);
}
HRESULT CCatalog::Lock() { return m_thread!=GetCurrentThreadId() ? QuadooForm::EWrongThread : !m_mutex ? QuadooForm::EInvalidState : WaitMutex(m_mutex); }
VOID CCatalog::Unlock() { ReleaseMutex(m_mutex); }
HRESULT CCatalog::Initialize(PCWSTR directory)
{
	HRESULT hr; WCHAR folder[MAX_PATH]; INT error;
	CheckIf(m_registry,QuadooForm::EInvalidState);
	if(directory) { DWORD count=GetFullPathNameW(directory,ARRAYSIZE(folder),folder,NULL); CheckIf(!count || count>=ARRAYSIZE(folder),E_INVALIDARG); Check(RStrCreateW(count,folder,&m_directory)); }
	else { Check(SHGetFolderPathW(NULL,CSIDL_APPDATA,NULL,SHGFP_TYPE_CURRENT,folder)); Check(RStrFormatW(&m_directory,L"%ls\\QuadooForms\\DesignTime",folder)); }
	Check(RStrFormatW(&m_usageDirectory,L"%ls\\Usage",m_directory.c_wstr()));
	error=SHCreateDirectoryExW(NULL,m_usageDirectory.c_wstr(),NULL); CheckIf(error!=ERROR_SUCCESS && error!=ERROR_ALREADY_EXISTS && error!=ERROR_FILE_EXISTS,HRESULT_FROM_WIN32(error));
	Check(OpenMutex(m_usageDirectory.c_wstr(),&m_mutex)); Check(QuadooGetControlRegistry(&m_registry)); Check(Refresh(NULL));
Cleanup: return hr;
}
static BOOL OwnFactory(const TArray<Entry*>& entries,IUnknown* factory)
{
	for(sysint n=0;n<entries.Length();n++) if(entries[n]->factory && QFD::SameObject(entries[n]->factory,factory)) return TRUE;
	return FALSE;
}
static BOOL BuiltinType(IQuadooControlRegistry* registry,RSTRING type)
{
	TStackRef<IQuadooControlFactory> visual; TStackRef<IQuadooComponentRegistry> components; TStackRef<IQuadooComponentFactory> nonvisual;
	if(SUCCEEDED(registry->FindFactory(type,&visual))) return TRUE;
	return SUCCEEDED(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components))) && SUCCEEDED(components->FindComponentFactory(type,&nonvisual));
}
static HRESULT RecoverFiles(PCWSTR directory)
{
	HRESULT hr=S_OK; HANDLE search=INVALID_HANDLE_VALUE; WIN32_FIND_DATAW data;
	static PCWSTR const suffixes[]={L".installing",L".json.removing"};
	for(UINT n=0;n<ARRAYSIZE(suffixes);n++)
	{
		CRString pattern; Check(RStrFormatW(&pattern,L"%ls\\*%ls",directory,suffixes[n]));
		search=FindFirstFileW(pattern.c_wstr(),&data);
		if(search==INVALID_HANDLE_VALUE) { CheckIf(GetLastError()!=ERROR_FILE_NOT_FOUND,HrEnsureLastError()); continue; }
		do
		{
			if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
			if(TStrLenAssert(data.cFileName)!=36+TStrLenAssert(suffixes[n])) continue;
			WCHAR text[40]; GUID id; text[0]=L'{'; CopyMemory(text+1,data.cFileName,36*sizeof(WCHAR)); text[37]=L'}'; text[38]=0;
			if(FAILED(CLSIDFromString(text,&id))) continue;
			CRString path; Check(RStrFormatW(&path,L"%ls\\%ls",directory,data.cFileName));
			if(!DeleteFileW(path.c_wstr())) OutputDebugStringW(L"QuadooForms: package staging cleanup is pending.\n");
		} while(FindNextFileW(search,&data));
		DWORD error=GetLastError(); FindClose(search); search=INVALID_HANDLE_VALUE; CheckIf(error!=ERROR_NO_MORE_FILES,HRESULT_FROM_WIN32(error));
	}
Cleanup: if(search!=INVALID_HANDLE_VALUE) FindClose(search); return hr;
}
HRESULT CCatalog::Build(TArray<Entry*>& entries,IQuadooControlRegistry** result)
{
	HRESULT hr; TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooFactoryCatalog> catalog; TStackRef<IQuadooComponentRegistry> components,oldComponents; UINT count;
	Check(QuadooGetControlRegistry(&registry)); Check(registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&components)));
	// Build an empty staging registry through the published APIs. Reuse every
	// existing native factory, including built-ins: replacing a built-in factory
	// would lose its live-instance accounting and could resurrect removed types.
	{
		TStackRef<IQuadooFactoryCatalog> baseline; Check(registry->QueryInterface(__uuidof(IQuadooFactoryCatalog),reinterpret_cast<PVOID*>(&baseline)));
		Check(baseline->GetFactoryCount(&count));
		while(count) { TStackRef<IQuadooControlFactory> factory; CRString name; Check(baseline->GetFactory(0,&factory)); Check(factory->GetTypeName(&name)); Check(registry->UnregisterFactory(name)); Check(baseline->GetFactoryCount(&count)); }
		Check(components->GetComponentFactoryCount(&count));
		while(count) { TStackRef<IQuadooComponentFactory> factory; CRString name; Check(components->GetComponentFactory(0,&factory)); Check(factory->GetTypeName(&name)); Check(components->UnregisterComponentFactory(name)); Check(components->GetComponentFactoryCount(&count)); }
	}
	Check(m_registry->QueryInterface(__uuidof(IQuadooFactoryCatalog),reinterpret_cast<PVOID*>(&catalog))); Check(catalog->GetFactoryCount(&count));
	for(UINT n=0;n<count;n++)
	{
		TStackRef<IQuadooControlFactory> factory,existing; CRString name;
		Check(catalog->GetFactory(n,&factory)); if(OwnFactory(m_entries,factory)) continue;
		Check(factory->GetTypeName(&name)); if(registry->FindFactory(name,&existing)==HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) { Check(registry->RegisterFactory(factory)); }
	}
	Check(m_registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&oldComponents))); Check(oldComponents->GetComponentFactoryCount(&count));
	for(UINT n=0;n<count;n++)
	{
		TStackRef<IQuadooComponentFactory> factory,existing; CRString name;
		Check(oldComponents->GetComponentFactory(n,&factory)); if(OwnFactory(m_entries,factory)) continue;
		Check(factory->GetTypeName(&name)); if(components->FindComponentFactory(name,&existing)==HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) { Check(components->RegisterComponentFactory(factory)); }
	}
	for(sysint n=0;n<entries.Length();n++)
	{
		Entry* entry=entries[n]; if(!entry->definition) continue; entry->status=S_OK; entry->diagnostic=static_cast<RSTRING>(NULL);
		for(sysint p=0;p<n;p++) if(entries[p]->definition && (IsEqualGUID(entries[p]->definition->id,entry->definition->id) || !lstrcmpiW(entries[p]->type.c_wstr(),entry->type.c_wstr()))) entry->status=HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);
		if(SUCCEEDED(entry->status))
		{
			// Check case-only collisions across both categories, including native types.
			TStackRef<IQuadooFactoryCatalog> all; UINT factories; Check(registry->QueryInterface(__uuidof(IQuadooFactoryCatalog),reinterpret_cast<PVOID*>(&all))); Check(all->GetFactoryCount(&factories));
			for(UINT p=0;p<factories;p++) { TStackRef<IQuadooControlFactory> factory; CRString name; Check(all->GetFactory(p,&factory)); Check(factory->GetTypeName(&name)); if(!lstrcmpiW(name.c_wstr(),entry->type.c_wstr())) entry->status=HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS); }
			Check(components->GetComponentFactoryCount(&factories));
			for(UINT p=0;p<factories;p++) { TStackRef<IQuadooComponentFactory> factory; CRString name; Check(components->GetComponentFactory(p,&factory)); Check(factory->GetTypeName(&name)); if(!lstrcmpiW(name.c_wstr(),entry->type.c_wstr())) entry->status=HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS); }
		}
		if(SUCCEEDED(entry->status) && !entry->factory) { Check(CreateFactory(entry->definition,m_usageDirectory.c_wstr(),&entry->factory)); }
		if(SUCCEEDED(entry->status))
		{
			if(entry->definition->visual) { TStackRef<IQuadooControlFactory> factory; Check(entry->factory->QueryInterface(__uuidof(IQuadooControlFactory),reinterpret_cast<PVOID*>(&factory))); entry->status=registry->RegisterFactory(factory); }
			else { TStackRef<IQuadooComponentFactory> factory; Check(entry->factory->QueryInterface(__uuidof(IQuadooComponentFactory),reinterpret_cast<PVOID*>(&factory))); entry->status=components->RegisterComponentFactory(factory); }
		}
		if(FAILED(entry->status)) { CheckIf(entry->status==E_OUTOFMEMORY,entry->status); Check(RStrFormatW(&entry->diagnostic,L"Package type/metadata conflict (0x%.8x).",entry->status)); }
	}
	*result=registry.Detach(); hr=S_OK;
Cleanup: return hr;
}
VOID CCatalog::Commit(TArray<Entry*>& entries,IQuadooControlRegistry* registry)
{
	for(sysint n=0;n<m_entries.Length();n++) if(m_entries[n]->factory && !OwnFactory(entries,m_entries[n]->factory)) RetireFactory(m_entries[n]->factory);
	m_entries.Swap(entries); ReplaceInterface(m_registry,registry);
}
HRESULT CCatalog::Refresh(IHost* host)
{
	HRESULT hr; BOOL locked=FALSE,entered=FALSE; TArray<Entry*> entries; Entry* entry=NULL; HANDLE search=INVALID_HANDLE_VALUE;
	WIN32_FIND_DATAW data; CRString pattern; TStackRef<IQuadooControlRegistry> registry; CPrepared prepared;
	Check(Lock()); locked=TRUE; CheckIf(m_mutating,HRESULT_FROM_WIN32(ERROR_BUSY)); m_mutating=TRUE; entered=TRUE;
	Check(RecoverFiles(m_directory.c_wstr())); Check(RStrFormatW(&pattern,L"%ls\\*.json",m_directory.c_wstr()));
	search=FindFirstFileW(pattern.c_wstr(),&data);
	if(search==INVALID_HANDLE_VALUE) { CheckIf(GetLastError()!=ERROR_FILE_NOT_FOUND,HrEnsureLastError()); }
	else do
	{
		if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
		entry=__new Entry; CheckAlloc(entry); Check(RStrFormatW(&entry->path,L"%ls\\%ls",m_directory.c_wstr(),data.cFileName));
		entry->status=LoadPackage(entry->path.c_wstr(),FALSE,&entry->definition);
		if(SUCCEEDED(entry->status))
		{
			entry->name=entry->definition->name; entry->type=entry->definition->type;
			WCHAR guid[40]; StringFromGUID2(entry->definition->id,guid,ARRAYSIZE(guid)); guid[37]=0;
			CRString filename; Check(RStrFormatW(&filename,L"%ls.json",guid+1));
			if(lstrcmpiW(data.cFileName,filename.c_wstr())) entry->status=E_INVALIDARG;
			for(sysint n=0;n<m_entries.Length();n++)
			{
				if(!lstrcmpiW(entry->path.c_wstr(),m_entries[n]->path.c_wstr()) && m_entries[n]->definition)
				{
					if(!JSONCompareObjects(entry->definition->json,m_entries[n]->definition->json))
						SetInterface(entry->factory,m_entries[n]->factory);
					else if(m_entries[n]->factory)
					{
						TStackRef<IQuadooFactoryLifetime> lifetime;
						UINT live;
						Check(m_entries[n]->factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime)));
						Check(lifetime->GetLiveCount(&live));
						CheckIf(live,HRESULT_FROM_WIN32(ERROR_BUSY));
					}
				}
			}
		}
		if(FAILED(entry->status))
		{
			entry->name=RSTRING_CAST(data.cFileName);
			Check(RStrFormatW(&entry->diagnostic,L"Invalid package (0x%.8x).",entry->status));
			SafeRelease(entry->definition);
		}
		sysint position=0;
		while(position<entries.Length() && lstrcmpiW(entries[position]->path.c_wstr(),entry->path.c_wstr())<0)
			position++;
		Check(entries.InsertAt(&entry,position));
		entry=NULL;
	} while(FindNextFileW(search,&data));
	if(search!=INVALID_HANDLE_VALUE) { DWORD error=GetLastError(); CheckIf(error!=ERROR_NO_MORE_FILES,HRESULT_FROM_WIN32(error)); FindClose(search); search=INVALID_HANDLE_VALUE; }
	{
		BOOL same=entries.Length()==m_entries.Length();
		for(sysint n=0;same && n<entries.Length();n++) same=!lstrcmpiW(entries[n]->path.c_wstr(),m_entries[n]->path.c_wstr()) && entries[n]->status==m_entries[n]->status && ((!entries[n]->definition && !m_entries[n]->definition) || (entries[n]->definition && m_entries[n]->definition && !JSONCompareObjects(entries[n]->definition->json,m_entries[n]->definition->json)));
		CheckIfIgnore(same,S_FALSE);
	}
	Check(Build(entries,&registry));
	Check(prepared.Stage(host,registry));
	Commit(entries,registry);
	prepared.Commit();

Cleanup:
	if(search!=INVALID_HANDLE_VALUE)
		FindClose(search);
	__delete entry;
	FreeEntries(entries);
	if(entered)
		m_mutating=FALSE;
	if(locked)
		Unlock();
	return hr;
}

class CUsage : public IDefinitionVisitor
{
	HRESULT Walk(IJSONObject* object,RSTRING path,UINT depth)
	{
		HRESULT hr;
		CRString type,name;
		CheckIf(depth>128,E_INVALIDARG);
		Check(ReadString(object,L"type",type));
		BOOL found=FALSE;
		for(sysint n=0;n<types.Length();n++)
		{
			if(type==types[n])
				found=TRUE;
		}
		if(!found)
		{
			RSTRING pointer=type;
			Check(types.Append(pointer));
			RStrAddRef(pointer);
		}
		if(!RStrLen(wanted) || type==static_cast<RSTRING>(wanted))
		{
			matches++;
			Check(ReadString(object,L"name",name,L"(unnamed)"));
			if(!RStrLen(detail))
			{
				Check(RStrFormatW(&detail,L"%ls: %ls (%ls)",RStrToWide(path),name.c_wstr(),type.c_wstr()));
			}
		}
		for(UINT group=0;group<2;group++) if(object->HasField(RSTRING_CAST(group ? L"components" : L"children")))
		{
			TStackRef<IJSONValue> value;
			TStackRef<IJSONArray> array;
			Check(object->FindNonNullValueW(group ? L"components" : L"children",&value));
			Check(value->GetArray(&array));
			for(sysint n=0;n<array->Count();n++)
			{
				TStackRef<IJSONObject> child;
				Check(array->GetObject(n,&child));
				Check(Walk(child,path,depth+1));
			}
		}
	Cleanup:
		return hr;
	}

public:
	TArray<RSTRING> types; CRString wanted,detail; UINT matches;
	CUsage() : matches(0) {}
	~CUsage() { for(sysint n=0;n<types.Length();n++) RStrRelease(types[n]); }
	HRESULT Visit(IJSONObject* object,RSTRING path) { HRESULT hr=Walk(object,path,0); if(FAILED(hr)) { detail=static_cast<RSTRING>(NULL); RStrFormatW(&detail,L"Cannot verify usage in %ls (0x%.8x).",RStrToWide(path),hr); } return hr; }
	HRESULT Unavailable(RSTRING path,HRESULT error) { detail=static_cast<RSTRING>(NULL); RStrFormatW(&detail,L"Cannot verify usage in %ls (0x%.8x).",RStrToWide(path),error); return error; }
};
class CDefinitionUsage : public CBaseUnknown, public IQuadooDesignUsage
{
	TStackRef<CCatalog> m_catalog;
	TArray<HANDLE> m_handles;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooDesignUsage)
	END_UNK_MAP
	CDefinitionUsage(CCatalog* catalog) : m_catalog(catalog) {}
	~CDefinitionUsage() { for(sysint n=0;n<m_handles.Length();n++) CloseHandle(m_handles[n]); }
	STDMETHOD(Update)(IJSONObject* definition)
	{
		HRESULT hr; BOOL locked=FALSE; CUsage usage; TArray<HANDLE> handles; HANDLE handle=NULL;
		Check(m_catalog->Lock()); locked=TRUE; Check(usage.Visit(definition,RSTRING_CAST(L"(designer document)")));
		for(sysint n=0;n<usage.types.Length();n++) { Check(AcquireUsage(m_catalog->UsageDirectory(),usage.types[n],FALSE,&handle)); Check(handles.Append(handle)); handle=NULL; }
		m_handles.Swap(handles);
	Cleanup: if(handle) CloseHandle(handle); for(sysint n=0;n<handles.Length();n++) CloseHandle(handles[n]); if(locked) m_catalog->Unlock(); return hr;
	}
	STDMETHOD(Include)(RSTRING type)
	{
		HRESULT hr; BOOL locked=FALSE; HANDLE handle=NULL;
		Check(m_catalog->Lock()); locked=TRUE; Check(AcquireUsage(m_catalog->UsageDirectory(),type,FALSE,&handle)); Check(m_handles.Append(handle)); handle=NULL;
	Cleanup: if(handle) CloseHandle(handle); if(locked) m_catalog->Unlock(); return hr;
	}
};
static HRESULT ProbeCustomUses(PCWSTR directory)
{
	HRESULT hr; HANDLE search=INVALID_HANDLE_VALUE,file=INVALID_HANDLE_VALUE,probe=NULL;
	WIN32_FIND_DATAW data; CRString pattern; TStackRef<IQuadooControlRegistry> builtins;
	Check(QuadooGetControlRegistry(&builtins)); Check(RStrFormatW(&pattern,L"%ls\\*.use",directory));
	search=FindFirstFileW(pattern.c_wstr(),&data); CheckIf(search==INVALID_HANDLE_VALUE,HrEnsureLastError());
	do
	{
		if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
		CRString path,type; WCHAR text[256]; DWORD read,size;
		Check(RStrFormatW(&path,L"%ls\\%ls",directory,data.cFileName));
		file=CreateFileW(path.c_wstr(),GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL); CheckIf(file==INVALID_HANDLE_VALUE,HrEnsureLastError());
		size=GetFileSize(file,NULL); CheckIf(!size || size>sizeof(text) || size%sizeof(WCHAR),E_INVALIDARG);
		CheckIf(!ReadFile(file,text,size,&read,NULL) || read!=size,HrEnsureLastError()); CloseHandle(file); file=INVALID_HANDLE_VALUE;
		Check(RStrCreateW(size/sizeof(WCHAR),text,&type));
		if(BuiltinType(builtins,type) || type==RSTRING_CAST(L"$unknown")) continue;
		Check(AcquireUsage(directory,type,TRUE,&probe)); CloseHandle(probe); probe=NULL;
	} while(FindNextFileW(search,&data));
	CheckIf(GetLastError()!=ERROR_NO_MORE_FILES,HrEnsureLastError()); hr=S_OK;
Cleanup: if(file!=INVALID_HANDLE_VALUE) CloseHandle(file); if(probe) CloseHandle(probe); if(search!=INVALID_HANDLE_VALUE) FindClose(search); return hr;
}
STDMETHODIMP CCatalog::CreateUsage(IJSONObject* definition,IQuadooDesignUsage** usage)
{
	HRESULT hr; TStackRef<CDefinitionUsage> candidate;
	CheckIf(!usage,E_POINTER); *usage=NULL; CheckIf(!definition,E_INVALIDARG);
	candidate.Attach(__new CDefinitionUsage(this)); CheckAlloc(candidate); Check(candidate->Update(definition)); *usage=candidate.Detach();
Cleanup: return hr;
}
HRESULT CCatalog::ProtectUnknown()
{
	HRESULT hr=S_OK; BOOL locked=FALSE;
	Check(Lock()); locked=TRUE;
	if(!m_unknown) { Check(AcquireUsage(m_usageDirectory.c_wstr(),RSTRING_CAST(L"$unknown"),FALSE,&m_unknown)); }
Cleanup: if(locked) Unlock(); return hr;
}
HRESULT CCatalog::UpdateUsage(IHost* host)
{
	HRESULT hr; BOOL locked=FALSE; CUsage usage; TArray<HANDLE> handles; HANDLE handle=NULL;
	Check(Lock()); locked=TRUE; Check(ProtectUnknown()); if(host) { Check(host->VisitDefinitions(&usage)); }
	for(sysint n=0;n<usage.types.Length();n++) { Check(AcquireUsage(m_usageDirectory.c_wstr(),usage.types[n],FALSE,&handle)); Check(handles.Append(handle)); handle=NULL; }
	for(sysint n=0;n<m_leases.Length();n++) CloseHandle(m_leases[n]); m_leases.Clear(); m_leases.Swap(handles); CloseHandle(m_unknown); m_unknown=NULL;
Cleanup: if(handle) CloseHandle(handle); for(sysint n=0;n<handles.Length();n++) CloseHandle(handles[n]); if(locked) Unlock(); return hr;
}
HRESULT CCatalog::CanRemove(UINT index,IHost* host,CRString& detail)
{
	HRESULT hr; BOOL locked=FALSE; HANDLE probe=NULL; CUsage usage;
	detail=static_cast<RSTRING>(NULL);
	Check(Lock()); locked=TRUE; CheckIf(index>=Count(),E_INVALIDARG);
	if(m_entries[index]->definition) usage.wanted=m_entries[index]->type;
	if(host) { hr=host->VisitDefinitions(&usage); if(FAILED(hr)) { detail=usage.detail; ProtectUnknown(); Check(hr); } }
	if(!m_entries[index]->definition)
	{
		TStackRef<IQuadooControlRegistry> builtins; Check(QuadooGetControlRegistry(&builtins)); usage.matches=0;
		for(sysint n=0;n<usage.types.Length();n++) if(!BuiltinType(builtins,usage.types[n])) usage.matches++;
		usage.detail=RSTRING_CAST(L"Close documents using custom or unknown types before removing a package whose type cannot be read.");
	}
	Check(UpdateUsage(host));
	if(usage.matches) { Check(RStrFormatW(&detail,L"In use (%u match%ls). %ls",usage.matches,usage.matches==1 ? L"" : L"es",usage.detail.c_wstr())); hr=S_FALSE; goto Cleanup; }
	if(m_entries[index]->factory) { TStackRef<IQuadooFactoryLifetime> lifetime; UINT live; Check(m_entries[index]->factory->QueryInterface(__uuidof(IQuadooFactoryLifetime),reinterpret_cast<PVOID*>(&lifetime))); Check(lifetime->GetLiveCount(&live)); if(live) { Check(RStrFormatW(&detail,L"%u retained control instance(s) still use this package.",live)); hr=S_FALSE; goto Cleanup; } }
	hr=AcquireUsage(m_usageDirectory.c_wstr(),RSTRING_CAST(L"$unknown"),TRUE,&probe);
	if(FAILED(hr)) { detail=RSTRING_CAST(L"Another designer is opening or cannot verify a project. Close or repair that project before removing packages."); hr=S_FALSE; goto Cleanup; }
	CloseHandle(probe); probe=NULL;
	if(m_entries[index]->definition)
	{
		hr=AcquireUsage(m_usageDirectory.c_wstr(),m_entries[index]->type,TRUE,&probe);
		if(FAILED(hr)) { detail=RSTRING_CAST(L"This control type is in use by another running designer or Quadoo Studio."); hr=S_FALSE; goto Cleanup; }
	}
	else
	{
		hr=ProbeCustomUses(m_usageDirectory.c_wstr());
		if(FAILED(hr)) { detail=RSTRING_CAST(L"Another designer uses custom or unknown controls. Close those documents before removing an unreadable package."); hr=S_FALSE; goto Cleanup; }
	}
	detail=RSTRING_CAST(L"This package can be removed."); hr=S_OK;
Cleanup: if(probe) CloseHandle(probe); if(locked) Unlock(); return hr;
}
static HRESULT SaveJSON(PCWSTR path,IJSONObject* object)
{
	HRESULT hr; CMemoryStream text,utf8; HANDLE file=INVALID_HANDLE_VALUE; BYTE* bytes; INT count; DWORD written;
	Check(JSONSerializeObject(object,&text)); count=WideCharToMultiByte(CP_UTF8,0,text.TGetReadPtr<WCHAR>(),text.DataRemaining()/sizeof(WCHAR),NULL,0,NULL,NULL); CheckIf(!count || count>MaxPackageBytes,E_INVALIDARG);
	Check(utf8.TWriteAdvance<BYTE>(&bytes,count)); CheckIf(!WideCharToMultiByte(CP_UTF8,0,text.TGetReadPtr<WCHAR>(),text.DataRemaining()/sizeof(WCHAR),reinterpret_cast<PSTR>(bytes),count,NULL,NULL),HrEnsureLastError());
	file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL); CheckIf(file==INVALID_HANDLE_VALUE,HrEnsureLastError());
	Check(Checkpoint()); CheckIf(!WriteFile(file,bytes,count,&written,NULL) || written!=static_cast<DWORD>(count),HrEnsureLastError()); Check(Checkpoint()); CheckIf(!FlushFileBuffers(file),HrEnsureLastError());
Cleanup: if(file!=INVALID_HANDLE_VALUE) CloseHandle(file); return hr;
}
HRESULT CCatalog::Install(PCWSTR source,IHost* host)
{
	HRESULT hr; BOOL locked=FALSE,published=FALSE,entered=FALSE; TArray<Entry*> entries; Entry* entry=NULL; CRString temporary,final;
	TStackRef<CDefinition> definition; TStackRef<IQuadooControlRegistry> registry; CPrepared prepared;
	Check(Lock()); locked=TRUE; CheckIf(m_mutating,HRESULT_FROM_WIN32(ERROR_BUSY)); m_mutating=TRUE; entered=TRUE;
	Check(LoadPackage(source,TRUE,&definition)); Check(Checkpoint()); Check(CopyEntries(m_entries,entries));
	for(sysint n=0;n<entries.Length();n++) { CheckIf(entries[n]->definition && (IsEqualGUID(entries[n]->definition->id,definition->id) || !lstrcmpiW(entries[n]->type.c_wstr(),definition->type.c_wstr())),HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)); }
	WCHAR guid[40]; StringFromGUID2(definition->id,guid,ARRAYSIZE(guid)); guid[37]=0; Check(RStrFormatW(&final,L"%ls\\%ls.json",m_directory.c_wstr(),guid+1)); Check(RStrFormatW(&temporary,L"%ls\\%ls.installing",m_directory.c_wstr(),guid+1));
	CheckIf(GetFileAttributesW(final.c_wstr())!=INVALID_FILE_ATTRIBUTES,HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
	entry=__new Entry; CheckAlloc(entry); entry->path=final; entry->name=definition->name; entry->type=definition->type; SetInterface(entry->definition,static_cast<CDefinition*>(definition));
	Check(entries.Append(entry)); entry=NULL; Check(Build(entries,&registry)); Check(entries[entries.Length()-1]->status); Check(Checkpoint()); Check(prepared.Stage(host,registry)); Check(Checkpoint());
	Check(SaveJSON(temporary.c_wstr(),definition->json)); Check(Checkpoint()); CheckIf(!MoveFileExW(temporary.c_wstr(),final.c_wstr(),MOVEFILE_WRITE_THROUGH),HrEnsureLastError()); published=TRUE;
	Commit(entries,registry); prepared.Commit(); PostMessageW(HWND_BROADCAST,CatalogChangedMessage(),0,0); hr=S_OK;
Cleanup: if(!published && RStrLen(temporary)) DeleteFileW(temporary.c_wstr()); delete entry; FreeEntries(entries); if(entered) m_mutating=FALSE; if(locked) Unlock(); return hr;
}
HRESULT CCatalog::Remove(UINT index,IHost* host)
{
	HRESULT hr; BOOL locked=FALSE,renamed=FALSE,entered=FALSE; TArray<Entry*> entries; TStackRef<IQuadooControlRegistry> registry; CPrepared prepared;
	CRString detail,original,temporary; HANDLE usage=NULL,unknown=NULL;
	Check(Lock()); locked=TRUE; CheckIf(m_mutating,HRESULT_FROM_WIN32(ERROR_BUSY)); m_mutating=TRUE; entered=TRUE;
	hr=CanRemove(index,host,detail); CheckIf(hr==S_FALSE,HRESULT_FROM_WIN32(ERROR_BUSY)); Check(hr); Check(Checkpoint());
	Check(AcquireUsage(m_usageDirectory.c_wstr(),RSTRING_CAST(L"$unknown"),TRUE,&unknown));
	if(m_entries[index]->definition) { Check(AcquireUsage(m_usageDirectory.c_wstr(),m_entries[index]->type,TRUE,&usage)); }
	original=m_entries[index]->path; Check(RStrFormatW(&temporary,L"%ls.removing",original.c_wstr()));
	Check(CopyEntries(m_entries,entries)); delete entries[index]; entries.Remove(index,NULL); Check(Build(entries,&registry)); Check(Checkpoint()); Check(prepared.Stage(host,registry)); Check(Checkpoint());
	CheckIf(!MoveFileExW(original.c_wstr(),temporary.c_wstr(),MOVEFILE_WRITE_THROUGH),HrEnsureLastError()); renamed=TRUE; Commit(entries,registry); prepared.Commit();
	// A cleanup failure leaves an ignored tombstone, never a registered package.
	if(!DeleteFileW(temporary.c_wstr())) OutputDebugStringW(L"QuadooForms: committed package removal has a pending tombstone.\n");
	PostMessageW(HWND_BROADCAST,CatalogChangedMessage(),0,0); hr=S_OK;
Cleanup: UNREFERENCED_PARAMETER(renamed); if(usage) CloseHandle(usage); if(unknown) CloseHandle(unknown); FreeEntries(entries); if(entered) m_mutating=FALSE; if(locked) Unlock(); return hr;
}
STDMETHODIMP CCatalog::RegisterFactory(IQuadooControlFactory* factory) { HRESULT hr; BOOL locked=FALSE; Check(Lock()); locked=TRUE; CheckIf(m_mutating,HRESULT_FROM_WIN32(ERROR_BUSY)); hr=m_registry->RegisterFactory(factory); Cleanup: if(locked) Unlock(); return hr; }
STDMETHODIMP CCatalog::UnregisterFactory(RSTRING type) { HRESULT hr; BOOL locked=FALSE; Check(Lock()); locked=TRUE; CheckIf(m_mutating,HRESULT_FROM_WIN32(ERROR_BUSY)); hr=m_registry->UnregisterFactory(type); Cleanup: if(locked) Unlock(); return hr; }
STDMETHODIMP CCatalog::FindFactory(RSTRING type,IQuadooControlFactory** factory) { return m_thread!=GetCurrentThreadId() ? QuadooForm::EWrongThread : m_registry->FindFactory(type,factory); }
STDMETHODIMP CCatalog::GetFactoryCount(UINT* count) { HRESULT hr; TStackRef<IQuadooFactoryCatalog> catalog; Check(m_registry->QueryInterface(__uuidof(IQuadooFactoryCatalog),reinterpret_cast<PVOID*>(&catalog))); Check(catalog->GetFactoryCount(count)); Cleanup: return hr; }
STDMETHODIMP CCatalog::GetFactory(UINT index,IQuadooControlFactory** factory) { HRESULT hr; TStackRef<IQuadooFactoryCatalog> catalog; Check(m_registry->QueryInterface(__uuidof(IQuadooFactoryCatalog),reinterpret_cast<PVOID*>(&catalog))); Check(catalog->GetFactory(index,factory)); Cleanup: return hr; }
#define QFP_COMPONENT_FORWARD(method,args,pass) STDMETHODIMP CCatalog::method args { HRESULT hr; TStackRef<IQuadooComponentRegistry> registry; Check(m_thread==GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread); Check(m_registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&registry))); Check(registry->method pass); Cleanup: return hr; }
STDMETHODIMP CCatalog::RegisterComponentFactory(IQuadooComponentFactory* factory) { HRESULT hr; BOOL locked=FALSE; TStackRef<IQuadooComponentRegistry> registry; Check(Lock()); locked=TRUE; CheckIf(m_mutating,HRESULT_FROM_WIN32(ERROR_BUSY)); Check(m_registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&registry))); Check(registry->RegisterComponentFactory(factory)); Cleanup: if(locked) Unlock(); return hr; }
STDMETHODIMP CCatalog::UnregisterComponentFactory(RSTRING type) { HRESULT hr; BOOL locked=FALSE; TStackRef<IQuadooComponentRegistry> registry; Check(Lock()); locked=TRUE; CheckIf(m_mutating,HRESULT_FROM_WIN32(ERROR_BUSY)); Check(m_registry->QueryInterface(__uuidof(IQuadooComponentRegistry),reinterpret_cast<PVOID*>(&registry))); Check(registry->UnregisterComponentFactory(type)); Cleanup: if(locked) Unlock(); return hr; }
QFP_COMPONENT_FORWARD(FindComponentFactory,(RSTRING type,IQuadooComponentFactory** factory),(type,factory))
QFP_COMPONENT_FORWARD(GetComponentFactoryCount,(UINT* count),(count))
QFP_COMPONENT_FORWARD(GetComponentFactory,(UINT index,IQuadooComponentFactory** factory),(index,factory))
}
