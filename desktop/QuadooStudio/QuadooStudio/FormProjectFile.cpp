#include <windows.h>
#include "Published\CodeEdit.h"
#include "Library\Util\TextHelpers.h"
#include "FormProjectFile.h"
#include "QuadooProject.h"
#include "FormSourceSync.h"

static UINT_PTR s_nextDesignerSession;
static HRESULT UnitString(IJSONObject* data, PCWSTR key, RSTRING* value)
{
	HRESULT hr;
	TStackRef<IJSONValue> json;
	Check(data->FindNonNullValueW(key,&json));
	Check(json->GetString(value));
Cleanup:
	return hr;
}
CStudioFormUnit::CStudioFormUnit() : m_sourceSnapshot(FALSE), m_definitionSnapshot(FALSE), owner(NULL), sourceFile(NULL), metadataDirty(FALSE), sourceError(S_OK), recoveryModified(FALSE), metadata(NULL), editor(NULL), id(GUID_NULL), session(++s_nextDesignerSession), outlineRevision(0), form(TRUE), design(FALSE), expandRoot(TRUE) {}
CStudioFormUnit::~CStudioFormUnit() { Close(); SafeRelease(metadata); }
VOID CStudioFormUnit::Hide()
{
	if(editor) { ShowWindow(editor->Window(),SW_HIDE); ShowWindow(editor->PaletteWindow(),SW_HIDE); }
}
VOID CStudioFormUnit::Close()
{
	if(editor) editor->Document().SetSourceSite(NULL);
	if(editor && IsWindow(editor->Window())) DestroyWindow(editor->Window());
	SafeRelease(editor); outlineRevision = 0; session = ++s_nextDesignerSession; expandRoot = TRUE;
}
HRESULT CStudioFormUnit::SourceName(RSTRING source, RSTRING* name)
{
	HRESULT hr;
	CRString candidate;
	PCWSTR path = RStrToWide(source), leaf = path, end;
	CheckIf(!name || !path,E_INVALIDARG);
	*name = NULL;
	for(PCWSTR p = path; *p; p++) if(*p == L'\\' || *p == L'/') leaf = p + 1;
	end = TStrRChr(leaf,L'.');
	CheckIf(!end || end == leaf,E_INVALIDARG);
	Check(RStrCreateW(static_cast<INT>(end - leaf),leaf,&candidate));
	Check(StudioSource::ValidateIdentifier(candidate));
	*name = candidate.Detach();
Cleanup:
	return hr;
}
// A recovery record names only this unit's two known targets. Backups have
// deterministic sibling names; recovery never accepts paths to operate on from
// the record itself. Keep backups until both writes (or restores) complete.
static HRESULT RecoverUnit(PCWSTR source, PCWSTR definition, PCWSTR project = NULL)
{
	HRESULT hr = S_OK;
	CRString journal, sourceBackup, definitionBackup, projectBackup, expected;
	PWSTR text = NULL; INT length = 0;
	Check(RStrFormatW(&journal,L"%ls.studio-save",project ? project : definition));
	Check(RStrFormatW(&sourceBackup,L"%ls.studio-backup",source));
	Check(RStrFormatW(&definitionBackup,L"%ls.studio-backup",definition));
	if(GetFileAttributesW(journal.c_wstr()) != INVALID_FILE_ATTRIBUTES)
	{
		if(project)
		{
			Check(RStrFormatW(&projectBackup,L"%ls.studio-backup",project));
			Check(RStrFormatW(&expected,L"QuadooStudio unit save 2\r\n%ls\r\n%ls\r\n%ls\r\n",source,definition,project));
		}
		else
		{
			Check(RStrFormatW(&expected,L"QuadooStudio unit save 1\r\n%ls\r\n%ls\r\n",source,definition));
		}
		Check(Text::LoadFromFile(journal.c_wstr(),&text,&length));
		CheckIf(length != RStrLen(expected) || memcmp(text,expected.c_wstr(),length * sizeof(WCHAR)),HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
		CheckIf(!CopyFileW(sourceBackup.c_wstr(),source,FALSE),HrEnsureLastError());
		CheckIf(!CopyFileW(definitionBackup.c_wstr(),definition,FALSE),HrEnsureLastError());
		if(project) {
			CheckIf(!CopyFileW(projectBackup.c_wstr(),project,FALSE),HrEnsureLastError());
		}
		CheckIf(!DeleteFileW(journal.c_wstr()),HrEnsureLastError());
	}
	DeleteFileW(sourceBackup.c_wstr()); DeleteFileW(definitionBackup.c_wstr());
	if(RStrLen(projectBackup)) DeleteFileW(projectBackup.c_wstr());
Cleanup:
	SafeDeleteArray(text);
	return hr;
}
static HRESULT ReadUnitText(PCWSTR path, RSTRING* result)
{
	HRESULT hr;
	PWSTR text = NULL;
	INT length = 0;
	Check(Text::LoadFromFile(path,&text,&length,CP_UTF8));
	Check(RStrCreateW(length,text,result));
Cleanup:
	SafeDeleteArray(text);
	return hr;
}
HRESULT CStudioFormUnit::RecoverProject(PCWSTR projectPath)
{
	HRESULT hr = S_OK;
	CRString journal, record, text, directory;
	TStackRef<IJSONObject> project;
	TStackRef<IJSONValue> value;
	TStackRef<IJSONArray> files;
	BOOL matched = FALSE;
	Check(RStrFormatW(&journal,L"%ls.studio-save",projectPath));
	if(GetFileAttributesW(journal.c_wstr()) == INVALID_FILE_ATTRIBUTES) goto Cleanup;
	Check(ReadUnitText(journal.c_wstr(),&record));
	Check(ReadUnitText(projectPath,&text));
	Check(QFD::Parse(text.c_wstr(),RStrLen(text),&project));
	Check(project->FindNonNullValueW(L"files",&value));
	Check(value->GetArray(&files));
	value.Release();
	{
		PCWSTR leaf = TStrRChr(projectPath,L'\\');
		CheckIf(!leaf,E_INVALIDARG);
		Check(RStrCreateW(static_cast<INT>(leaf - projectPath + 1),projectPath,&directory));
	}
	for(sysint n = 0; n < files->Count(); n++)
	{
		TStackRef<IJSONObject> entry, metadata;
		CRString source, definition, expected;
		PWSTR sourcePath = NULL, definitionPath = NULL; INT count;
		Check(files->GetObject(n,&entry));
		if(!entry->HasField(RSTRING_CAST(L"designer"))) continue;
		Check(entry->FindNonNullValueW(L"designer",&value));
		Check(value->GetObject(&metadata));
		value.Release();
		Check(UnitString(entry,L"path",&source));
		Check(UnitString(metadata,L"definition",&definition));
		hr = Formatting::TBuildDirectory(directory.c_wstr(),RStrLen(directory),source.c_wstr(),RStrLen(source),&sourcePath,&count);
		if(SUCCEEDED(hr)) hr = Formatting::TBuildDirectory(directory.c_wstr(),RStrLen(directory),definition.c_wstr(),RStrLen(definition),&definitionPath,&count);
		if(SUCCEEDED(hr)) hr = RStrFormatW(&expected,L"QuadooStudio unit save 2\r\n%ls\r\n%ls\r\n%ls\r\n",sourcePath,definitionPath,projectPath);
		if(SUCCEEDED(hr) && expected == static_cast<RSTRING>(record)) { hr = RecoverUnit(sourcePath,definitionPath,projectPath); matched = TRUE; }
		SafeDeleteArray(sourcePath); SafeDeleteArray(definitionPath);
		Check(hr);
		if(matched) break;
	}
	CheckIf(!matched,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
Cleanup:
	return hr;
}
HRESULT CStudioFormUnit::Initialize(IJSONObject* data, RSTRING directory, RSTRING source)
{
	HRESULT hr;
	CRString relative, kind, identity, className, interfaceName, expectedInterface;
	PWSTR absolute = NULL; INT count = 0;
	Check(UnitString(data,L"definition",&relative));
	Check(UnitString(data,L"kind",&kind));
	CheckIf(!(kind == RSTRING_CAST(L"form")) && !(kind == RSTRING_CAST(L"frame")),E_INVALIDARG);
	form = kind == RSTRING_CAST(L"form");
	Check(UnitString(data,L"unitId",&identity));
	Check(CLSIDFromString(const_cast<PWSTR>(identity.c_wstr()),&id));
	CheckIf(IsEqualGUID(id,GUID_NULL),E_INVALIDARG);
	Check(SourceName(source,&name));
	Check(UnitString(data,L"class",&className));
	Check(UnitString(data,L"eventsInterface",&interfaceName));
	Check(RStrFormatW(&expectedInterface,L"I%ls",name.c_wstr()));
	CheckIf(lstrcmpW(className.c_wstr(),name.c_wstr()) || lstrcmpW(interfaceName.c_wstr(),expectedInterface.c_wstr()),E_INVALIDARG);
	Check(Formatting::TBuildDirectory(RStrToWide(directory),RStrLen(directory),relative.c_wstr(),RStrLen(relative),&absolute,&count));
	Check(RStrCreateW(count,absolute,&definition));
	SafeDeleteArray(absolute); absolute = NULL;
	Check(Formatting::TBuildDirectory(RStrToWide(directory),RStrLen(directory),RStrToWide(source),RStrLen(source),&absolute,&count));
	Check(RecoverUnit(absolute,definition.c_wstr()));
	m_sourceSnapshot = SUCCEEDED(ReadUnitText(absolute,&m_savedSource));
	m_definitionSnapshot = SUCCEEDED(ReadUnitText(definition.c_wstr(),&m_savedDefinition));
	SetInterface(metadata,data);
	savedMetadata = data;
Cleanup:
	SafeDeleteArray(absolute);
	return hr;
}
HRESULT CStudioFormUnit::EnsureEditor(HWND parent, HWND paletteParent, IQuadooControlRegistry* registry, UINT dpi)
{
	HRESULT hr = S_OK;
	TStackRef<QFD::CDesignerEditor> candidate;
	CRString currentDefinition;
	QFD::EditorHostOptions options = { sizeof(options),paletteParent,parent,session };
	if(editor) goto Cleanup;
	Check(ReadUnitText(definition.c_wstr(),&currentDefinition));
	candidate.Attach(__new QFD::CDesignerEditor);
	CheckAlloc(candidate);
	Check(candidate->CreateHosted(parent,&options,registry));
	ShowWindow(candidate->Window(),SW_HIDE);
	candidate->SetDPI(dpi);
	Check(candidate->OpenDocument(definition.c_wstr()));
	CheckIf(candidate->Document().IsForm() != form,QuadooForm::EInvalidDefinition);
	m_savedDefinition = currentDefinition;
	m_definitionSnapshot = TRUE;
	editor = candidate.Detach();
	editor->Document().SetSourceSite(owner ? this : NULL);
Cleanup:
	if(candidate && IsWindow(candidate->Window())) DestroyWindow(candidate->Window());
	return hr;
}
HRESULT CStudioFormUnit::Save(ITextDocument* source, PCWSTR sourcePath)
{
	HRESULT hr;
	TStackRef<IJSONObject> json;
	TStackRef<ITextDocument> retainedSource;
	CMemoryStream sourceText, definitionText, projectText;
	PWSTR original = NULL; INT originalLength = 0;
	CRString journal, sourceBackup, definitionBackup, record, currentSource, currentDefinition, savedSource, savedDefinition, projectPath, projectBackup, savedProject;
	WCHAR sourceTemp[MAX_PATH] = {0}, definitionTemp[MAX_PATH] = {0}, projectTemp[MAX_PATH] = {0}, directory[MAX_PATH];
	BOOL journalWritten = FALSE;
	if(editor)
	{
		Check(editor->CommitPropertyEdit());
	}
	if(owner)
	{
		if(editor || metadataDirty) {
			Check(owner->SynchronizeFormSource(sourceFile));
		}
		if(!source)
		{
			Check(owner->SourceDocument(sourceFile,&retainedSource));
			source = retainedSource;
		}
		Check(owner->PrepareUnitSave(sourceFile,&projectText,&projectPath));
		Check(RecoverProject(projectPath.c_wstr()));
	}
	Check(RecoverUnit(sourcePath,definition.c_wstr()));
	Check(ReadUnitText(sourcePath,&currentSource));
	CheckIf(!m_sourceSnapshot || !(m_savedSource == static_cast<RSTRING>(currentSource)),HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
	if(!editor && !metadataDirty)
	{
		CheckIf(!source,E_UNEXPECTED);
		Check(source->StreamOut(&sourceText));
		Check(RStrCreateW(sourceText.TDataRemaining<WCHAR>(),sourceText.TGetReadPtr<WCHAR>(),&savedSource));
		Check(TStrCchCpy(directory,ARRAYSIZE(directory),sourcePath));
		PWSTR leaf = const_cast<PWSTR>(TStrRChr(directory,L'\\'));
		CheckIf(!leaf,E_INVALIDARG);
		*leaf = 0;
		CheckIf(!GetTempFileNameW(directory,L"QSS",0,sourceTemp),HrEnsureLastError());
		Check(Text::SaveToFile(sourceText.TGetReadPtr<WCHAR>(),sourceText.TDataRemaining<WCHAR>(),sourceTemp));
		CheckIf(!MoveFileExW(sourceTemp,sourcePath,MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),HrEnsureLastError());
		sourceTemp[0] = 0;
		m_savedSource = savedSource;
		source->ResetModifiedSnapshot();
		goto Cleanup;
	}
	Check(ReadUnitText(definition.c_wstr(),&currentDefinition));
	// Refuse to overwrite edits made by another application. Keep both in-memory
	// buffers dirty so the user can export/copy them before reopening the unit.
	CheckIf(!m_definitionSnapshot || !(m_savedDefinition == static_cast<RSTRING>(currentDefinition)),HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
	if(editor) {
		Check(editor->Document().Serialize(&json));
	}
	else {
		Check(QFD::Parse(currentDefinition.c_wstr(),RStrLen(currentDefinition),&json));
	}
	Check(JSONSerializeObject(json,&definitionText));
	if(source)
	{
		Check(source->StreamOut(&sourceText));
	}
	else
	{
		ULONG written;
		Check(Text::LoadFromFile(sourcePath,&original,&originalLength));
		Check(sourceText.Write(original,originalLength * sizeof(WCHAR),&written));
	}
	Check(RStrCreateW(sourceText.TDataRemaining<WCHAR>(),sourceText.TGetReadPtr<WCHAR>(),&savedSource));
	Check(RStrCreateW(definitionText.TDataRemaining<WCHAR>(),definitionText.TGetReadPtr<WCHAR>(),&savedDefinition));
	if(owner)
	{
		Check(RStrCreateW(static_cast<INT>(projectText.TDataRemaining<WCHAR>()),projectText.TGetReadPtr<WCHAR>(),&savedProject));
		Check(RStrFormatW(&journal,L"%ls.studio-save",projectPath.c_wstr()));
		Check(RStrFormatW(&projectBackup,L"%ls.studio-backup",projectPath.c_wstr()));
	}
	else {
		Check(RStrFormatW(&journal,L"%ls.studio-save",definition.c_wstr()));
	}
	Check(RStrFormatW(&sourceBackup,L"%ls.studio-backup",sourcePath));
	Check(RStrFormatW(&definitionBackup,L"%ls.studio-backup",definition.c_wstr()));
	if(owner) {
		Check(RStrFormatW(&record,L"QuadooStudio unit save 2\r\n%ls\r\n%ls\r\n%ls\r\n",sourcePath,definition.c_wstr(),projectPath.c_wstr()));
	}
	else {
		Check(RStrFormatW(&record,L"QuadooStudio unit save 1\r\n%ls\r\n%ls\r\n",sourcePath,definition.c_wstr()));
	}
	Check(TStrCchCpy(directory,ARRAYSIZE(directory),sourcePath));
	PWSTR leaf = const_cast<PWSTR>(TStrRChr(directory,L'\\'));
	CheckIf(!leaf,E_INVALIDARG);
	*leaf = 0;
	CheckIf(!GetTempFileNameW(directory,L"QSS",0,sourceTemp),HrEnsureLastError());
	Check(TStrCchCpy(directory,ARRAYSIZE(directory),definition.c_wstr()));
	leaf = const_cast<PWSTR>(TStrRChr(directory,L'\\'));
	CheckIf(!leaf,E_INVALIDARG);
	*leaf = 0;
	CheckIf(!GetTempFileNameW(directory,L"QSF",0,definitionTemp),HrEnsureLastError());
	if(owner)
	{
		Check(TStrCchCpy(directory,ARRAYSIZE(directory),projectPath.c_wstr()));
		PWSTR projectLeaf = const_cast<PWSTR>(TStrRChr(directory,L'\\'));
		CheckIf(!projectLeaf,E_INVALIDARG);
		*projectLeaf = 0;
		CheckIf(!GetTempFileNameW(directory,L"QSP",0,projectTemp),HrEnsureLastError());
		Check(Text::SaveToFile(projectText.TGetReadPtr<WCHAR>(),static_cast<INT>(projectText.TDataRemaining<WCHAR>()),projectTemp));
	}
	Check(Text::SaveToFile(sourceText.TGetReadPtr<WCHAR>(),sourceText.TDataRemaining<WCHAR>(),sourceTemp));
	Check(Text::SaveToFile(definitionText.TGetReadPtr<WCHAR>(),definitionText.TDataRemaining<WCHAR>(),definitionTemp));
	CheckIf(!CopyFileW(sourcePath,sourceBackup.c_wstr(),TRUE),HrEnsureLastError());
	CheckIf(!CopyFileW(definition.c_wstr(),definitionBackup.c_wstr(),TRUE),HrEnsureLastError());
	if(owner) {
		CheckIf(!CopyFileW(projectPath.c_wstr(),projectBackup.c_wstr(),TRUE),HrEnsureLastError());
	}
	Check(Text::SaveToFile(record.c_wstr(),RStrLen(record),journal.c_wstr()));
	journalWritten = TRUE;
	CheckIf(!MoveFileExW(sourceTemp,sourcePath,MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),HrEnsureLastError());
	sourceTemp[0] = 0;
	CheckIf(!MoveFileExW(definitionTemp,definition.c_wstr(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),HrEnsureLastError());
	definitionTemp[0] = 0;
	if(owner)
	{
		CheckIf(!MoveFileExW(projectTemp,projectPath.c_wstr(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),HrEnsureLastError());
		projectTemp[0] = 0;
	}
	CheckIf(!DeleteFileW(journal.c_wstr()),HrEnsureLastError());
	journalWritten = FALSE;
	m_savedSource = savedSource;
	m_savedDefinition = savedDefinition;
	metadataDirty = FALSE;
	savedMetadata = metadata;
	if(owner) owner->UnitProjectSaved(savedProject);
	if(editor) editor->Document().SetDirty(FALSE);
	if(source) source->ResetModifiedSnapshot();
	hr = S_OK;
Cleanup:
	if(journalWritten) RecoverUnit(sourcePath,definition.c_wstr(),owner ? projectPath.c_wstr() : NULL);
	if(!journalWritten && RStrLen(journal)) DeleteFileW(journal.c_wstr());
	if(!journalWritten && RStrLen(sourceBackup)) DeleteFileW(sourceBackup.c_wstr());
	if(!journalWritten && RStrLen(definitionBackup)) DeleteFileW(definitionBackup.c_wstr());
	if(!journalWritten && RStrLen(projectBackup)) DeleteFileW(projectBackup.c_wstr());
	if(projectTemp[0]) DeleteFileW(projectTemp);
	if(sourceTemp[0]) DeleteFileW(sourceTemp);
	if(definitionTemp[0]) DeleteFileW(definitionTemp);
	SafeDeleteArray(original);
	return hr;
}

HRESULT CStudioFormUnit::PrepareDefinition(IJSONObject* proposed, QFD::ISourceEdit** edit)
{
	return owner ? owner->PrepareFormSource(sourceFile,proposed,NULL,edit) : E_UNEXPECTED;
}
HRESULT CStudioFormUnit::OpenHandler(REFGUID component, RSTRING eventName)
{
	return owner ? owner->OpenFormHandler(sourceFile,component,eventName) : E_UNEXPECTED;
}
