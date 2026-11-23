#include <windows.h>
#include "Library\Util\TextHelpers.h"
#include "QuadooProject.h"
#include "ApplicationProject.h"
#include "FormSourceSync.h"
#include "Library\Window\BaseWindow.h"

static ULONG MappedPosition(ULONG position, const StudioSource::Edit& edit)
{
	if(position >= static_cast<ULONG>(edit.offset + edit.length)) return position + RStrLen(edit.text) - edit.length;
	if(position > static_cast<ULONG>(edit.offset)) return edit.offset + RStrLen(edit.text);
	return position;
}

static ULONG Newlines(PCWSTR text, INT count)
{
	ULONG lines = 0;
	for(INT n = 0; n < count; n++) if(text[n] == L'\n') lines++;
	return lines;
}

static HRESULT StudioPrepareEventDefinition(IJSONObject* root, REFGUID id, RSTRING eventName, RSTRING handler, UINT depth = 0)
{
	HRESULT hr;
	TStackRef<IJSONValue> value;
	GUID current;
	CheckIf(depth > 128,E_INVALIDARG);
	Check(root->FindNonNullValueW(L"id",&value));
	Check(value->GetStringAsGuid(current));
	value.Release();
	if(IsEqualGUID(id,current))
	{
		TStackRef<IJSONObject> events;
		if(root->HasField(RSTRING_CAST(L"events"))) {
			Check(root->FindNonNullValueW(L"events",&value));
			Check(value->GetObject(&events));
		value.Release(); }
		else {
			Check(JSONCreateObject(&events));
			Check(JSONWrapObject(events,&value));
			Check(root->AddValueW(L"events",value));
		value.Release(); }
		Check(JSONCreateString(handler,&value));
		Check(events->AddValue(eventName,value));
		goto Cleanup;
	}
	for(UINT f = 0; f < 2; f++)
	{
		PCWSTR field = f ? L"components" : L"children";
		if(!root->HasField(RSTRING_CAST(field))) continue;
		TStackRef<IJSONArray> items;
		Check(root->FindNonNullValueW(field,&value));
		Check(value->GetArray(&items));
		value.Release();
		for(sysint n = 0; n < items->Count(); n++)
		{
			TStackRef<IJSONObject> item;
			Check(items->GetObject(n,&item));
			hr = StudioPrepareEventDefinition(item,id,eventName,handler,depth + 1);
			if(hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) goto Cleanup;
		}
	}
	hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
Cleanup:
	return hr;
}

class CStudioSourceEdit : public CBaseUnknown, public QFD::ISourceEdit
{
	CQuadooProject* m_owner;
	CProjectFile* m_file;
	TStackRef<ITextDocument> m_document;
	BOOL m_finished, m_changed, m_locked, m_wasModified, m_metadataChanged;
	TEXT_EDIT_VIEW m_view;
	ULONG m_scrollOffset;
	TArray<TVNLINESCHANGED> m_lines;
	CRString m_originalText;

public:
	StudioSource::Result result;

public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
	END_UNK_MAP

public:
	CStudioSourceEdit(CQuadooProject* owner, CProjectFile* file) : m_owner(owner),m_file(file),m_finished(FALSE),m_changed(FALSE),m_locked(FALSE),m_wasModified(FALSE),m_metadataChanged(FALSE),m_scrollOffset(0)
	{
		owner->AddRef(); ZeroMemory(&m_view,sizeof(m_view));
	}

	~CStudioSourceEdit() { Finish(FALSE); m_owner->Release(); }

	HRESULT Apply(ITextDocument* document, RSTRING original)
	{
		HRESULT hr;
		CheckIf(m_owner->m_sourceDepth,HRESULT_FROM_WIN32(ERROR_BUSY));
		m_owner->m_sourceDepth++; m_locked = TRUE;
		m_document = document;
		m_originalText = original;
		m_wasModified = document->IsModified();
		m_metadataChanged = JSONCompareObjects(m_file->m_form->metadata,result.metadata) != 0;
		if(m_owner->m_pCodeFile == m_file) m_owner->m_pEditor->GetTextEditView(&m_view);
		else m_view = m_file->StoredView();
		if(m_view.nVScrollPos < document->LineCount()) m_scrollOffset = static_cast<ULONG>(document->LineOffset(m_view.nVScrollPos));
		for(sysint n = 0; n < result.edits.Length(); n++)
		{
			const StudioSource::Edit& edit = result.edits[n];
			TVNLINESCHANGED change; ULONG offset; ZeroMemory(&change,sizeof(change));
			CheckIf(!document->GetLineFromOffset(edit.offset,&change.nLineNo,&offset),E_INVALIDARG);
			change.fAtLineStart = !offset;
			change.cOldLines = Newlines(RStrToWide(original) + edit.offset,edit.length);
			change.cNewLines = Newlines(edit.text.c_wstr(),RStrLen(edit.text));
			Check(m_lines.Append(change));
		}
		Check(StudioSource::Checkpoint());
		if(result.edits.Length())
		{
			document->Break(); document->Group();
			for(sysint n = 0; n < result.edits.Length(); n++)
			{
				const StudioSource::Edit& edit = result.edits[n];
				hr = document->Replace(edit.offset,edit.text.c_wstr(),RStrLen(edit.text),edit.length);
				if(FAILED(hr)) break;
				m_changed = TRUE;
			}
			document->Ungroup(); document->Break();
			Check(hr);
		}
		Check(StudioSource::Checkpoint());
	Cleanup:
		return hr;
	}

	virtual HRESULT Finish(BOOL commit)
	{
		HRESULT hr = S_OK;
		if(m_finished) return S_FALSE;
		m_finished = TRUE;
		if(commit)
		{
			for(sysint n = 0; n < result.edits.Length(); n++)
			{
				const StudioSource::Edit& edit = result.edits[n]; BOOL changed;
				m_file->AdjustBreakpoints(&m_lines[n],&changed);
				m_view.nSelectionStart = MappedPosition(m_view.nSelectionStart,edit);
				m_view.nSelectionEnd = MappedPosition(m_view.nSelectionEnd,edit);
				m_view.nCursorOffset = MappedPosition(m_view.nCursorOffset,edit);
				m_scrollOffset = MappedPosition(m_scrollOffset,edit);
			}
			ULONG offset;
			m_document->GetLineFromOffset(m_view.nCursorOffset,&m_view.nCurrentLine,&offset);
			m_document->GetLineFromOffset(m_scrollOffset,&m_view.nVScrollPos,&offset);
			ReplaceInterface(m_file->m_form->metadata,static_cast<IJSONObject*>(result.metadata));
			m_file->m_form->sourceDiagnostic = result.diagnostic;
			m_file->m_form->sourceError = S_OK;
			if(m_changed || m_metadataChanged) m_file->m_form->metadataDirty = TRUE;
		}
		else if(m_document)
		{
			CRString current;
			if(m_changed)
			{
				ULONG start, end;
				hr = m_document->Undo(&start,&end);
			}
			HRESULT verify = StudioSource::ReadDocument(m_document,&current);
			// Even a document implementation that reports failure after changing
			// text must not leave partial declarations in the user's source.
			if(FAILED(verify) || !(current == static_cast<RSTRING>(m_originalText)))
			{
				if(!m_changed)
				{
					ULONG start, end;
					hr = m_document->Undo(&start,&end);
					current = static_cast<RSTRING>(NULL);
					verify = StudioSource::ReadDocument(m_document,&current);
				}
				if(FAILED(verify) || !(current == static_cast<RSTRING>(m_originalText)))
				{
					hr = m_document->Load(m_originalText.c_wstr(),RStrLen(m_originalText));
					if(SUCCEEDED(hr)) m_file->m_form->sourceDiagnostic = RSTRING_CAST(L"Source was restored after a text-edit failure; its Undo history was reset.");
				}
			}
			if(SUCCEEDED(hr) && !m_wasModified) m_document->ResetModifiedSnapshot();
			if(m_wasModified && !m_document->IsModified()) m_file->m_form->metadataDirty = TRUE;
			if(FAILED(hr))
			{
				m_file->m_form->sourceRecovery = m_originalText;
				m_file->m_form->recoveryModified = m_wasModified;
				m_file->m_form->sourceError = hr;
				m_file->m_form->sourceDiagnostic = RSTRING_CAST(L"Source rollback failed. Original text is retained; use Synchronize Script Declarations to restore it.");
			}
		}
		if(m_document)
		{
			if(m_owner->m_pCodeFile == m_file) m_owner->m_pEditor->SetTextEditView(&m_view);
			else m_file->StoredView() = m_view;
		}
		if(m_locked) { m_owner->m_sourceDepth--; m_locked = FALSE; }
		return hr;
	}
};

HRESULT CQuadooProject::SourceDocument(CProjectFile* file, ITextDocument** document)
{
	HRESULT hr;
	TStackRef<ICodeEditor> temporary;
	TStackRef<IOleWindow> window;
	HWND hwnd = NULL;
	PWSTR text = NULL; INT count = 0;
	RECT rect = {0,0,1,1};
	CheckIf(!file || !document,E_POINTER);
	*document = NULL;
	if(m_pCodeFile == file) { *document = m_pEditor->GetTextDocument(); (*document)->AddRef(); hr = S_OK; goto Cleanup; }
	if(file->StoredDocument()) { *document = file->StoredDocument(); (*document)->AddRef(); hr = S_OK; goto Cleanup; }
	Check(Text::LoadFromFile(file->m_pwzAbsolutePath,&text,&count,CP_UTF8));
	Check(CodeEditCreate(m_hwnd,rect,4,false,false,&temporary));
	Check(temporary->QueryInterface(&window));
	Check(window->GetWindow(&hwnd));
	::ShowWindow(hwnd,SW_HIDE);
	Check(temporary->Prepare(text,count));
	file->StoreDocument(temporary->GetTextDocument());
	*document = file->StoredDocument(); (*document)->AddRef();
Cleanup:
	if(hwnd) DestroyWindow(hwnd);
	SafeDeleteArray(text);
	return hr;
}

HRESULT CQuadooProject::PrepareUnitSave(CProjectFile* file, CMemoryStream* project, RSTRING* path)
{
	HRESULT hr;
	PWSTR disk = NULL; INT length = 0;
	CheckIf(!project || !path,E_POINTER);
	*path = NULL;
	Check(CStudioFormUnit::RecoverProject(RStrToWide(m_rstrProject)));
	Check(Text::LoadFromFile(RStrToWide(m_rstrProject),&disk,&length,CP_UTF8));
	CheckIf(length != RStrLen(m_savedProject) || memcmp(disk,m_savedProject.c_wstr(),length * sizeof(WCHAR)),HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
	m_savingUnit = file;
	Check(UpdateFiles());
	Check(SerializeStudioProject(m_pProject,*project));
	RStrSet(*path,m_rstrProject);
Cleanup:
	m_savingUnit = NULL;
	SafeDeleteArray(disk);
	return hr;
}

HRESULT CQuadooProject::SavedUnitBreakpoints(CProjectFile* file, IJSONValue** points)
{
	HRESULT hr;
	TStackRef<IJSONObject> saved, entry;
	TStackRef<IJSONValue> files;
	CheckIf(!file || !points,E_POINTER);
	*points = NULL;
	Check(QFD::Parse(m_savedProject.c_wstr(),RStrLen(m_savedProject),&saved));
	Check(saved->FindNonNullValueW(L"files",&files));
	hr = JSONFindArrayObjectIndirect(files,RSTRING_CAST(L"path"),file->m_rstrPath,&entry,NULL);
	if(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) { hr = S_FALSE; goto Cleanup; }
	Check(hr);
	if(entry->HasField(RSTRING_CAST(L"breakpoints")))
	{
		Check(entry->FindNonNullValueW(L"breakpoints",points));
	}
	else
		hr = S_FALSE;
Cleanup:
	return hr;
}

HRESULT CQuadooProject::PrepareFormSource(CProjectFile* file, IJSONObject* proposed, RSTRING handler, QFD::ISourceEdit** edit, INT* position, BOOL repair)
{
	HRESULT hr;
	TStackRef<ITextDocument> document;
	TStackRef<CStudioSourceEdit> candidate;
	CRString original;
	CheckIf(!file || !file->m_form || !proposed || !edit,E_INVALIDARG);
	*edit = NULL;
	CheckIf(m_sourceDepth,HRESULT_FROM_WIN32(ERROR_BUSY));
	CheckIf(RStrLen(file->m_form->sourceRecovery) && !repair,E_UNEXPECTED);
	Check(SourceDocument(file,&document));
	Check(StudioSource::ReadDocument(document,&original));
	candidate.Attach(__new CStudioSourceEdit(this,file));
	CheckAlloc(candidate);
	Check(StudioSource::Prepare(original,file->m_form->name,file->m_form->id,file->m_form->metadata,proposed,handler,candidate->result,repair));
	Check(candidate->Apply(document,original));
	if(position)
		*position = candidate->result.handlerOffset;
	*edit = candidate.Detach();
Cleanup:
	if(FAILED(hr) && file && file->m_form)
	{
		file->m_form->sourceError = hr;
		if(candidate && RStrLen(candidate->result.diagnostic))
			file->m_form->sourceDiagnostic = candidate->result.diagnostic;
		else
		{
			file->m_form->sourceDiagnostic = static_cast<RSTRING>(NULL);
			RStrFormatW(&file->m_form->sourceDiagnostic,L"Source synchronization failed (0x%.8x).",hr);
		}
	}
	return hr;
}

HRESULT CQuadooProject::SynchronizeFormSource(CProjectFile* file, BOOL repair)
{
	HRESULT hr;
	TStackRef<IJSONObject> definition;
	TStackRef<QFD::ISourceEdit> edit;
	CheckIf(!file || !file->m_form,E_INVALIDARG);
	if(repair && RStrLen(file->m_form->sourceRecovery))
	{
		TStackRef<ITextDocument> source;
		Check(SourceDocument(file,&source));
		Check(source->Load(file->m_form->sourceRecovery.c_wstr(),RStrLen(file->m_form->sourceRecovery)));
		if(file->m_form->recoveryModified) file->m_form->metadataDirty = TRUE;
		else source->ResetModifiedSnapshot();
	}
	if(file->m_form->editor)
	{
		Check(file->m_form->editor->Document().Serialize(&definition));
	}
	else
	{
		PWSTR text = NULL; INT count;
		hr = Text::LoadFromFile(file->m_form->definition.c_wstr(),&text,&count,CP_UTF8);
		if(SUCCEEDED(hr)) hr = QFD::Parse(text,count,&definition);
		SafeDeleteArray(text);
		Check(hr);
	}
	Check(PrepareFormSource(file,definition,NULL,&edit,NULL,repair));
	Check(edit->Finish(TRUE));
	if(repair)
		file->m_form->sourceRecovery = static_cast<RSTRING>(NULL);
Cleanup:
	return hr;
}

HRESULT CQuadooProject::OpenFormHandler(CProjectFile* file, REFGUID component, RSTRING eventName)
{
	HRESULT hr;
	TStackRef<IQuadooComponent> control;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IJSONObject> definition;
	TStackRef<QFD::ISourceEdit> edit;
	CRString handler, name;
	UINT index; INT position = -1;
	QFD::CDesignerEditor* editor = file && file->m_form ? file->m_form->editor : NULL;
	CheckIf(!editor,E_INVALIDARG);
	Check(editor->CommitPropertyEdit());
	Check(editor->Document().FindComponent(component,&control));
	Check(QFD::Property(control,RStrToWide(eventName),&properties,&index));
	Check(properties->GetValueEvent(index,&handler));
	if(!RStrLen(handler))
	{
		TStackRef<IJSONValue> value;
		Check(control->GetName(&name));
		PCWSTR suffix = RStrToWide(eventName); if(suffix[0] == L'O' && suffix[1] == L'n') suffix += 2;
		handler = static_cast<RSTRING>(NULL);
		Check(RStrFormatW(&handler,L"%ls_%ls",name.c_wstr(),suffix));
		// Preflight the implementation with the proposed binding before editing
		// either document. The native property is then set under that source edit.
		Check(editor->Document().Serialize(&definition));
		Check(StudioPrepareEventDefinition(definition,component,eventName,handler));
		Check(PrepareFormSource(file,definition,handler,&edit,&position));
		Check(JSONCreateString(handler,&value));
		// The source is already prepared. Avoid recursively entering its site.
		editor->Document().SetSourceSite(NULL);
		hr = editor->Document().SetProperty(control,index,value);
		editor->Document().SetSourceSite(file->m_form);
		Check(hr);
		Check(edit->Finish(TRUE));
	}
	else
	{
		Check(editor->Document().Serialize(&definition));
		Check(PrepareFormSource(file,definition,handler,&edit,&position));
		Check(edit->Finish(TRUE));
	}
	CheckIf(position < 0,HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
	file->m_form->design = FALSE;
	Check(SwitchToFile(file));
	{
		TEXT_EDIT_VIEW view = {0}; ULONG line, column;
		m_pEditor->GetTextEditView(&view);
		view.nCursorOffset = view.nSelectionStart = view.nSelectionEnd = position;
		m_pEditor->GetTextDocument()->GetLineFromOffset(position,&line,&column);
		view.nCurrentLine = line; m_pEditor->SetTextEditView(&view); m_pEditor->ScrollView(0,line); m_pEditor->SetFocus();
	}
Cleanup:
	return hr;
}
