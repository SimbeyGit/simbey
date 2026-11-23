#include "Packages.h"
#include "..\Imaging\ImageResource.h"
#include <float.h>

namespace QFP
{
Property::~Property() { SafeRelease(initial); for(sysint n=0;n<choices.Length();n++) delete choices[n]; }
Event::~Event() { for(sysint n=0;n<arguments.Length();n++) delete arguments[n]; }
CDefinition::~CDefinition() { for(sysint n=0;n<properties.Length();n++) delete properties[n]; for(sysint n=0;n<events.Length();n++) delete events[n]; }
HRESULT ReadString(IJSONObject* object, PCWSTR name, CRString& value, PCWSTR fallback)
{
	HRESULT hr; TStackRef<IJSONValue> item;
	if(!object->HasField(RSTRING_CAST(name)))
	{
		CheckIf(!fallback,E_INVALIDARG);
		Check(RStrCreateW(TStrLenAssert(fallback),fallback,&value));
	}
	else { Check(object->FindNonNullValueW(name,&item)); Check(item->GetString(&value)); }
Cleanup:
	return hr;
}
HRESULT ReadInt(IJSONObject* object, PCWSTR name, INT* value, INT fallback)
{
	HRESULT hr = S_OK; TStackRef<IJSONValue> item; *value=fallback;
	if(object->HasField(RSTRING_CAST(name))) { Check(object->FindNonNullValueW(name,&item)); Check(item->GetInteger(value)); }
Cleanup:
	return hr;
}
HRESULT ReadBool(IJSONObject* object, PCWSTR name, BOOL* value, BOOL fallback)
{
	HRESULT hr = S_OK; TStackRef<IJSONValue> item; bool flag=fallback!=FALSE;
	if(object->HasField(RSTRING_CAST(name))) { Check(object->FindNonNullValueW(name,&item)); Check(item->GetBoolean(&flag)); }
	*value=flag;
Cleanup:
	return hr;
}
static BOOL InNames(PCWSTR names, RSTRING name)
{
	UINT length=RStrLen(name);
	for(PCWSTR p=names; *p; )
	{
		PCWSTR end=p; while(*end && *end!=L'|') end++;
		if(static_cast<UINT>(end-p)==length && !memcmp(p,RStrToWide(name),length*sizeof(WCHAR))) return TRUE;
		p=*end ? end+1 : end;
	}
	return FALSE;
}
HRESULT Fields(IJSONObject* object, PCWSTR names)
{
	HRESULT hr=S_OK;
	CheckIf(!object || object->Count()>1024,E_INVALIDARG);
	for(sysint n=0;n<object->Count();n++) { CRString name; Check(object->GetValueName(n,&name)); CheckIf(!InNames(names,name),E_INVALIDARG); }
Cleanup:
	return hr;
}
static HRESULT Integral(IJSONValue* input, LONGLONG* number)
{
	HRESULT hr; INT integer;
	CheckIf(!input,E_INVALIDARG);
	if(input->GetType()==JSON::Integer) { Check(input->GetInteger(&integer)); *number=integer; }
	else { Check(input->GetLongInteger(number)); }
Cleanup:
	return hr;
}
HRESULT Normalize(const Property* property, IJSONValue* input, IJSONValue** output)
{
	HRESULT hr=S_OK; CRString text; LONGLONG number=0; bool flag; DOUBLE real;
	TStackRef<IJSONObject> object, normalized; TStackRef<IJSONArray> array;
	CheckIf(!property || !output,E_POINTER); *output=NULL;
	if(property->type==QuadooProperty::Icon)
	{
		HICON icon=NULL; hr=QuadooLoadIconValue(input,20,20,&icon); if(icon) DestroyIcon(icon); Check(hr);
		Check(JSONClone(input,output,TRUE));
	}
	else if(property->type==QuadooProperty::Picture)
	{
		if(input) { TStackRef<QFI::CImageResource> image; Check(QFI::CImageResource::Create(input,&image)); }
		Check(JSONClone(input,output,TRUE));
	}
	else if(property->type == QuadooProperty::ObjectReference && !input)
	{
		// Optional references such as the inherited PopupMenu have a null default.
		hr = S_OK;
	}
	else
	{
		CheckIf(!input,E_INVALIDARG);
		switch(property->type)
		{
		case QuadooProperty::String: case QuadooProperty::Memo: case QuadooProperty::Event: case QuadooProperty::ObjectReference:
			Check(input->GetString(&text));
			CheckIf(!RStrIsWide(static_cast<RSTRING>(text)) || RStrLen(text)>524288,E_INVALIDARG);
			for(INT n=0;n<RStrLen(text);n++) { CheckIf(!text.c_wstr()[n] || (property->type!=QuadooProperty::Memo && (text.c_wstr()[n]==L'\r' || text.c_wstr()[n]==L'\n')),E_INVALIDARG); }
			Check(JSONCreateString(text,output)); break;
		case QuadooProperty::Boolean:
			Check(input->GetBoolean(&flag)); Check(JSONCreateBool(flag,output)); break;
		case QuadooProperty::Float:
			if(input->GetType()==JSON::Integer || input->GetType()==JSON::LongInteger) { Check(Integral(input,&number)); real=static_cast<DOUBLE>(number); }
			else if(input->GetType()==JSON::Float) { FLOAT single; Check(input->GetFloat(&single)); real=single; }
			else { Check(input->GetDouble(&real)); }
			CheckIf(!_finite(real),E_INVALIDARG); Check(JSONCreateDouble(real,output)); break;
		case QuadooProperty::List:
			Check(input->GetArray(&array)); CheckIf(array->Count()>65536,E_INVALIDARG);
			for(sysint n=0;n<array->Count();n++) { CRString item; Check(array->GetString(n,&item)); CheckIf(!RStrIsWide(static_cast<RSTRING>(item)),E_INVALIDARG); }
			Check(JSONClone(input,output,TRUE)); break;
		case QuadooProperty::Collection:
			CheckIf(input->GetType()!=JSON::Object && input->GetType()!=JSON::Array,E_INVALIDARG); Check(JSONClone(input,output,TRUE)); break;
		case QuadooProperty::Rect: case QuadooProperty::Point: case QuadooProperty::Size: case QuadooProperty::Font:
			{
				Check(input->GetObject(&object)); Check(JSONCreateObject(&normalized));
				if(property->type==QuadooProperty::Font)
				{
					BOOL bold,italic; INT size;
					Check(Fields(object,L"Name|Size|Bold|Italic")); Check(ReadString(object,L"Name",text,L"Segoe UI"));
					Check(ReadInt(object,L"Size",&size,9)); CheckIf(size<1 || size>256,E_INVALIDARG);
					Check(ReadBool(object,L"Bold",&bold)); Check(ReadBool(object,L"Italic",&italic));
					Check(QFD::PutString(normalized,L"Name",text)); Check(QFD::PutInt(normalized,L"Size",size));
					TStackRef<IJSONValue> value; Check(JSONCreateBool(bold!=FALSE,&value)); Check(normalized->AddValueW(L"Bold",value)); value.Release();
					Check(JSONCreateBool(italic!=FALSE,&value)); Check(normalized->AddValueW(L"Italic",value));
				}
				else
				{
					static PCWSTR const rect[]={L"Left",L"Top",L"Width",L"Height"}, insets[]={L"Left",L"Top",L"Right",L"Bottom"}, point[]={L"X",L"Y"}, size[]={L"Width",L"Height"};
					BOOL inset=property->name==RSTRING_CAST(L"Margins") || property->name==RSTRING_CAST(L"Padding");
					PCWSTR const* fields=property->type==QuadooProperty::Rect ? (inset ? insets : rect) : property->type==QuadooProperty::Point ? point : size;
					UINT count=property->type==QuadooProperty::Rect ? 4 : 2;
					Check(Fields(object,property->type==QuadooProperty::Rect ? (inset ? L"Left|Top|Right|Bottom" : L"Left|Top|Width|Height") : property->type==QuadooProperty::Point ? L"X|Y" : L"Width|Height"));
					for(UINT n=0;n<count;n++) { INT value; Check(ReadInt(object,fields[n],&value)); CheckIf(value < -1000000 || value>1000000 || ((inset || !lstrcmpW(fields[n],L"Width") || !lstrcmpW(fields[n],L"Height")) && value<0),E_INVALIDARG); Check(QFD::PutInt(normalized,fields[n],value)); }
				}
				Check(JSONWrapObject(normalized,output));
			}
			break;
		default:
			if(input->GetType()==JSON::String && property->choices.Length())
			{
				Check(input->GetString(&text)); sysint n;
				for(n=0;n<property->choices.Length();n++) if(text==static_cast<RSTRING>(property->choices[n]->name)) break;
				CheckIf(n==property->choices.Length(),E_INVALIDARG); number=property->choices[n]->value;
			}
			else { Check(Integral(input,&number)); }
			if(property->type!=QuadooProperty::Long) { CheckIf(number<INT_MIN || number>INT_MAX,E_INVALIDARG); }
			if(property->type==QuadooProperty::Color) { DWORD tag=static_cast<DWORD>(number)&0xff000000; CheckIf(tag!=0 && tag!=0x80000000,E_INVALIDARG); }
			if(property->type==QuadooProperty::Enum)
			{
				sysint n; for(n=0;n<property->choices.Length();n++) if(property->choices[n]->value==number) break;
				CheckIf(n==property->choices.Length(),E_INVALIDARG);
			}
			if(property->type==QuadooProperty::Flags)
			{
				LONGLONG bits=0; for(sysint n=0;n<property->choices.Length();n++) bits|=property->choices[n]->value;
				CheckIf(number<0 || (number&~bits),E_INVALIDARG);
			}
			if(InNames(L"Left|Top|Width|Height|MinWidth|MinHeight|MaxWidth|MaxHeight|TabOrder",property->name))
			{
				CheckIf(number < -1000000 || number>1000000,E_INVALIDARG);
				CheckIf(!(property->name==RSTRING_CAST(L"Left")) && !(property->name==RSTRING_CAST(L"Top")) && number<0,E_INVALIDARG);
			}
			if(property->type==QuadooProperty::Long) { Check(JSONCreateLongInteger(number,output)); }
			else { Check(JSONCreateInteger(static_cast<INT>(number),output)); }
			break;
		}
	}
Cleanup:
	return hr;
}
class CPropertyDescriptor : public CBaseUnknown, public IQuadooPropertyDescriptor
{
	TStackRef<CDefinition> m_owner; Property* m_property;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyDescriptor)
	END_UNK_MAP
	CPropertyDescriptor(CDefinition* owner, Property* property) : m_owner(owner), m_property(property) {}
	STDMETHOD(GetName)(RSTRING* value) { return CopyString(m_property->name,value); }
	STDMETHOD(GetDisplayName)(RSTRING* value) { return CopyString(m_property->display,value); }
	STDMETHOD(GetCategory)(RSTRING* value) { return CopyString(m_property->category,value); }
	STDMETHOD(GetDescription)(RSTRING* value) { return CopyString(m_property->description,value); }
	STDMETHOD(GetType)(QuadooProperty::Type* value) { if(value) *value=m_property->type; return value ? S_OK : E_POINTER; }
	STDMETHOD(GetFlags)(DWORD* value) { if(value) *value=m_property->flags; return value ? S_OK : E_POINTER; }
	STDMETHOD(GetDefaultValue)(IJSONValue** value) { return value ? JSONClone(m_property->initial,value,TRUE) : E_POINTER; }
	STDMETHOD(GetEnumCount)(UINT* count) { if(count) *count=static_cast<UINT>(m_property->choices.Length()); return count ? S_OK : E_POINTER; }
	STDMETHOD(GetEnumValue)(UINT index,RSTRING* name,LONGLONG* value)
	{
		HRESULT hr; CheckIf(!name || !value,E_POINTER); *name=NULL;
		CheckIf(index>=static_cast<UINT>(m_property->choices.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		Check(CopyString(m_property->choices[index]->name,name)); *value=m_property->choices[index]->value;
	Cleanup: return hr;
	}
};
class CProperties : public CBaseUnknown, public IQuadooPropertyDescriptors
{
	TStackRef<CDefinition> m_owner;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooPropertyDescriptors)
	END_UNK_MAP
	CProperties(CDefinition* owner) : m_owner(owner) {}
	STDMETHOD(GetCount)(UINT* count) { if(count) *count=static_cast<UINT>(m_owner->properties.Length()); return count ? S_OK : E_POINTER; }
	STDMETHOD(GetAt)(UINT index,IQuadooPropertyDescriptor** value)
	{
		HRESULT hr=S_OK; CheckIf(!value,E_POINTER); *value=NULL;
		CheckIf(index>=static_cast<UINT>(m_owner->properties.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		*value=__new CPropertyDescriptor(m_owner,m_owner->properties[index]); CheckAlloc(*value);
	Cleanup: return hr;
	}
	STDMETHOD(Find)(RSTRING name,IQuadooPropertyDescriptor** value) { if(!value) return E_POINTER; *value=NULL; UINT n=m_owner->Find(RStrToWide(name)); return n==UINT_MAX ? DISP_E_MEMBERNOTFOUND : GetAt(n,value); }
};
class CEventDescriptor : public CBaseUnknown, public IQuadooEventDescriptor
{
	TStackRef<CDefinition> m_owner; Event* m_event;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventDescriptor)
	END_UNK_MAP
	CEventDescriptor(CDefinition* owner,Event* event) : m_owner(owner), m_event(event) {}
	STDMETHOD(GetName)(RSTRING* value) { return CopyString(m_event->name,value); }
	STDMETHOD(GetCategory)(RSTRING* value) { return CopyString(m_event->category,value); }
	STDMETHOD(GetFlags)(DWORD* flags) { if(flags) *flags=m_event->flags; return flags ? S_OK : E_POINTER; }
	STDMETHOD(GetArgumentCount)(UINT* count) { if(count) *count=static_cast<UINT>(m_event->arguments.Length()); return count ? S_OK : E_POINTER; }
	STDMETHOD(GetArgument)(UINT index,RSTRING* name,QuadooForm::EventValueType* type,BOOL* writable)
	{
		HRESULT hr; CheckIf(!name || !type || !writable,E_POINTER); *name=NULL;
		CheckIf(index>=static_cast<UINT>(m_event->arguments.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		Check(CopyString(m_event->arguments[index]->name,name)); *type=m_event->arguments[index]->type; *writable=m_event->arguments[index]->writable;
	Cleanup: return hr;
	}
};
class CEvents : public CBaseUnknown, public IQuadooEventDescriptors
{
	TStackRef<CDefinition> m_owner;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooEventDescriptors)
	END_UNK_MAP
	CEvents(CDefinition* owner) : m_owner(owner) {}
	STDMETHOD(GetCount)(UINT* count) { if(count) *count=static_cast<UINT>(m_owner->events.Length()); return count ? S_OK : E_POINTER; }
	STDMETHOD(GetAt)(UINT index,IQuadooEventDescriptor** value)
	{
		HRESULT hr=S_OK; CheckIf(!value,E_POINTER); *value=NULL;
		CheckIf(index>=static_cast<UINT>(m_owner->events.Length()),HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));
		*value=__new CEventDescriptor(m_owner,m_owner->events[index]); CheckAlloc(*value);
	Cleanup: return hr;
	}
	STDMETHOD(Find)(RSTRING name,IQuadooEventDescriptor** value)
	{
		if(!value) return E_POINTER; *value=NULL;
		for(sysint n=0;n<m_owner->events.Length();n++) if(m_owner->events[n]->name==name) return GetAt(static_cast<UINT>(n),value);
		return DISP_E_MEMBERNOTFOUND;
	}
};
UINT CDefinition::Find(PCWSTR name) const { if(name) for(sysint n=0;n<properties.Length();n++) if(!lstrcmpW(properties[n]->name.c_wstr(),name)) return static_cast<UINT>(n); return UINT_MAX; }
HRESULT CDefinition::GetProperties(IQuadooPropertyDescriptors** value) { if(!value) return E_POINTER; *value=__new CProperties(this); return *value ? S_OK : E_OUTOFMEMORY; }
HRESULT CDefinition::GetEvents(IQuadooEventDescriptors** value) { if(!value) return E_POINTER; *value=__new CEvents(this); return *value ? S_OK : E_OUTOFMEMORY; }
}
