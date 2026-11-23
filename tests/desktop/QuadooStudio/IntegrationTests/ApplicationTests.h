#include "..\..\..\..\desktop\QuadooStudio\QuadooStudio\ApplicationTarget.h"

static const WCHAR s_helperSource[] = L"#pragma once\r\nfunction ExtraProjectCode()\r\n{\r\n\treturn 123;\r\n}\r\n";

static HRESULT FileFingerprint(PCWSTR path, ULONGLONG* fingerprint)
{
	HRESULT hr = S_OK;
	HANDLE file = INVALID_HANDLE_VALUE;
	BYTE data[4096];
	DWORD read;
	ULONGLONG hash = 14695981039346656037ui64;
	file = CreateFileW(path,GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
	CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
	do
	{
		CheckIf(!ReadFile(file,data,sizeof(data),&read,NULL),HrEnsureLastError());
		for(DWORD n = 0; n < read; n++)
		{
			hash = (hash ^ data[n]) * 1099511628211ui64;
		}
	} while(read);
	*fingerprint = hash;
Cleanup:
	SafeCloseFileHandle(file);
	return hr;
}

static HRESULT RunBuiltApplication(RSTRING executable, PCWSTR workingDirectory)
{
	HRESULT hr;
	STARTUPINFOW startup = {sizeof(startup)};
	PROCESS_INFORMATION process = {0};
	CRString command;
	PWSTR writable = NULL;
	DWORD code;
	Check(RStrFormatW(&command,L"\"%ls\" test-argument",RStrToWide(executable)));
	Check(TDuplicateStringAssert(command.c_wstr(),&writable));
	startup.dwFlags = STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_SHOWNOACTIVATE;
	CheckIf(!CreateProcessW(RStrToWide(executable),writable,NULL,NULL,FALSE,0,NULL,workingDirectory,&startup,&process),HrEnsureLastError());
	CheckIf(WaitForSingleObject(process.hProcess,15000) != WAIT_OBJECT_0,HRESULT_FROM_WIN32(ERROR_TIMEOUT));
	CheckIf(!GetExitCodeProcess(process.hProcess,&code),HrEnsureLastError());
	REQUIRE(code == 0);
Cleanup:
	if(process.hProcess)
	{
		if(WaitForSingleObject(process.hProcess,0) == WAIT_TIMEOUT)
		{
			// Only this test-owned child is terminated on a failing timeout.
			TerminateProcess(process.hProcess,1);
			WaitForSingleObject(process.hProcess,2000);
		}
		CloseHandle(process.hProcess);
	}
	if(process.hThread)
	{
		CloseHandle(process.hThread);
	}
	SafeDeleteArray(writable);
	return hr;
}

static HRESULT RejectApplicationBuild(IJSONObject* project, RSTRING projectPath, RSTRING runtimeDirectory, RSTRING previous, PCWSTR missingPath = NULL)
{
	HRESULT hr;
	HRESULT result;
	CRString ignored;
	CRString error;
	ULONGLONG before;
	ULONGLONG after;
	Check(FileFingerprint(RStrToWide(previous),&before));
	result = StudioApplication::Build(project,projectPath,runtimeDirectory,ignored,error);
	REQUIRE(FAILED(result));
	REQUIRE(!RStrLen(ignored) && RStrLen(error));
	if(missingPath)
	{
		REQUIRE(result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
		REQUIRE(TStrStr(error.c_wstr(),missingPath) != NULL);
		REQUIRE(TStrStr(error.c_wstr(),L"QuadooStudio.exe") != NULL);
	}
	Check(FileFingerprint(RStrToWide(previous),&after));
	REQUIRE(before == after);
Cleanup:
	return hr;
}

static HRESULT AddApplicationPlugins(RSTRING directory, IJSONArray* components)
{
	HRESULT hr;
	CRString modules;
	CMemoryStream byteCode;
	CSourceCompilerStatus compiler;
	ULONG written;
	HANDLE file = INVALID_HANDLE_VALUE;
	PCWSTR const names[] = {L"DirectPlugin",L"FallbackPlugin"};
	PCWSTR const paths[] = {L"DirectPlugin.quadoo",L"FallbackPlugin.qbc"};
	static const WCHAR source[] = L"class Plugin { function Ping() { return 73; } };\nfunction CreateObject(options) { return new Plugin; }\n";
	Check(RStrFormatW(&modules,L"%r\\Modules",directory));
	CheckIf(!CreateDirectoryW(modules.c_wstr(),NULL),HrEnsureLastError());
	Check(QuadooParseTextToStream(L"Plugin",6,source,ARRAYSIZE(source) - 1,0,&byteCode,NULL,&compiler));
	for(UINT n = 0; n < ARRAYSIZE(names); n++)
	{
		CRString path;
		CRString identity;
		GUID id;
		WCHAR text[40];
		TStackRef<IJSONObject> plugin;
		TStackRef<IJSONObject> properties;
		TStackRef<IJSONValue> value;
		Check(RStrFormatW(&path,L"%r\\%ls",n ? static_cast<RSTRING>(modules) : directory,paths[n]));
		if(n)
		{
			file = CreateFileW(path.c_wstr(),GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
			CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
			CheckIf(!WriteFile(file,byteCode.GetReadPtr(),byteCode.DataRemaining(),&written,NULL),HrEnsureLastError());
			CheckIf(written != byteCode.DataRemaining(),STG_E_WRITEFAULT);
			SafeCloseFileHandle(file);
		}
		else
		{
			Check(Text::SaveToFile(source,ARRAYSIZE(source) - 1,path.c_wstr()));
		}
		Check(JSONCreateObject(&plugin));
		Check(JSONCreateObject(&properties));
		Check(QFD::PutString(plugin,L"type",RSTRING_CAST(L"QuadooPlugin")));
		Check(QFD::PutString(plugin,L"name",RSTRING_CAST(names[n])));
		Check(CoCreateGuid(&id));
		StringFromGUID2(id,text,ARRAYSIZE(text));
		Check(RStrCreateW(TStrLenAssert(text),text,&identity));
		Check(QFD::PutString(plugin,L"id",identity));
		Check(QFD::PutString(properties,L"Module",RSTRING_CAST(paths[n])));
		Check(JSONWrapObject(properties,&value));
		Check(plugin->AddValueW(L"properties",value));
		value.Release();
		Check(JSONWrapObject(plugin,&value));
		Check(components->Add(value));
	}
Cleanup:
	SafeCloseFileHandle(file);
	return hr;
}

#include "ApplicationDebugTests.h"

static HRESULT ApplicationBuildAndRun(PCWSTR scratch, BOOL plugins = FALSE, BOOL debugging = FALSE)
{
	HRESULT hr;
	TStackRef<IJSONObject> project, entry, metadata, definition, events, timer, timerEvents;
	TStackRef<IJSONArray> files, components;
	TStackRef<IJSONValue> value;
	CRString folder, projectPath, sourcePath, definitionPath, helperPath, framePath, frameDefinitionPath;
	CRString source, definitionText, identity, executable, diagnostic, runtimeDirectory, startupDirectory;
	CRString hiddenSource, hiddenDefinition, hiddenHelper, hiddenFrame, hiddenFrameDefinition;
	CRString directModule;
	CRString fallbackModule;
	CRString hiddenDirect;
	CRString hiddenFallback;
	BOOL movedDirect = FALSE;
	BOOL movedFallback = FALSE;
	StudioSource::Result generated;
	GUID unitID;
	WCHAR runtime[MAX_PATH], working[MAX_PATH];
	ULONGLONG before, after;
	HANDLE lock = INVALID_HANDLE_VALUE;
	BOOL movedSource = FALSE, movedDefinition = FALSE, movedHelper = FALSE, movedFrame = FALSE, movedFrameDefinition = FALSE;
	Check(RStrFormatW(&folder,L"%ls\\Application %ls",scratch,plugins ? L"Plugins" : L"Run"));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&projectPath,L"%ls\\RuntimeExample.qsproj",folder.c_wstr()));
	Check(RStrFormatW(&sourcePath,L"%ls\\MainForm.quadoo",folder.c_wstr()));
	Check(RStrFormatW(&definitionPath,L"%ls\\MainForm.qform.json",folder.c_wstr()));
	Check(RStrFormatW(&helperPath,L"%ls\\Helper.quadoo",folder.c_wstr()));
	Check(RStrFormatW(&framePath,L"%ls\\DetailsFrame.quadoo",folder.c_wstr()));
	Check(RStrFormatW(&frameDefinitionPath,L"%ls\\DetailsFrame.qframe.json",folder.c_wstr()));
	Check(JSONCreateObject(&project));
	Check(QFD::PutString(project,L"target",RSTRING_CAST(L"application")));
	Check(QFD::PutString(project,L"args",RSTRING_CAST(L"")));
	Check(QFD::PutString(project,L"startDir",RSTRING_CAST(L".\\")));
	Check(CreateApplicationFixture(project,projectPath.c_wstr()));
	Check(project->FindNonNullValueW(L"files",&value));
	Check(value->GetArray(&files));
	value.Release();
	Check(files->GetObject(0,&entry));
	Check(entry->FindNonNullValueW(L"designer",&value));
	Check(value->GetObject(&metadata));
	value.Release();
	Check(ReadText(sourcePath.c_wstr(),source));
	Check(ReadText(definitionPath.c_wstr(),definitionText));
	Check(QFD::Parse(definitionText.c_wstr(),RStrLen(definitionText),&definition));
	Check(JSONCreateArray(&components));
	Check(JSONCreateObject(&timer));
	Check(QFD::PutString(timer,L"type",RSTRING_CAST(L"Timer")));
	Check(QFD::PutString(timer,L"name",RSTRING_CAST(L"Timer1")));
	Check(QFD::PutString(timer,L"id",RSTRING_CAST(L"{593CE015-B015-40D9-8855-600000000001}")));
	Check(JSONCreateObject(&timerEvents));
	Check(QFD::PutString(timerEvents,L"OnTimer",RSTRING_CAST(L"Tick")));
	Check(JSONWrapObject(timerEvents,&value));
	Check(timer->AddValueW(L"events",value));
	value.Release();
	Check(JSONWrapObject(timer,&value));
	Check(components->Add(value));
	value.Release();
	if(plugins)
	{
		Check(AddApplicationPlugins(folder,components));
		TStackRef<IJSONArray> dependencies;
		TStackRef<IJSONArray> otherDependencies;
		CRString dependencyPath;
		Check(JSONCreateArray(&dependencies));
		Check(JSONCreateString(RSTRING_CAST(L".\\DirectPlugin.quadoo"),&value));
		Check(dependencies->Add(value));
		value.Release();
		Check(RStrFormatW(&dependencyPath,L"%r\\Modules\\FallbackPlugin.qbc",static_cast<RSTRING>(folder)));
		Check(JSONCreateString(dependencyPath,&value));
		Check(dependencies->Add(value));
		value.Release();
		Check(JSONCreateArray(&otherDependencies));
		Check(JSONCreateString(RSTRING_CAST(L".\\WrongArchitectureOnly.qbc"),&value));
		Check(otherDependencies->Add(value));
		value.Release();
#ifdef _WIN64
		Check(StudioApplication::WrapModuleSettings(otherDependencies,dependencies,&value));
#else
		Check(StudioApplication::WrapModuleSettings(dependencies,otherDependencies,&value));
#endif
		Check(project->AddValueW(L"modules",value));
		value.Release();
		Check(ProjectTestIcon(project,TRUE));
	}
	Check(JSONWrapArray(components,&value));
	Check(definition->AddValueW(L"components",value));
	value.Release();
	Check(AQVM::StringField(metadata,L"unitId",&identity));
	Check(CLSIDFromString(const_cast<PWSTR>(identity.c_wstr()),&unitID));
	Check(StudioSource::Prepare(source,RSTRING_CAST(L"MainForm"),unitID,metadata,definition,RSTRING_CAST(L"Tick"),generated));
	{
		CRString body, prefix;
		PCWSTR stub = wcsstr(generated.text.c_wstr(),L"return true;");
		CheckIf(!stub,E_FAIL);
		Check(RStrCreateW(static_cast<INT>(stub - generated.text.c_wstr()),generated.text.c_wstr(),&prefix));
		PCWSTR checks = L"";
		if(plugins)
			checks = L"if(DirectPlugin.Ping() != 73 || FallbackPlugin.Ping() != 73 || Host.LoadQuadoo(\"FallbackPlugin.qbc\").Ping() != 73)\r\n\t\t{\r\n\t\t\tthrow \"Runtime plugin replacement failed\";\r\n\t\t}\r\n\t\t";
		else if(debugging)
			checks = L"var details = new DetailsFrame(this);\r\n\t\tvar proof = ExtraProjectCode();\r\n\t\tif(proof != 123 || Host.LoadQuadoo(\"DebugOnlyModule.quadoo\").Ping() != 456) throw \"Debug script did not load its includes or module.\";\r\n\t\t";
		Check(RStrFormatW(&body,L"#include \"Helper.quadoo\"\r\n%ls%lsForm.Close();\r\n\t\treturn true;%ls",prefix.c_wstr(),checks,stub + 12));
		Check(Text::SaveToFile(body.c_wstr(),RStrLen(body),sourcePath.c_wstr()));
	}
	Check(JSONWrapObject(generated.metadata,&value));
	Check(entry->AddValueW(L"designer",value));
	value.Release();
	{
		CMemoryStream json;
		Check(JSONSerializeObject(definition,&json));
		Check(Text::SaveToFile(json.TGetReadPtr<WCHAR>(),static_cast<INT>(json.TDataRemaining<WCHAR>()),definitionPath.c_wstr()));
	}
	{
		Check(Text::SaveToFile(s_helperSource,ARRAYSIZE(s_helperSource) - 1,helperPath.c_wstr()));
		TStackRef<IJSONObject> extra;
		Check(JSONCreateObject(&extra));
		Check(QFD::PutString(extra,L"path",RSTRING_CAST(L".\\Helper.quadoo")));
		Check(JSONWrapObject(extra,&value));
		Check(files->Add(value));
		value.Release();
	}
	{
		TStackRef<IJSONObject> frameUnit, frameEntry, frameDefinition;
		StudioSource::Result frame;
		GUID id = {0x593ce015,0xb015,0x40d9,{0x88,0x55,0x60,0,0,0,0,2}};
		PCWSTR frameJSON = L"{\"schemaVersion\":1,\"type\":\"Frame\",\"name\":\"DetailsFrame\",\"id\":\"{593CE015-B015-40D9-8855-600000000002}\"}";
		Check(QFD::Parse(frameJSON,TStrLenAssert(frameJSON),&frameDefinition));
		Check(JSONCreateObject(&frameUnit));
		Check(QFD::PutString(frameUnit,L"unitId",RSTRING_CAST(L"{593CE015-B015-40D9-8855-600000000002}")));
		Check(QFD::PutString(frameUnit,L"kind",RSTRING_CAST(L"frame")));
		Check(QFD::PutString(frameUnit,L"class",RSTRING_CAST(L"DetailsFrame")));
		Check(QFD::PutString(frameUnit,L"eventsInterface",RSTRING_CAST(L"IDetailsFrame")));
		Check(QFD::PutString(frameUnit,L"definition",RSTRING_CAST(L".\\DetailsFrame.qframe.json")));
		Check(StudioSource::Prepare(RSTRING_CAST(L"#pragma once\ninterface IDetailsFrame { };\nclass DetailsFrame : extern(Frame), IDetailsFrame { };\n"),RSTRING_CAST(L"DetailsFrame"),id,frameUnit,frameDefinition,NULL,frame));
		REQUIRE(wcsstr(frame.text.c_wstr(),L"Frame = Frame.Create(this, Owner);") != NULL);
		Check(Text::SaveToFile(frame.text.c_wstr(),RStrLen(frame.text),framePath.c_wstr()));
		CMemoryStream json;
		Check(JSONSerializeObject(frameDefinition,&json));
		Check(Text::SaveToFile(json.TGetReadPtr<WCHAR>(),static_cast<INT>(json.TDataRemaining<WCHAR>()),frameDefinitionPath.c_wstr()));
		Check(JSONCreateObject(&frameEntry));
		Check(QFD::PutString(frameEntry,L"path",RSTRING_CAST(L".\\DetailsFrame.quadoo")));
		Check(JSONWrapObject(frame.metadata,&value));
		Check(frameEntry->AddValueW(L"designer",value));
		value.Release();
		Check(JSONWrapObject(frameEntry,&value));
		Check(files->Add(value));
		value.Release();
	}
	Check(SaveStudioProject(project,projectPath.c_wstr()));
	{
		// A legacy cache, even malformed, must not override files[].default.
		TStackRef<IJSONObject> application;
		Check(project->FindNonNullValueW(L"application", &value));
		Check(value->GetObject(&application));
		value.Release();
		Check(JSONCreateInteger(17, &value));
		Check(application->AddValueW(L"mainForm", value));
		value.Release();
		Check(SaveStudioProject(project, projectPath.c_wstr()));
	}
	CheckIf(!GetModuleFileNameW(NULL,runtime,ARRAYSIZE(runtime)),HrEnsureLastError());
	{
		PCWSTR leaf = TStrRChr(runtime,L'\\');
		CheckIf(!leaf,E_FAIL);
		Check(RStrCreateW(static_cast<INT>(leaf - runtime),runtime,&runtimeDirectory));
	}
	if(debugging)
	{
		Check(ApplicationDebugAndRun(project,projectPath,folder,runtimeDirectory,sourcePath));
		goto Cleanup;
	}
	Check(StudioApplication::Build(project,projectPath,runtimeDirectory,executable,diagnostic));
	REQUIRE(GetFileAttributesW(executable.c_wstr()) != INVALID_FILE_ATTRIBUTES);
	{
		HINSTANCE image = reinterpret_cast<HINSTANCE>(LoadLibraryExW(executable.c_wstr(),NULL,LOAD_LIBRARY_AS_DATAFILE));
		REQUIRE(image != NULL);
		TStackRef<IJSONObject> embedded;
		TStackRef<IJSONObject> startupCatalog;
		CRString startupClass;
		Check(AQVM::ResourceObject(image, AQVM::CatalogType, MAKEINTRESOURCEW(AQVM::CatalogResource), &startupCatalog));
		Check(AQVM::ValidateCatalog(startupCatalog));
		Check(AQVM::StringField(startupCatalog, L"startupClass", &startupClass));
		REQUIRE(startupClass == RSTRING_CAST(L"MainForm"));
		REQUIRE(!startupCatalog->HasField(RSTRING_CAST(L"mainClass")));
		hr = AQVM::ResourceObject(image,AQVM::FrameType,L"DETAILSFRAME",&embedded);
		if(plugins && SUCCEEDED(hr))
		{
			HRSRC group = FindResourceW(image,MAKEINTRESOURCEW(1),RT_GROUP_ICON);
			HRSRC icon = FindResourceW(image,MAKEINTRESOURCEW(1),RT_ICON);
			HICON preview = static_cast<HICON>(LoadImageW(image,MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,LR_DEFAULTCOLOR));
			BOOL valid = group && icon && SizeofResource(image,group) == 34 && preview;
			if(valid)
			{
				// Directory entry 1 is the repaired 16x16 image, including its mask.
				valid = SizeofResource(image,icon) == sizeof(BITMAPINFOHEADER) + 16 * 16 * 4 + 16 * 4;
			}
			if(preview)
			{
				DestroyIcon(preview);
			}
			if(!Expect(valid,L"Application icon resources",__LINE__))
			{
				hr = E_FAIL;
			}
			TStackRef<IJSONValue> deployed;
			TStackRef<IJSONArray> names;
			if(SUCCEEDED(hr))
			{
				hr = startupCatalog->FindNonNullValueW(L"modules",&deployed);
			}
			if(SUCCEEDED(hr))
			{
				hr = deployed->GetArray(&names);
			}
			if(SUCCEEDED(hr) && !Expect(names->Count() == 2,L"Only the selected architecture's modules were deployed",__LINE__))
			{
				hr = E_FAIL;
			}
		}
		FreeLibrary(image);
		Check(hr);
		REQUIRE(embedded->HasField(RSTRING_CAST(L"type")));
	}
	Check(FileFingerprint(executable.c_wstr(),&before));
	if(plugins)
	{
		PCWSTR const invalid[] = {L"[\"MissingDependency.qbc\"]",L"[\"DirectPlugin.quadoo\",\".\\\\DirectPlugin.quadoo\"]",L"[\"QuadooVM.dll\"]",L"[\"RuntimeExample.exe\"]",L"[17]",L"\"not an array\""};
		TStackRef<IJSONValue> original;
		Check(project->FindNonNullValueW(L"modules",&original));
		for(UINT n = 0; n < ARRAYSIZE(invalid); n++)
		{
			TStackRef<IJSONValue> bad;
			Check(JSONParse(NULL,invalid[n],TStrLenAssert(invalid[n]),&bad));
			if(bad->GetType() == JSON::Array)
			{
				TStackRef<IJSONObject> settings;
				Check(JSONCreateObject(&settings));
				Check(settings->AddValueW(sizeof(void*) == 8 ? L"x64" : L"x86",bad));
				bad.Release();
				Check(JSONWrapObject(settings,&bad));
			}
			Check(project->AddValueW(L"modules",bad));
			Check(RejectApplicationBuild(project,projectPath,runtimeDirectory,executable));
		}
		Check(project->AddValueW(L"modules",original));
		Check(QFD::PutString(project,L"icon",RSTRING_CAST(L"invalid icon")));
		Check(RejectApplicationBuild(project,projectPath,runtimeDirectory,executable));
		Check(ProjectTestIcon(project,TRUE));
		// Force a changed module to be published before an injected failure, then
		// verify that both its prior bytes and the prior executable were restored.
		Check(RStrFormatW(&directModule,L"%r\\DirectPlugin.quadoo",static_cast<RSTRING>(folder)));
		Check(RStrFormatW(&fallbackModule,L"%r\\Modules\\FallbackPlugin.qbc",static_cast<RSTRING>(folder)));
		CRString outputModule;
		CRString moduleText;
		ULONGLONG moduleBefore;
		ULONGLONG moduleAfter;
		Check(RStrCreateW(static_cast<INT>(TStrRChr(executable.c_wstr(),L'\\') - executable.c_wstr()),executable.c_wstr(),&outputModule));
		CRString outputPath;
		Check(RStrFormatW(&outputPath,L"%r\\DirectPlugin.quadoo",static_cast<RSTRING>(outputModule)));
		Check(FileFingerprint(outputPath.c_wstr(),&moduleBefore));
		Check(ReadText(directModule.c_wstr(),moduleText));
		Check(Text::SaveToFile(SLP(L"changed module"),directModule.c_wstr()));
		StudioApplication::FailAfter(3);
		Check(RejectApplicationBuild(project,projectPath,runtimeDirectory,executable));
		StudioApplication::FailAfter(-1);
		Check(FileFingerprint(outputPath.c_wstr(),&moduleAfter));
		REQUIRE(moduleBefore == moduleAfter);
		Check(Text::SaveToFile(moduleText.c_wstr(),moduleText.Length(),directModule.c_wstr()));
	}
	for(INT failure = 0; failure < 4; failure++)
	{
		CRString ignored, error;
		StudioApplication::FailAfter(failure);
		REQUIRE(FAILED(StudioApplication::Build(project,projectPath,runtimeDirectory,ignored,error)));
		StudioApplication::FailAfter(-1);
		Check(FileFingerprint(executable.c_wstr(),&after));
		REQUIRE(before == after);
	}
	lock = CreateFileW(executable.c_wstr(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
	CheckIf(lock == INVALID_HANDLE_VALUE,HrEnsureLastError());
	{
		CRString ignored, error;
		REQUIRE(FAILED(StudioApplication::Build(project,projectPath,runtimeDirectory,ignored,error)));
	}
	CloseHandle(lock);
	lock = INVALID_HANDLE_VALUE;
	Check(FileFingerprint(executable.c_wstr(),&after));
	REQUIRE(before == after);
	{
		CRString missingRuntime;
		CRString badTemplate;
		CRString validTemplate;
		TStackRef<IJSONValue> enabled;
		Check(JSONCreateBool(false,&enabled));
		Check(entry->AddValueW(L"default",enabled));
		Check(RejectApplicationBuild(project,projectPath,runtimeDirectory,executable));
		enabled.Release();
		Check(JSONCreateBool(true,&enabled));
		Check(entry->AddValueW(L"default",enabled));
		Check(RStrFormatW(&missingRuntime,L"%ls\\Missing Runtime",folder.c_wstr()));
		CheckIf(!CreateDirectoryW(missingRuntime.c_wstr(),NULL),HrEnsureLastError());
		Check(RStrFormatW(&badTemplate,L"%ls\\AQVMT.bin",missingRuntime.c_wstr()));
		Check(RejectApplicationBuild(project,projectPath,missingRuntime,executable,badTemplate.c_wstr()));
		CheckIf(!CopyFileW(runtime,badTemplate.c_wstr(),FALSE),HrEnsureLastError());
		Check(RejectApplicationBuild(project,projectPath,missingRuntime,executable));
		Check(RStrFormatW(&validTemplate,L"%ls\\AQVMT.bin",runtimeDirectory.c_wstr()));
		CheckIf(!CopyFileW(validTemplate.c_wstr(),badTemplate.c_wstr(),FALSE),HrEnsureLastError());
		// Add runtime files one at a time so each missing dependency is checked
		// independently, even though a previous good application already exists.
		PCWSTR const dependencies[] = {L"QuadooForm.dll",L"QuadooVM.dll",L"QuadooParser.dll",L"JSON.dll",L"SimbeyCore.dll"};
		for(UINT n = 0; n < ARRAYSIZE(dependencies); n++)
		{
			CRString original;
			CRString missing;
			Check(RStrFormatW(&original,L"%ls\\%ls",runtimeDirectory.c_wstr(),dependencies[n]));
			Check(RStrFormatW(&missing,L"%ls\\%ls",missingRuntime.c_wstr(),dependencies[n]));
			Check(RejectApplicationBuild(project,projectPath,missingRuntime,executable,missing.c_wstr()));
			CheckIf(!CopyFileW(original.c_wstr(),missing.c_wstr(),FALSE),HrEnsureLastError());
		}
		static const WCHAR collision[] = L"function WinMain(a, b)\r\n{\r\n\treturn 0;\r\n}\r\n";
		Check(Text::SaveToFile(collision,ARRAYSIZE(collision) - 1,helperPath.c_wstr()));
		Check(RejectApplicationBuild(project,projectPath,runtimeDirectory,executable));
		Check(Text::SaveToFile(s_helperSource,ARRAYSIZE(s_helperSource) - 1,helperPath.c_wstr()));
		static const WCHAR helperCollision[] = L"function __QuadooApplicationStartup(nCmdShow)\r\n{\r\n}\r\n";
		Check(Text::SaveToFile(SLP(helperCollision), helperPath.c_wstr()));
		Check(RejectApplicationBuild(project, projectPath, runtimeDirectory, executable));
		Check(Text::SaveToFile(SLP(s_helperSource), helperPath.c_wstr()));
	}
	Check(RStrFormatW(&hiddenSource,L"%ls.hidden",sourcePath.c_wstr()));
	Check(RStrFormatW(&hiddenDefinition,L"%ls.hidden",definitionPath.c_wstr()));
	Check(RStrFormatW(&hiddenHelper,L"%ls.hidden",helperPath.c_wstr()));
	Check(RStrFormatW(&hiddenFrame,L"%ls.hidden",framePath.c_wstr()));
	Check(RStrFormatW(&hiddenFrameDefinition,L"%ls.hidden",frameDefinitionPath.c_wstr()));
	CheckIf(!MoveFileW(sourcePath.c_wstr(),hiddenSource.c_wstr()),HrEnsureLastError());
	movedSource = TRUE;
	CheckIf(!MoveFileW(definitionPath.c_wstr(),hiddenDefinition.c_wstr()),HrEnsureLastError());
	movedDefinition = TRUE;
	CheckIf(!MoveFileW(helperPath.c_wstr(),hiddenHelper.c_wstr()),HrEnsureLastError());
	movedHelper = TRUE;
	CheckIf(!MoveFileW(framePath.c_wstr(),hiddenFrame.c_wstr()),HrEnsureLastError());
	movedFrame = TRUE;
	CheckIf(!MoveFileW(frameDefinitionPath.c_wstr(),hiddenFrameDefinition.c_wstr()),HrEnsureLastError());
	movedFrameDefinition = TRUE;
	CheckIf(!GetWindowsDirectoryW(working,ARRAYSIZE(working)),HrEnsureLastError());
	if(plugins)
	{
		Check(RStrFormatW(&hiddenDirect,L"%r.hidden",static_cast<RSTRING>(directModule)));
		Check(RStrFormatW(&hiddenFallback,L"%r.hidden",static_cast<RSTRING>(fallbackModule)));
		CheckIf(!MoveFileW(directModule.c_wstr(),hiddenDirect.c_wstr()),HrEnsureLastError());
		movedDirect = TRUE;
		CheckIf(!MoveFileW(fallbackModule.c_wstr(),hiddenFallback.c_wstr()),HrEnsureLastError());
		movedFallback = TRUE;
	}
	Check(RunBuiltApplication(executable,working));
	REQUIRE(GetFileAttributesW(sourcePath.c_wstr()) == INVALID_FILE_ATTRIBUTES);
	Check(RStrFormatW(&startupDirectory,L"%ls\\Main.quadoo",folder.c_wstr()));
	REQUIRE(GetFileAttributesW(startupDirectory.c_wstr()) == INVALID_FILE_ATTRIBUTES);
Cleanup:
	StudioApplication::FailAfter(-1);
	if(movedDirect)
	{
		MoveFileW(hiddenDirect.c_wstr(),directModule.c_wstr());
	}
	if(movedFallback)
	{
		MoveFileW(hiddenFallback.c_wstr(),fallbackModule.c_wstr());
	}
	if(lock != INVALID_HANDLE_VALUE)
	{
		CloseHandle(lock);
	}
	if(movedSource)
	{
		MoveFileW(hiddenSource.c_wstr(),sourcePath.c_wstr());
	}
	if(movedDefinition)
	{
		MoveFileW(hiddenDefinition.c_wstr(),definitionPath.c_wstr());
	}
	if(movedHelper)
	{
		MoveFileW(hiddenHelper.c_wstr(),helperPath.c_wstr());
	}
	if(movedFrame)
	{
		MoveFileW(hiddenFrame.c_wstr(),framePath.c_wstr());
	}
	if(movedFrameDefinition)
	{
		MoveFileW(hiddenFrameDefinition.c_wstr(),frameDefinitionPath.c_wstr());
	}
	if(FAILED(hr) && RStrLen(diagnostic))
	{
		Log(diagnostic.c_wstr());
		Log(L"\r\n");
	}
	return hr;
}
