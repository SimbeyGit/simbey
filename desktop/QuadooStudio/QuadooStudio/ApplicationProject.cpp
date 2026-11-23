#include <windows.h>
#include <shlwapi.h>
#include "Library\ChooseFile.h"
#include "Library\Util\TextHelpers.h"
#include "QuadooProject.h"
#include "ApplicationProject.h"
#include "ApplicationSettingsDlg.h"
#include "FormSourceSync.h"

static HRESULT SaveNewText(PCWSTR path, PCWSTR text, INT length)
{
	HRESULT hr;
	HANDLE file = CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
	Check(Text::SaveToHandle(text,length,file));
Cleanup:
	if(file != INVALID_HANDLE_VALUE) { CloseHandle(file); if(FAILED(hr)) DeleteFileW(path); }
	return hr;
}
HRESULT SerializeStudioProject(IJSONObject* object, CMemoryStream& text)
{
	HRESULT hr;
	TStackRef<IJSONValue> target;
	TStackRef<IJSONValue> legacy;
	TStackRef<IJSONValue> copy;
	TStackRef<IJSONValue> settings;
	TStackRef<IJSONArray> x86;
	TStackRef<IJSONObject> migrated;
	CRString kind;
	if(object->HasField(RSTRING_CAST(L"modules")) && SUCCEEDED(object->FindNonNullValueW(L"target",&target)) &&
		SUCCEEDED(target->GetString(&kind)) && kind == RSTRING_CAST(L"application"))
	{
		Check(object->FindValueW(L"modules",&legacy));
		if(legacy && legacy->GetType() == JSON::Array)
		{
			Check(legacy->GetArray(&x86));
			Check(StudioApplication::WrapModuleSettings(x86,NULL,&settings));
			// Migrate the saved snapshot so failed writes leave live settings alone.
			Check(JSONCloneObject(object,&copy,FALSE));
			Check(copy->GetObject(&migrated));
			Check(migrated->AddValueW(L"modules",settings));
			object = migrated;
		}
	}
	Check(JSONSerializeObject(object,&text));
Cleanup:
	return hr;
}

HRESULT SaveStudioProject(IJSONObject* object, PCWSTR path)
{
	HRESULT hr;
	CMemoryStream text;
	WCHAR directory[MAX_PATH], temporary[MAX_PATH] = {0};
	Check(SerializeStudioProject(object,text));
	Check(TStrCchCpy(directory,ARRAYSIZE(directory),path));
	PWSTR leaf = const_cast<PWSTR>(TStrRChr(directory,L'\\'));
	CheckIf(!leaf,E_INVALIDARG);
	*leaf = 0;
	CheckIf(!GetTempFileNameW(directory,L"QSP",0,temporary),HrEnsureLastError());
	Check(Text::SaveToFile(text.TGetReadPtr<WCHAR>(),text.TDataRemaining<WCHAR>(),temporary));
	CheckIf(!MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),HrEnsureLastError());
	temporary[0] = 0;
Cleanup:
	if(temporary[0]) DeleteFileW(temporary);
	return hr;
}
static HRESULT UnitMetadata(RSTRING source, RSTRING definition, BOOL form, IJSONObject** result)
{
	HRESULT hr;
	TStackRef<IJSONObject> data;
	CRString name, interfaceName, identity;
	GUID id; WCHAR text[40];
	Check(CStudioFormUnit::SourceName(source,&name));
	Check(RStrFormatW(&interfaceName,L"I%ls",name.c_wstr()));
	Check(CoCreateGuid(&id));
	CheckIf(!StringFromGUID2(id,text,ARRAYSIZE(text)),E_FAIL);
	Check(RStrCreateW(TStrLenAssert(text),text,&identity));
	Check(JSONCreateObject(&data));
	Check(QFD::PutString(data,L"unitId",identity));
	Check(QFD::PutString(data,L"kind",RSTRING_CAST(form ? L"form" : L"frame")));
	Check(QFD::PutString(data,L"definition",definition));
	Check(QFD::PutString(data,L"class",name));
	Check(QFD::PutString(data,L"eventsInterface",interfaceName));
	*result = data.Detach();
Cleanup:
	return hr;
}

static HRESULT CreateUnitFiles(PCWSTR source, BOOL form, IJSONObject** metadata, CRString& definition)
{
	HRESULT hr;
	CRString path, name, body, idText, stem;
	TStackRef<IJSONObject> root, properties;
	TStackRef<IJSONValue> value;
	CMemoryStream json;
	TStackRef<IJSONObject> unit;
	StudioSource::Result generated;
	GUID id; WCHAR text[40];
	BOOL madeSource = FALSE, madeDefinition = FALSE;
	Check(RStrCreateW(TStrLenAssert(source),source,&path));
	Check(CStudioFormUnit::SourceName(path,&name));
	PCWSTR extension = TStrRChr(source,L'.');
	CheckIf(!extension || lstrcmpiW(extension,L".quadoo"),E_INVALIDARG);
	Check(RStrCreateW(static_cast<INT>(extension - source),source,&stem));
	Check(RStrFormatW(&definition,L"%ls.%ls.json",stem.c_wstr(),form ? L"qform" : L"qframe"));
	CheckIf(GetFileAttributesW(definition.c_wstr()) != INVALID_FILE_ATTRIBUTES,HRESULT_FROM_WIN32(ERROR_FILE_EXISTS));
	Check(RStrFormatW(&body,L"#pragma once\r\n// Quadoo Studio %ls unit.\r\n\r\ninterface I%ls\r\n{\r\n};\r\n\r\nclass %ls : extern(%ls), I%ls\r\n{\r\n};\r\n",form ? L"Form" : L"Frame",name.c_wstr(),name.c_wstr(),form ? L"Form" : L"Frame",name.c_wstr()));
	Check(JSONCreateObject(&root));
	Check(JSONCreateObject(&properties));
	Check(QFD::PutInt(root,L"schemaVersion",1));
	Check(QFD::PutString(root,L"type",RSTRING_CAST(form ? L"Form" : L"Frame")));
	Check(QFD::PutString(root,L"name",name));
	Check(CoCreateGuid(&id));
	CheckIf(!StringFromGUID2(id,text,ARRAYSIZE(text)),E_FAIL);
	Check(RStrCreateW(TStrLenAssert(text),text,&idText));
	Check(QFD::PutString(root,L"id",idText));
	Check(QFD::PutInt(properties,L"Width",form ? 640 : 400));
	Check(QFD::PutInt(properties,L"Height",form ? 400 : 240));
	if(form)
	{
		Check(QFD::PutString(properties,L"Caption",name));
	}
	Check(JSONWrapObject(properties,&value));
	Check(root->AddValueW(L"properties",value));
	Check(JSONSerializeObject(root,&json));
	Check(UnitMetadata(path,definition,form,&unit));
	value.Release();
	Check(unit->FindNonNullValueW(L"unitId",&value));
	Check(value->GetStringAsGuid(id));
	Check(StudioSource::Prepare(body,name,id,unit,root,NULL,generated));
	Check(SaveNewText(source,generated.text.c_wstr(),RStrLen(generated.text)));
	madeSource = TRUE;
	Check(SaveNewText(definition.c_wstr(),json.TGetReadPtr<WCHAR>(),json.TDataRemaining<WCHAR>()));
	madeDefinition = TRUE;
	*metadata = generated.metadata.Detach();
Cleanup:
	if(FAILED(hr))
	{
		if(madeSource)
			DeleteFileW(source);
		if(madeDefinition)
			DeleteFileW(definition.c_wstr());
	}
	return hr;
}

HRESULT CreateApplicationProject(IJSONObject* project, PCWSTR path)
{
	HRESULT hr;
	TStackRef<IJSONObject> application;
	TStackRef<IJSONArray> files;
	TStackRef<IJSONValue> value;
	Check(JSONCreateArray(&files));
	Check(JSONWrapArray(files,&value));
	Check(project->AddValueW(L"files",value));
	value.Release();
	Check(JSONCreateObject(&application));
	Check(QFD::PutInt(application,L"schemaVersion",1));
	Check(JSONWrapObject(application,&value));
	Check(project->AddValueW(L"application",value));
	Check(SaveStudioProject(project,path));
Cleanup:
	return hr;
}
HRESULT CQuadooProject::NewFormUnit(BOOL form, PCWSTR sourcePath)
{
	HRESULT hr;
	CChooseFile chooser;
	WCHAR relative[MAX_PATH];
	WCHAR definitionRelative[MAX_PATH];
	CRString definition;
	CRString path;
	TStackRef<IJSONObject> metadata;
	CProjectTab* tab = NULL;
	BOOL created = FALSE;
	BOOL committed = FALSE;
	CheckIf(!IsApplication(),E_ACCESSDENIED);
	if(ActiveDesigner())
	{
		Check(ActiveDesigner()->CommitPropertyEdit());
	}
	if(!sourcePath)
	{
		Check(chooser.Initialize());
		Check(chooser.SetInitialDirectory(RStrToWide(m_rstrProjectDir)));
		CheckIfIgnore(!chooser.SaveFile(m_hwnd,form ? L"Add Form Source" : L"New Frame Source",form ? L"Form1.quadoo" : L"Frame1.quadoo",L"QuadooScript (*.quadoo)\0*.quadoo\0\0"),OLECMDERR_E_CANCELED);
		sourcePath = chooser.GetFile(0);
	}
	Check(CreateRelativeProjectPath(relative,ARRAYSIZE(relative),sourcePath,0));
	Check(RStrCreateW(TStrLenAssert(relative),relative,&path));
	CheckIf(m_mapFiles.HasItem(path),HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
	Check(CreateUnitFiles(sourcePath,form,&metadata,definition));
	created = TRUE;
	Check(CreateRelativeProjectPath(definitionRelative,ARRAYSIZE(definitionRelative),definition.c_wstr(),0));
	definition = static_cast<RSTRING>(NULL);
	Check(RStrCreateW(TStrLenAssert(definitionRelative),definitionRelative,&definition));
	Check(QFD::PutString(metadata,L"definition",definition));
	Check(AddFile(path,&tab));
	Check(InitializeFormUnit(static_cast<CProjectFile*>(tab),metadata));
	if(form && !FindDefaultScript())
	{
		tab->m_fDefault = true;
	}
	Check(UpdateFiles());
	Check(Save());
	committed = TRUE;
	static_cast<CProjectFile*>(tab)->m_form->design = TRUE;
	Check(SwitchToFile(tab));
Cleanup:
	if(FAILED(hr) && !committed)
	{
		if(tab)
		{
			ClearTreeNodes(tab);
			m_mapFiles.Remove(path,NULL);
			__delete tab;
			UpdateFiles();
		}
		if(created)
		{
			DeleteFileW(sourcePath);
			// The absolute sibling is derived from the explicitly chosen source.
			CRString absolute;
			CRString stem;
			PCWSTR extension = TStrRChr(sourcePath,L'.');
			if(extension && SUCCEEDED(RStrCreateW(static_cast<INT>(extension - sourcePath),sourcePath,&stem)) && SUCCEEDED(RStrFormatW(&absolute,L"%ls.%ls.json",stem.c_wstr(),form ? L"qform" : L"qframe")))
			{
				DeleteFileW(absolute.c_wstr());
			}
		}
	}
	return hr;
}
HRESULT CQuadooProject::AddFormUnit(BOOL)
{
	HRESULT hr;
	CChooseFile definitionPicker, sourcePicker;
	WCHAR relative[MAX_PATH], relativeDefinition[MAX_PATH];
	CRString path, definitionPath, rootType;
	PWSTR text = NULL; INT length = 0;
	TStackRef<IJSONObject> definition, metadata;
	TStackRef<IJSONValue> value;
	CProjectTab* tab = NULL;
	BOOL added = FALSE, committed = FALSE, attached = FALSE;
	CheckIf(!IsApplication(),E_ACCESSDENIED);
	Check(definitionPicker.Initialize());
	Check(definitionPicker.SetInitialDirectory(RStrToWide(m_rstrProjectDir)));
	CheckIfIgnore(!definitionPicker.OpenSingleFile(m_hwnd,L"Add Form or Frame Definition",L"Form/Frame JSON\0*.qform.json;*.qframe.json;*.json\0\0"),OLECMDERR_E_CANCELED);
	Check(Text::LoadFromFile(definitionPicker.GetFile(0),&text,&length,CP_UTF8));
	Check(QFD::Parse(text,length,&definition));
	Check(definition->FindNonNullValueW(L"type",&value));
	Check(value->GetString(&rootType));
	CheckIf(!(rootType == RSTRING_CAST(L"Form")) && !(rootType == RSTRING_CAST(L"Frame")),E_INVALIDARG);
	Check(sourcePicker.Initialize());
	Check(sourcePicker.SetInitialDirectory(RStrToWide(m_rstrProjectDir)));
	CheckIfIgnore(!sourcePicker.OpenSingleFile(m_hwnd,L"Choose Associated QuadooScript Source",L"QuadooScript (*.quadoo)\0*.quadoo\0\0"),OLECMDERR_E_CANCELED);
	Check(CreateRelativeProjectPath(relative,ARRAYSIZE(relative),sourcePicker.GetFile(0),0));
	Check(CreateRelativeProjectPath(relativeDefinition,ARRAYSIZE(relativeDefinition),definitionPicker.GetFile(0),0));
	Check(RStrCreateW(TStrLenAssert(relative),relative,&path));
	Check(RStrCreateW(TStrLenAssert(relativeDefinition),relativeDefinition,&definitionPath));
	Check(UnitMetadata(path,definitionPath,rootType == RSTRING_CAST(L"Form"),&metadata));
	added = !m_mapFiles.HasItem(path);
	Check(AddFile(path,&tab));
	CheckIf(static_cast<CProjectFile*>(tab)->m_form,HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS));
	Check(InitializeFormUnit(static_cast<CProjectFile*>(tab),metadata));
	attached = TRUE;
	Check(UpdateFiles());
	Check(Save());
	committed = TRUE;
	static_cast<CProjectFile*>(tab)->m_form->design = TRUE;
	Check(SwitchToFile(tab));
Cleanup:
	if(FAILED(hr) && !committed && tab)
	{
		if(added) { ClearTreeNodes(tab); m_mapFiles.Remove(path,NULL); __delete tab; }
		else if(attached) SafeRelease(static_cast<CProjectFile*>(tab)->m_form);
		UpdateFiles();
	}
	SafeDeleteArray(text);
	return hr;
}
