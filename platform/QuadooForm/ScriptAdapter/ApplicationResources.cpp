#include "ApplicationResources.h"
#include "Library\Core\StringCore.h"
#include "Library\Util\TextHelpers.h"

HRESULT AQVM::StringField(IJSONObject* object, PCWSTR name, RSTRING* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> field;
	Check(object->FindNonNullValueW(name,&field));
	Check(field->GetString(value));
Cleanup:
	return hr;
}

HRESULT AQVM::ClassResourceName(RSTRING className, RSTRING* name)
{
	HRESULT hr;
	PWSTR buffer;
	INT length = RStrLen(className);
	Check(RStrAllocW(length,name,&buffer));
	CopyMemory(buffer,RStrToWide(className),length * sizeof(WCHAR));
	CharUpperBuffW(buffer,length);
Cleanup:
	return hr;
}

HRESULT AQVM::PutString(IJSONObject* object, PCWSTR name, RSTRING value)
{
	HRESULT hr;
	TStackRef<IJSONValue> field;
	Check(JSONCreateString(value,&field));
	Check(object->AddValueW(name,field));
Cleanup:
	return hr;
}

HRESULT AQVM::PutInt(IJSONObject* object, PCWSTR name, INT value)
{
	HRESULT hr;
	TStackRef<IJSONValue> field;
	Check(JSONCreateInteger(value,&field));
	Check(object->AddValueW(name,field));
Cleanup:
	return hr;
}

HRESULT AQVM::ParseObject(PCWSTR text, INT length, IJSONObject** object)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	CheckIf(!object,E_POINTER);
	*object = NULL;
	Check(JSONParse(NULL,text,length,&value));
	Check(value->GetObject(object));
Cleanup:
	return hr;
}

HRESULT AQVM::ResourceObject(HINSTANCE module, PCWSTR type, PCWSTR name, IJSONObject** object)
{
	HRESULT hr;
	HRSRC resource;
	HGLOBAL data;
	DWORD bytes;
	const BYTE* raw;
	PWSTR converted = NULL;
	INT length;
	CheckIf(!object,E_POINTER);
	*object = NULL;
	resource = FindResourceW(module,name,type);
	CheckIf(!resource,HrEnsureLastError());
	bytes = SizeofResource(module,resource);
	CheckIf(!bytes || bytes > MaximumResourceBytes,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
	data = LoadResource(module,resource);
	CheckIf(!data,HrEnsureLastError());
	raw = static_cast<const BYTE*>(LockResource(data));
	CheckIf(!raw,E_FAIL);
	if(bytes >= 2 && !raw[1])
	{
		CheckIf(bytes % sizeof(WCHAR),HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
		Check(ParseObject(reinterpret_cast<PCWSTR>(raw),bytes / sizeof(WCHAR),object));
	}
	else
	{
		Check(Text::AllocateUnicodeFromCodePage(raw,bytes,&converted,&length,CP_UTF8));
		Check(ParseObject(converted,length,object));
	}
Cleanup:
	SafeDeleteArray(converted);
	return hr;
}

HRESULT AQVM::CopyObject(IJSONObject* source, IJSONObject** object)
{
	HRESULT hr;
	CMemoryStream stream;
	Check(JSONSerializeObject(source,&stream));
	Check(ParseObject(stream.TGetReadPtr<WCHAR>(),static_cast<INT>(stream.TDataRemaining<WCHAR>()),object));
Cleanup:
	return hr;
}

HRESULT AQVM::FindUnit(IJSONObject* catalog, RSTRING className, IJSONObject** unit)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONArray> units;
	CheckIf(!unit,E_POINTER);
	*unit = NULL;
	Check(catalog->FindNonNullValueW(L"units",&value));
	Check(value->GetArray(&units));
	for(sysint n = 0; n < units->Count(); n++)
	{
		TStackRef<IJSONObject> candidate;
		CRString name;
		Check(units->GetObject(n,&candidate));
		Check(StringField(candidate,L"class",&name));
		if(name == className)
		{
			*unit = candidate.Detach();
			break;
		}
	}
	CheckIf(!*unit,DISP_E_UNKNOWNNAME);
Cleanup:
	return hr;
}

HRESULT AQVM::GenerateStartup(RSTRING startupClass, RSTRING* source)
{
	return RStrFormatW(source,
		L"\r\ninterface I__QuadooApplicationLifetime\r\n{\r\n"
		L"\tvirtual function CollectReleasedObjects() = 0;\r\n};\r\n"
		L"class __QuadooApplicationLifetime : I__QuadooApplicationLifetime\r\n{\r\n"
		L"\tvirtual function CollectReleasedObjects()\r\n\t{\r\n\t\treturn gc();\r\n\t}\r\n};\r\n"
		L"function __QuadooApplicationStartup (nCmdShow)\r\n{\r\n"
		L"\tHost.SetApplicationLifetime(new __QuadooApplicationLifetime);\r\n"
		L"\tHost.ShowForm(new %r, nCmdShow);\r\n}\r\n"
		L"function WinMain (strCmdLine, nCmdShow)\r\n{\r\n"
		L"\t__QuadooApplicationStartup(nCmdShow);\r\n\treturn Host.RunApplication();\r\n}\r\n",
		startupClass);
}

HRESULT AQVM::ValidateCatalog(IJSONObject* catalog)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONArray> units;
	TStackRef<IJSONObject> startupUnit;
	CRString startupClass;
	INT version;
	CheckIf(!catalog,E_INVALIDARG);
	Check(catalog->FindNonNullValueW(L"schemaVersion",&value));
	Check(value->GetInteger(&version));
	CheckIf(version != CatalogVersion,HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
	value.Release();
	Check(catalog->FindNonNullValueW(L"units",&value));
	Check(value->GetArray(&units));
	CheckIf(units->Count() < 1 || units->Count() > MaximumUnits,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
	for(sysint n = 0; n < units->Count(); n++)
	{
		TStackRef<IJSONObject> unit;
		CRString name, kind, events, id;
		GUID guid;
		Check(units->GetObject(n,&unit));
		Check(StringField(unit,L"class",&name));
		Check(StringField(unit,L"kind",&kind));
		Check(StringField(unit,L"eventsInterface",&events));
		Check(StringField(unit,L"unitId",&id));
		CheckIf(!RStrLen(name) || !RStrLen(events) || (!(kind == RSTRING_CAST(L"form")) && !(kind == RSTRING_CAST(L"frame"))),HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
		Check(CLSIDFromString(const_cast<PWSTR>(id.c_wstr()),&guid));
		CheckIf(IsEqualGUID(guid,GUID_NULL),HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
		for(sysint p = 0; p < n; p++)
		{
			TStackRef<IJSONObject> previous;
			CRString other, otherID;
			GUID previousID;
			Check(units->GetObject(p,&previous));
			Check(StringField(previous,L"class",&other));
			Check(StringField(previous,L"unitId",&otherID));
			Check(CLSIDFromString(const_cast<PWSTR>(otherID.c_wstr()),&previousID));
			CheckIf(!lstrcmpiW(name.c_wstr(),other.c_wstr()) || IsEqualGUID(guid,previousID),HRESULT_FROM_WIN32(ERROR_DUP_NAME));
		}
	}
	Check(StringField(catalog,L"startupClass",&startupClass));
	Check(FindUnit(catalog,startupClass,&startupUnit));
	{
		CRString kind;
		Check(StringField(startupUnit,L"kind",&kind));
		CheckIf(!(kind == RSTRING_CAST(L"form")),HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
	}
Cleanup:
	return hr;
}
