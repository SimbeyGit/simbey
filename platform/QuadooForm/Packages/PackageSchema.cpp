#include "Packages.h"
#include "..\Imaging\ImageResource.h"

namespace QFP
{
// Walk the original tokens before JSONParse, which intentionally permits object
// replacement. Decode key tokens through JSON itself so escaped aliases collide.
class CUniqueKeys
{
	PCWSTR m_text; UINT m_length,m_at;
	VOID Space() { while(m_at<m_length && (m_text[m_at]==L' ' || m_text[m_at]==L'\r' || m_text[m_at]==L'\n' || m_text[m_at]==L'\t')) m_at++; }
	HRESULT String(RSTRING* key)
	{
		HRESULT hr=S_OK; UINT start=m_at; TStackRef<IJSONValue> value;
		CheckIf(m_at>=m_length || m_text[m_at++]!=L'"',E_INVALIDARG);
		while(m_at<m_length)
		{
			WCHAR ch=m_text[m_at++];
			if(ch==L'\\') { CheckIf(m_at>=m_length,E_INVALIDARG); m_at++; }
			else if(ch==L'"') { if(key) { Check(JSONParse(NULL,m_text+start,m_at-start,&value)); Check(value->GetString(key)); } goto Cleanup; }
		}
		hr=E_INVALIDARG;
	Cleanup: return hr;
	}
public:
	CUniqueKeys(PCWSTR text,UINT length) : m_text(text),m_length(length),m_at(0) {}
	HRESULT Value(UINT depth=0)
	{
		HRESULT hr=S_OK; TArray<RSTRING> keys; WCHAR close=0; BOOL object=FALSE;
		CheckIf(depth>64,E_INVALIDARG); Space(); CheckIf(m_at>=m_length,E_INVALIDARG);
		if(m_text[m_at]==L'"') { Check(String(NULL)); }
		else if(m_text[m_at]==L'{' || m_text[m_at]==L'[')
		{
			object=m_text[m_at++]==L'{'; close=object ? L'}' : L']'; Space();
			if(m_at<m_length && m_text[m_at]==close) m_at++;
			else for(;;)
			{
				if(object)
				{
					CRString key; Check(String(&key));
					for(sysint n=0;n<keys.Length();n++) { CheckIf(key==keys[n],E_INVALIDARG); }
					CheckIf(keys.Length()>=1024,E_INVALIDARG); RSTRING pointer=key; Check(keys.Append(pointer)); key.Detach();
					Space(); CheckIf(m_at>=m_length || m_text[m_at++]!=L':',E_INVALIDARG);
				}
				Check(Value(depth+1)); Space(); CheckIf(m_at>=m_length,E_INVALIDARG);
				WCHAR ch=m_text[m_at++]; if(ch==close) break;
				CheckIf(ch!=L',',E_INVALIDARG); Space();
			}
		}
		else
		{
			UINT start=m_at;
			while(m_at<m_length && m_text[m_at]!=L',' && m_text[m_at]!=L']' && m_text[m_at]!=L'}' && m_text[m_at]>L' ') m_at++;
			CheckIf(start==m_at,E_INVALIDARG);
		}
		if(!depth) { Space(); CheckIf(m_at!=m_length,E_INVALIDARG); }
	Cleanup:
		for(sysint n=0;n<keys.Length();n++) RStrRelease(keys[n]);
		return hr;
	}
};
HRESULT ParseJSON(PCWSTR text,UINT length,IJSONObject** object)
{
	HRESULT hr; CUniqueKeys keys(text,length);
	CheckIf(!object,E_POINTER); *object=NULL;
	CheckIf(!text || length>MaxPackageBytes,E_INVALIDARG); Check(keys.Value()); Check(QFD::Parse(text,length,object));
Cleanup: return hr;
}
HRESULT LoadJSON(PCWSTR path,IJSONObject** object)
{
	HRESULT hr; PWSTR text=NULL; INT length; WIN32_FILE_ATTRIBUTE_DATA data;
	CheckIf(!object,E_POINTER); *object=NULL;
	CheckIf(!GetFileAttributesExW(path,GetFileExInfoStandard,&data),HrEnsureLastError());
	CheckIf(data.nFileSizeHigh || data.nFileSizeLow>MaxPackageBytes,E_INVALIDARG);
	Check(Text::LoadFromFile(path,&text,&length,CP_UTF8)); Check(ParseJSON(text,length,object));
Cleanup: SafeDeleteArray(text); return hr;
}
static HRESULT Identifier(RSTRING text)
{
	HRESULT hr=S_OK;
	CheckIf(!RStrIsWide(text) || !RStrLen(text) || RStrLen(text)>128,E_INVALIDARG);
	for(INT n=0;n<RStrLen(text);n++)
	{
		WCHAR ch=RStrToWide(text)[n];
		CheckIf(!((ch>=L'A' && ch<=L'Z') || (ch>=L'a' && ch<=L'z') || ch==L'_' || (n && ch>=L'0' && ch<=L'9')),E_INVALIDARG);
	}
Cleanup: return hr;
}
static HRESULT AddBase(CDefinition* target)
{
	HRESULT hr; TStackRef<IQuadooControlRegistry> registry; TStackRef<IQuadooControlFactory> factory;
	TStackRef<IQuadooPropertyDescriptors> descriptors; UINT count; Property* property=NULL;
	Check(QuadooGetControlRegistry(&registry)); Check(registry->FindFactory(RSTRING_CAST(L"Panel"),&factory)); Check(factory->GetProperties(&descriptors)); Check(descriptors->GetCount(&count));
	for(UINT n=0;n<count;n++)
	{
		TStackRef<IQuadooPropertyDescriptor> descriptor; CRString name;
		Check(descriptors->GetAt(n,&descriptor)); Check(descriptor->GetName(&name));
		QuadooProperty::Type baseType; Check(descriptor->GetType(&baseType)); if(baseType==QuadooProperty::Event) continue;
		if(!target->visual && !(name==RSTRING_CAST(L"Name"))) continue;
		if(target->visual && !target->container && (name==RSTRING_CAST(L"Padding") || name==RSTRING_CAST(L"Caption") || name==RSTRING_CAST(L"AutoSize"))) continue;
		property=__new Property; CheckAlloc(property); property->name=name;
		Check(descriptor->GetDisplayName(&property->display)); Check(descriptor->GetCategory(&property->category)); Check(descriptor->GetDescription(&property->description));
		Check(descriptor->GetType(&property->type)); Check(descriptor->GetFlags(&property->flags)); Check(descriptor->GetDefaultValue(&property->initial));
		UINT choices; Check(descriptor->GetEnumCount(&choices));
		for(UINT c=0;c<choices;c++)
		{
			Choice* choice=__new Choice; CheckAlloc(choice);
			hr=descriptor->GetEnumValue(c,&choice->name,&choice->value);
			if(SUCCEEDED(hr)) hr=property->choices.Append(choice);
			if(FAILED(hr)) { delete choice; Check(hr); }
		}
		Check(target->properties.Append(property)); property=NULL;
	}
Cleanup: delete property; return hr;
}
static HRESULT Default(Property* property,IJSONValue** value)
{
	HRESULT hr; TStackRef<IJSONObject> object; TStackRef<IJSONArray> array;
	switch(property->type)
	{
	case QuadooProperty::String: case QuadooProperty::Memo: case QuadooProperty::Event: case QuadooProperty::ObjectReference: hr=JSONCreateString(RSTRING_CAST(L""),value); break;
	case QuadooProperty::Boolean: hr=JSONCreateBool(false,value); break;
	case QuadooProperty::Float: hr=JSONCreateDouble(0,value); break;
	case QuadooProperty::Long: hr=JSONCreateLongInteger(0,value); break;
	case QuadooProperty::List: case QuadooProperty::Collection: Check(JSONCreateArray(&array)); Check(JSONWrapArray(array,value)); break;
	case QuadooProperty::Font: case QuadooProperty::Rect: case QuadooProperty::Point: case QuadooProperty::Size: Check(JSONCreateObject(&object)); Check(JSONWrapObject(object,value)); break;
	case QuadooProperty::Icon: case QuadooProperty::Picture: *value=NULL; hr=S_OK; break;
	default: hr=JSONCreateInteger(property->type==QuadooProperty::Enum && property->choices.Length() ? static_cast<INT>(property->choices[0]->value) : 0,value); break;
	}
Cleanup: return hr;
}
static HRESULT AddProperty(CDefinition* target,IJSONObject* source,BOOL event)
{
	HRESULT hr; Property* property=NULL; TStackRef<IJSONValue> value; CRString type; BOOL readOnly;
	static PCWSTR const types[]={L"Int",L"Long",L"String",L"Boolean",L"List",L"Icon",L"Color",L"Event",L"Enum",L"Flags",L"Float",L"Rect",L"Point",L"Size",L"Font",L"ObjectReference",L"Collection",L"Memo",L"Picture"};
	Check(Fields(source,L"name|type|default|displayName|category|description|readOnly|choices"));
	property=__new Property; CheckAlloc(property);
	Check(ReadString(source,L"name",property->name)); Check(Identifier(property->name));
	CheckIf(target->Find(property->name.c_wstr())!=UINT_MAX,E_INVALIDARG);
	if(!event)
	{
		static PCWSTR const reserved[]={L"Name",L"Left",L"Top",L"Width",L"Height",L"Visible",L"Enabled",L"Align",L"Anchors",L"Margins",L"Padding",L"AutoSize",L"MinWidth",L"MinHeight",L"MaxWidth",L"MaxHeight",L"TabOrder",L"Font",L"ForeColor",L"BackColor",L"Caption",L"Text",L"BorderStyle",L"BorderIcons",L"WindowState",L"FormStyle",L"ModalResult",L"OnCreate",L"OnDestroy"};
		for(UINT n=0;n<ARRAYSIZE(reserved);n++) { CheckIf(!lstrcmpiW(property->name.c_wstr(),reserved[n]),E_INVALIDARG); }
	}
	Check(ReadString(source,L"type",type));
	UINT t; for(t=0;t<ARRAYSIZE(types);t++) if(type==RSTRING_CAST(types[t])) break;
	CheckIf(t==ARRAYSIZE(types) || (t==QuadooProperty::Event && !event),E_INVALIDARG); property->type=static_cast<QuadooProperty::Type>(t);
	Check(ReadString(source,L"displayName",property->display,property->name.c_wstr())); Check(ReadString(source,L"category",property->category,event ? L"Events" : L"Custom"));
	Check(ReadString(source,L"description",property->description,L"")); Check(ReadBool(source,L"readOnly",&readOnly)); if(readOnly) property->flags|=QuadooForm::PropertyReadOnly;
	if(source->HasField(RSTRING_CAST(L"choices")))
	{
		TStackRef<IJSONArray> choices; CheckIf(property->type!=QuadooProperty::Enum && property->type!=QuadooProperty::Flags,E_INVALIDARG);
		Check(source->FindNonNullValueW(L"choices",&value)); Check(value->GetArray(&choices)); value.Release(); CheckIf(choices->Count()>256,E_INVALIDARG);
		for(sysint n=0;n<choices->Count();n++)
		{
			TStackRef<IJSONObject> entry; TStackRef<IJSONValue> number;
			Choice* choice=__new Choice; CheckAlloc(choice);
			hr=choices->GetObject(n,&entry);
			if(SUCCEEDED(hr)) hr=Fields(entry,L"name|value");
			if(SUCCEEDED(hr)) hr=ReadString(entry,L"name",choice->name);
			INT integer;
			if(SUCCEEDED(hr)) hr=ReadInt(entry,L"value",&integer);
			if(SUCCEEDED(hr)) { choice->value=integer; if(!RStrLen(choice->name)) hr=E_INVALIDARG; }
			for(sysint p=0;SUCCEEDED(hr) && p<property->choices.Length();p++) if(choice->name==static_cast<RSTRING>(property->choices[p]->name) || choice->value==property->choices[p]->value) hr=E_INVALIDARG;
			if(SUCCEEDED(hr)) hr=property->choices.Append(choice);
			if(FAILED(hr)) { delete choice; Check(hr); }
		}
	}
	CheckIf(property->type==QuadooProperty::Enum && !property->choices.Length(),E_INVALIDARG);
	if(source->HasField(RSTRING_CAST(L"default"))) { Check(source->FindValueW(L"default",&value)); }
	else { Check(Default(property,&value)); }
	Check(Normalize(property,value,&property->initial)); Check(target->properties.Append(property)); property=NULL;
Cleanup: delete property; return hr;
}
static HRESULT AddEvent(CDefinition* target,IJSONObject* source)
{
	HRESULT hr; Event* event=NULL; Argument* argument=NULL; TStackRef<IJSONArray> array;
	TStackRef<IJSONValue> value; TStackRef<IJSONObject> property; CRString type;
	Check(Fields(source,L"name|category|flags|arguments")); event=__new Event; CheckAlloc(event);
	Check(ReadString(source,L"name",event->name)); Check(Identifier(event->name)); Check(ReadString(source,L"category",event->category,L"Events"));
	CheckIf(target->Find(event->name.c_wstr())!=UINT_MAX,E_INVALIDARG);
	if(source->HasField(RSTRING_CAST(L"flags")))
	{
		Check(source->FindNonNullValueW(L"flags",&value)); Check(value->GetArray(&array)); value.Release();
		for(sysint n=0;n<array->Count();n++) { CRString flag; Check(array->GetString(n,&flag)); DWORD bit=flag==RSTRING_CAST(L"EventCancellable") ? 1 : flag==RSTRING_CAST(L"EventHasResult") ? 2 : 0; CheckIf(!bit || event->flags&bit,E_INVALIDARG); event->flags|=bit; }
		array.Release();
	}
	if(source->HasField(RSTRING_CAST(L"arguments")))
	{
		Check(source->FindNonNullValueW(L"arguments",&value)); Check(value->GetArray(&array)); value.Release(); CheckIf(array->Count()>64,E_INVALIDARG);
		static PCWSTR const types[]={L"ValueEmpty",L"ValueInt",L"ValueLong",L"ValueBool",L"ValueString",L"ValueComponent"};
		for(sysint n=0;n<array->Count();n++)
		{
			TStackRef<IJSONObject> entry; CRString type;
			Check(array->GetObject(n,&entry)); Check(Fields(entry,L"name|type|writable")); argument=__new Argument; CheckAlloc(argument);
			Check(ReadString(entry,L"name",argument->name)); Check(Identifier(argument->name)); Check(ReadString(entry,L"type",type));
			UINT t; for(t=0;t<ARRAYSIZE(types);t++) if(type==RSTRING_CAST(types[t])) break;
			CheckIf(t==ARRAYSIZE(types),E_INVALIDARG); argument->type=static_cast<QuadooForm::EventValueType>(t);
			Check(ReadBool(entry,L"writable",&argument->writable));
			for(sysint p=0;p<event->arguments.Length();p++) { CheckIf(argument->name==static_cast<RSTRING>(event->arguments[p]->name),E_INVALIDARG); }
			Check(event->arguments.Append(argument)); argument=NULL;
		}
	}
	Check(JSONCreateObject(&property)); Check(QFD::PutString(property,L"name",event->name)); Check(QFD::PutString(property,L"type",RSTRING_CAST(L"Event")));
	Check(QFD::PutString(property,L"category",event->category)); Check(AddProperty(target,property,TRUE)); Check(target->events.Append(event)); event=NULL;
Cleanup: delete argument; delete event; return hr;
}
HRESULT CDefinition::Initialize(IJSONObject* object)
{
	HRESULT hr; TStackRef<IJSONObject> control; TStackRef<IJSONValue> value; TStackRef<IJSONArray> array; CRString kind,idText; INT version;
	Check(Fields(object,L"schemaVersion|packageId|name|description|control|icon")); Check(ReadInt(object,L"schemaVersion",&version)); CheckIf(version!=1,E_INVALIDARG);
	Check(ReadString(object,L"packageId",idText)); Check(CLSIDFromString(const_cast<PWSTR>(idText.c_wstr()),&id)); CheckIf(IsEqualGUID(id,GUID_NULL),E_INVALIDARG);
	Check(ReadString(object,L"name",name)); CheckIf(!RStrLen(name) || RStrLen(name)>256,E_INVALIDARG); Check(ReadString(object,L"description",description,L""));
	Check(object->FindNonNullValueW(L"control",&value)); Check(value->GetObject(&control)); value.Release();
	Check(Fields(control,L"type|kind|container|properties|events")); Check(ReadString(control,L"type",type)); Check(Identifier(type)); Check(ReadString(control,L"kind",kind));
	CheckIf(!(kind==RSTRING_CAST(L"visual")) && !(kind==RSTRING_CAST(L"nonvisual")),E_INVALIDARG); visual=kind==RSTRING_CAST(L"visual");
	Check(ReadBool(control,L"container",&container)); CheckIf(container && !visual,E_INVALIDARG); Check(AddBase(this));
	if(control->HasField(RSTRING_CAST(L"properties")))
	{
		Check(control->FindNonNullValueW(L"properties",&value)); Check(value->GetArray(&array)); value.Release(); CheckIf(array->Count()>256,E_INVALIDARG);
		for(sysint n=0;n<array->Count();n++) { TStackRef<IJSONObject> property; Check(array->GetObject(n,&property)); Check(AddProperty(this,property,FALSE)); }
		array.Release();
	}
	for(UINT e=0;e<2;e++) { TStackRef<IJSONObject> event; Check(JSONCreateObject(&event)); Check(QFD::PutString(event,L"name",RSTRING_CAST(e ? L"OnDestroy" : L"OnCreate"))); Check(AddEvent(this,event)); }
	if(control->HasField(RSTRING_CAST(L"events")))
	{
		Check(control->FindNonNullValueW(L"events",&value)); Check(value->GetArray(&array)); value.Release(); CheckIf(array->Count()>64,E_INVALIDARG);
		for(sysint n=0;n<array->Count();n++) { TStackRef<IJSONObject> event; Check(array->GetObject(n,&event)); Check(AddEvent(this,event)); }
	}
	if(object->HasField(RSTRING_CAST(L"icon")))
	{
		TStackRef<QFI::CImageResource> image; TStackRef<IJSONObject> encoded; CRString format,encoding,data,ascii;
		CMemoryStream bytes,pixels; UINT width,height;
		Check(object->FindNonNullValueW(L"icon",&icon)); Check(icon->GetObject(&encoded)); Check(Fields(encoded,L"format|encoding|data"));
		Check(ReadString(encoded,L"format",format)); Check(ReadString(encoded,L"encoding",encoding)); Check(ReadString(encoded,L"data",data));
		CheckIf(!(format==RSTRING_CAST(L"png")) || !(encoding==RSTRING_CAST(L"base64")) || RStrLen(data)>MaxIconBytes*4/3+4,E_INVALIDARG);
		Check(RStrConvertToA(RStrLen(data),data.c_wstr(),&ascii)); Check(ScDecodeBase64A(RStrToAnsi(static_cast<RSTRING>(ascii)),RStrLen(ascii),&bytes,NULL));
		CheckIf(bytes.DataRemaining()>MaxIconBytes || bytes.DataRemaining()<24,E_INVALIDARG);
		{
			const BYTE* header=bytes.GetReadPtr();
			static const BYTE signature[]={137,80,78,71,13,10,26,10};
			CheckIf(memcmp(header,signature,sizeof(signature)) || memcmp(header+12,"IHDR",4),E_INVALIDARG);
			UINT cx=(header[16]<<24)|(header[17]<<16)|(header[18]<<8)|header[19],cy=(header[20]<<24)|(header[21]<<16)|(header[22]<<8)|header[23];
			CheckIf(!cx || !cy || cx>256 || cy>256,E_INVALIDARG);
		}
		{ CMemoryStream encodedBytes; Check(ScEncodeBase64A(bytes.GetReadPtr(),bytes.DataRemaining(),&encodedBytes,NULL)); CheckIf(encodedBytes.DataRemaining()!=static_cast<UINT>(RStrLen(ascii)) || memcmp(encodedBytes.GetReadPtr(),RStrToAnsi(static_cast<RSTRING>(ascii)),encodedBytes.DataRemaining()),E_INVALIDARG); }
		Check(QFI::DecodePNG(bytes.GetReadPtr(),bytes.DataRemaining(),pixels,&width,&height)); CheckIf(width>256 || height>256,E_INVALIDARG);
		Check(QFI::CImageResource::Create(icon,&image));
	}
	Check(JSONCloneObject(object,&value,TRUE)); Check(value->GetObject(&json));
Cleanup: return hr;
}
HRESULT CDefinition::GetIcon(UINT cx,UINT cy,HICON* result)
{
	HRESULT hr=S_OK; TStackRef<QFI::CImageResource> image;
	HDC dc=NULL,memory=NULL; HBITMAP bitmap=NULL,mask=NULL; HGDIOBJ previous=NULL; BYTE* pixels=NULL;
	RECT bounds;
	CheckIf(!result,E_POINTER); *result=NULL; CheckIf(!cx || !cy || cx>256 || cy>256,E_INVALIDARG); CheckIfIgnore(!icon,S_FALSE);
	Check(QFI::CImageResource::Create(icon,&image));
	BITMAPINFO info; ZeroMemory(&info,sizeof(info)); info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=cx; info.bmiHeader.biHeight=-static_cast<LONG>(cy); info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
	dc=GetDC(NULL); CheckIf(!dc,HrEnsureLastError()); memory=CreateCompatibleDC(dc); CheckIf(!memory,HrEnsureLastError());
	bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<PVOID*>(&pixels),NULL,0); CheckIf(!bitmap,HrEnsureLastError());
	ZeroMemory(pixels,cx*cy*4); previous=SelectObject(memory,bitmap);
	SetRect(&bounds,0,0,cx,cy);
	Check(image->Paint(memory,&bounds,96,QFI::Contain,4,QFI::HighQuality,255));
	// GDI AlphaBlend into a transparent DIB retains the premultiplied alpha.
	mask=CreateBitmap(cx,cy,1,1,NULL); CheckIf(!mask,HrEnsureLastError());
	{ HDC mono=CreateCompatibleDC(dc); CheckIf(!mono,HrEnsureLastError()); HGDIOBJ old=SelectObject(mono,mask); PatBlt(mono,0,0,cx,cy,BLACKNESS); SelectObject(mono,old); DeleteDC(mono); }
	{ ICONINFO iconInfo={TRUE,0,0,mask,bitmap}; *result=CreateIconIndirect(&iconInfo); CheckIf(!*result,HrEnsureLastError()); }
Cleanup:
	if(previous) SelectObject(memory,previous); if(bitmap) DeleteObject(bitmap); if(mask) DeleteObject(mask); if(memory) DeleteDC(memory); if(dc) ReleaseDC(NULL,dc);
	return hr;
}
HRESULT LoadPackage(PCWSTR path,BOOL sidecar,CDefinition** definition)
{
	HRESULT hr; TStackRef<IJSONObject> object; TStackRef<CDefinition> candidate;
	CheckIf(!definition,E_POINTER); *definition=NULL; Check(LoadJSON(path,&object));
	if(sidecar && !object->HasField(RSTRING_CAST(L"icon")))
	{
		WCHAR imagePath[MAX_PATH]; Check(TStrCchCpy(imagePath,ARRAYSIZE(imagePath),path)); PWSTR dot=const_cast<PWSTR>(TStrRChr(imagePath,L'.')); CheckIf(!dot,E_INVALIDARG); Check(TStrCchCpy(dot,static_cast<INT>(ARRAYSIZE(imagePath)-(dot-imagePath)),L".png"));
		DWORD attributes=GetFileAttributesW(imagePath);
		if(attributes!=INVALID_FILE_ATTRIBUTES)
		{
			HANDLE file=CreateFileW(imagePath,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); CheckIf(file==INVALID_HANDLE_VALUE,HrEnsureLastError());
			CMemoryStream bytes; DWORD high=0,size=GetFileSize(file,&high),read=0; BYTE* data=NULL;
			hr=(high || size>MaxIconBytes) ? E_INVALIDARG : bytes.TWriteAdvance<BYTE>(&data,size);
			if(SUCCEEDED(hr) && (!ReadFile(file,data,size,&read,NULL) || read!=size)) hr=HrEnsureLastError(); CloseHandle(file); Check(hr);
			TStackRef<IJSONValue> icon; Check(QFI::CreateValue(L"png",data,size,&icon)); Check(object->AddValueW(L"icon",icon));
		}
		else { DWORD error=GetLastError(); CheckIf(error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND,HRESULT_FROM_WIN32(error)); }
	}
	candidate.Attach(__new CDefinition); CheckAlloc(candidate); Check(candidate->Initialize(object)); *definition=candidate.Detach();
Cleanup: return hr;
}
}
