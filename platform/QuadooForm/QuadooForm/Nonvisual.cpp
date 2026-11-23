#include "Runtime.h"
#include "ComponentRuntime.h"
#include "Library\ChooseFile.h"
#include "Library\Core\MemoryStream.h"

namespace QF
{
static const DWORD Stored = QuadooForm::PropertySerializable;
static const PropertySpec identity[] = { { L"Name",QuadooProperty::String,Stored,0,0 } };
static const EventSpec lifetime[] = { { L"OnCreate",0,0,NULL,0 }, { L"OnDestroy",0,0,NULL,1 } };
const TypeSpec NonvisualType = { L"Component",NULL,identity,ARRAYSIZE(identity),lifetime,ARRAYSIZE(lifetime),0 };
static const EnumSpec options[] = { { L"FileMustExist",1 }, { L"PathMustExist",2 }, { L"OverwritePrompt",4 }, { L"NoChangeDir",8 }, { L"NoReadOnlyReturn",16 } };
static const PropertySpec dialogProperties[] =
{
	{ L"Title",QuadooProperty::String,Stored,0,1 }, { L"Filter",QuadooProperty::String,Stored,0,2 },
	{ L"FilterIndex",QuadooProperty::Int,Stored,1,3 }, { L"InitialDir",QuadooProperty::String,Stored,0,4 },
	{ L"DefaultExt",QuadooProperty::String,Stored,0,5 }, { L"FileName",QuadooProperty::String,Stored,0,6 }
};
static const EventSpec dialogEvents[] = { { L"OnAccept",0,0,NULL,2 }, { L"OnCancel",0,0,NULL,3 } };
static const TypeSpec dialogType = { L"FileDialog",&NonvisualType,dialogProperties,ARRAYSIZE(dialogProperties),dialogEvents,ARRAYSIZE(dialogEvents),0 };
static const PropertySpec openOptions[] = { { L"Options",QuadooProperty::Flags,Stored,11,7,options,ARRAYSIZE(options) } };
static const PropertySpec saveOptions[] = { { L"Options",QuadooProperty::Flags,Stored,14,7,options,ARRAYSIZE(options) } };
static const TypeSpec openType = { L"OpenFile",&dialogType,openOptions,ARRAYSIZE(openOptions),NULL,0,IDI_PALETTE_OPENFILE };
static const TypeSpec saveType = { L"SaveFile",&dialogType,saveOptions,ARRAYSIZE(saveOptions),NULL,0,IDI_PALETTE_SAVEFILE };

static HRESULT FilterBuffer(RSTRING text, CMemoryStream* buffer, UINT* groups)
{
	HRESULT hr = S_OK;
	UINT separators = 0, length = RStrLen(text), previous = 0; PWSTR write;
	*groups = 1;
	if(!length) text = RSTRING_CAST(L"All files (*.*)|*.*");
	length = RStrLen(text);
	CheckIf(length > 65530,E_INVALIDARG);
	for(UINT n = 0; n < length; n++)
	{
		WCHAR ch = RStrToWide(text)[n];
		CheckIf(!ch || ch == L'\r' || ch == L'\n',E_INVALIDARG);
		if(ch == L'|') { CheckIf(n == previous,E_INVALIDARG); previous = n + 1; separators++; }
	}
	CheckIf(!(separators & 1) || previous == length,E_INVALIDARG);
	*groups = (separators + 1) / 2;
	CheckIf(*groups > 4096,E_INVALIDARG);
	if(buffer)
	{
		Check(buffer->TWriteAdvance(&write,length + 2));
		for(UINT n = 0; n < length; n++) write[n] = RStrToWide(text)[n] == L'|' ? 0 : RStrToWide(text)[n];
		write[length] = write[length + 1] = 0;
	}
Cleanup:
	return hr;
}
class CFileDialog : public CNonvisual, public IQuadooFileDialog
{
	BOOL m_executing;
protected:
	virtual BOOL OpenDialog() const = 0;
	virtual HRESULT ValidateProperty(PCWSTR name, IJSONValue* value)
	{
		HRESULT hr = S_OK; INT number;
		if(!TStrCmpAssert(name,L"Options")) { Check(value->GetInteger(&number)); CheckIf(number & ~31,E_INVALIDARG); }
		else if(!TStrCmpAssert(name,L"FilterIndex")) { Check(value->GetInteger(&number)); CheckIf(number < 1 || number > 4096,E_INVALIDARG); }
		else if(!TStrCmpAssert(name,L"Filter")) { CRString text; UINT groups; Check(value->GetString(&text)); Check(FilterBuffer(text,NULL,&groups)); }
		else if(value && value->GetType() == JSON::String)
		{
			CRString text; Check(value->GetString(&text)); CheckIf(RStrLen(text) > 65530,E_INVALIDARG);
			for(INT n = 0; n < RStrLen(text); n++)
			{
				WCHAR ch = text.c_wstr()[n];
				CheckIf(!ch,E_INVALIDARG);
				if(!TStrCmpAssert(name,L"DefaultExt")) { CheckIf(ch == L'.' || ch == L'|' || ch == L'/' || ch == L'\\' || ch == L'*' || ch == L'?' || ch == L':' || ch == L'"' || ch == L'<' || ch == L'>',E_INVALIDARG); }
			}
		}
	Cleanup:
		return hr;
	}
	virtual HRESULT ChangedProperty(PCWSTR name)
	{
		HRESULT hr = S_OK; CRString filter; UINT groups;
		if(!TStrCmpAssert(name,L"Filter") || !TStrCmpAssert(name,L"FilterIndex"))
		{ Check(String(L"Filter",&filter)); Check(FilterBuffer(filter,NULL,&groups)); CheckIf(static_cast<UINT>(Integer(L"FilterIndex")) > groups,E_INVALIDARG); }
	Cleanup:
		return hr;
	}
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooFileDialog)
		UNK_CHAIN_TO(CNonvisual)
	END_UNK_MAP
	CFileDialog(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CNonvisual(context,owner,factory,live), m_executing(FALSE) {}
	HRESULT Construct(IJSONObject* definition, IQuadooPropertyDescriptors* properties, IQuadooEventDescriptors* events)
	{
		HRESULT hr;
		Check(CComponentObject::Construct(definition,properties,events));
		Check(ChangedProperty(L"FilterIndex"));
	Cleanup:
		return hr;
	}
	STDMETHOD(Execute)(HWND owner, BOOL* accepted)
	{
		HRESULT hr;
		TStackRef<CFileDialog> keepAlive(this); TStackRef<IQuadooControlContainer> root;
		CChooseFile chooser; CRString title, filter, initialDir, extension, filename;
		CMemoryStream nativeFilter; TStackRef<IJSONValue> selected, selectedIndex;
		TStackRef<IFileDialogDriver> driver; CRString selectedName;
		TStackRef<IJSONValue> previousFile, previousIndex;
		UINT groups, index, fileIndex = 0, filterIndex = 0; DWORD flags = OFN_HIDEREADONLY;
		BOOL entered = FALSE, committed = FALSE, oldFileEdited = FALSE, oldFilterEdited = FALSE;
		CheckIf(!accepted,E_POINTER); *accepted = FALSE;
		Check(CheckAlive());
		CheckIf(m_state != QuadooForm::Ready || m_mode == QuadooForm::Design || m_executing,QuadooForm::EInvalidState);
		if(owner) { CheckIf(!IsWindow(owner) || GetWindowThreadProcessId(owner,NULL) != m_thread,E_INVALIDARG); }
		else
		{
			CheckIf(!m_owner,QuadooForm::EDestroyed);
			Check(m_owner->GetRoot(&root)); Check(root->GetHandle(&owner));
			CheckIf(!owner || !IsWindow(owner),QuadooForm::EInvalidState);
			owner = GetAncestor(owner,GA_ROOT);
		}
		Check(String(L"Title",&title)); Check(String(L"Filter",&filter));
		Check(String(L"InitialDir",&initialDir)); Check(String(L"DefaultExt",&extension)); Check(String(L"FileName",&filename));
		Check(FilterBuffer(filter,&nativeFilter,&groups));
		index = Integer(L"FilterIndex"); CheckIf(!index || index > groups,E_INVALIDARG);
		INT options = Integer(L"Options");
		if(options & 1) flags |= OFN_FILEMUSTEXIST; if(options & 2) flags |= OFN_PATHMUSTEXIST;
		if(options & 4) flags |= OFN_OVERWRITEPROMPT; if(options & 8) flags |= OFN_NOCHANGEDIR; if(options & 16) flags |= OFN_NOREADONLYRETURN;
		Check(QueryOptional(m_context,&driver));
		if(!driver) { Check(chooser.Initialize()); Check(chooser.SetInitialDirectory(initialDir.c_wstr())); }
		m_executing = TRUE; entered = TRUE;
		if(driver) hr = driver->Choose(owner,OpenDialog(),title,filename,filter,initialDir,extension,index,options,&selectedName,&index);
		else hr = chooser.Choose(owner,OpenDialog(),RStrLen(title) ? title.c_wstr() : NULL,filename.c_wstr(),nativeFilter.TGetReadPtr<WCHAR>(),extension.c_wstr(),index,flags,&index);
		if(hr != S_OK) { if(hr == S_FALSE && m_state == QuadooForm::Ready) { HRESULT callback = Dispatch(L"OnCancel"); if(FAILED(callback)) hr = callback; } goto Cleanup; }
		Check(CheckAlive());
		if(!driver) { CheckIf(chooser.GetFileCount() != 1,E_UNEXPECTED); Check(RStrCreateW(TStrLenAssert(chooser.GetFile(0)),chooser.GetFile(0),&selectedName)); }
		CheckIf(!RStrIsWide(static_cast<RSTRING>(selectedName)) || !RStrLen(selectedName) || RStrLen(selectedName) > 65530 || index < 1 || index > groups,E_UNEXPECTED);
		Check(JSONCreateString(selectedName,&selected));
		Check(JSONCreateInteger(index,&selectedIndex));
		// Stage both results before publishing, so allocation failures leave the
		// previous filename and filter index intact. Notify only after both commit.
		fileIndex = Find(L"FileName"); filterIndex = Find(L"FilterIndex");
		CheckIf(fileIndex >= static_cast<UINT>(m_values.Length()) || filterIndex >= static_cast<UINT>(m_values.Length()),E_UNEXPECTED);
		BOOL fileChanged = JSONCompare(m_values[fileIndex].value,selected) != 0;
		BOOL filterChanged = JSONCompare(m_values[filterIndex].value,selectedIndex) != 0;
		previousFile.Attach(m_values[fileIndex].value); previousIndex.Attach(m_values[filterIndex].value);
		selected->AddRef(); selectedIndex->AddRef(); m_values[fileIndex].value = selected; m_values[filterIndex].value = selectedIndex;
		oldFileEdited = m_values[fileIndex].edited; oldFilterEdited = m_values[filterIndex].edited; committed = TRUE;
		m_values[fileIndex].edited = m_values[fileIndex].edited || fileChanged; m_values[filterIndex].edited = m_values[filterIndex].edited || filterChanged;
		*accepted = TRUE;
		Check(Dispatch(L"OnAccept"));
		if(m_state == QuadooForm::Ready && fileChanged) { Check(Notify(fileIndex)); }
		if(m_state == QuadooForm::Ready && filterChanged) { Check(Notify(filterIndex)); }
		hr = S_OK;
	Cleanup:
		if(FAILED(hr) && committed)
		{
			SafeRelease(m_values[fileIndex].value); m_values[fileIndex].value = previousFile.Detach(); m_values[fileIndex].edited = oldFileEdited;
			SafeRelease(m_values[filterIndex].value); m_values[filterIndex].value = previousIndex.Detach(); m_values[filterIndex].edited = oldFilterEdited;
			*accepted = FALSE;
		}
		if(entered) m_executing = FALSE;
		return hr;
	}
};
class COpenFile : public CFileDialog
{
	virtual BOOL OpenDialog() const { return TRUE; }
public:
	static const TypeSpec* Type() { return &openType; }
	COpenFile(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CFileDialog(context,owner,factory,live) {}
};
class CSaveFile : public CFileDialog
{
	virtual BOOL OpenDialog() const { return FALSE; }
public:
	static const TypeSpec* Type() { return &saveType; }
	CSaveFile(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CFileDialog(context,owner,factory,live) {}
};
HRESULT RegisterImageListFactory(IQuadooComponentRegistry* registry);
HRESULT RegisterColorDialogFactory(IQuadooComponentRegistry* registry);
HRESULT RegisterNonvisualFactories(IQuadooComponentRegistry* registry)
{
	HRESULT hr;
	Check(RegisterColorDialogFactory(registry));
	Check(RegisterImageListFactory(registry));
	{ TStackRef<TComponentFactory<COpenFile> > factory; factory.Attach(__new TComponentFactory<COpenFile>); CheckAlloc(factory); Check(factory->Initialize()); Check(registry->RegisterComponentFactory(factory)); }
	{ TStackRef<TComponentFactory<CSaveFile> > factory; factory.Attach(__new TComponentFactory<CSaveFile>); CheckAlloc(factory); Check(factory->Initialize()); Check(registry->RegisterComponentFactory(factory)); }
Cleanup:
	return hr;
}
}
