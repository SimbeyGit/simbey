#include "..\..\..\..\desktop\QuadooStudio\QuadooStudio\FormSourceSync.h"
#include "Published\QuadooParser.h"
#include "Published\QuadooVM.h"
#pragma comment(lib,"QuadooVM.lib")
class CSourceCompilerStatus : public IQuadooCompilerStatus
{
public:
	virtual VOID STDMETHODCALLTYPE OnCompilerAddFile(PCWSTR,INT) {}
	virtual VOID STDMETHODCALLTYPE OnCompilerStatus(PCWSTR) {}
	virtual VOID STDMETHODCALLTYPE OnCompilerError(HRESULT code,INT line,PCWSTR file,PCWSTR error)
	{
		CRString text;
		if(SUCCEEDED(RStrFormatW(&text,L"Compiler 0x%.8x %ls:%d: %ls\r\n",code,file,line,error))) Log(text.c_wstr());
	}
	virtual HRESULT STDMETHODCALLTYPE OnCompilerResolvePath(PCWSTR,PWSTR,INT) { return E_NOTIMPL; }
};
class CRejectSourceFactory : public CBaseUnknown, public IQuadooControlFactory, public IQuadooFactoryLifetime
{
	TStackRef<IQuadooControlFactory> m_base;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooControlFactory)
		UNK_INTERFACE(IQuadooFactoryLifetime)
	END_UNK_MAP
	CRejectSourceFactory(IQuadooControlFactory* base) : m_base(base) {}
	STDMETHOD(GetTypeName)(RSTRING* name) { return RStrCreateW(LSP(L"SourceReject"),name); }
	STDMETHOD(GetProperties)(IQuadooPropertyDescriptors** properties) { return m_base->GetProperties(properties); }
	STDMETHOD(GetEvents)(IQuadooEventDescriptors** events) { return m_base->GetEvents(events); }
	STDMETHOD(GetLiveCount)(UINT* count) { if(count) *count = 0; return count ? S_OK : E_POINTER; }
	STDMETHOD(CreateControl)(IQuadooConstructionContext*,IQuadooControlContainer*,IJSONObject*,IQuadooControl** control) { if(control) *control = NULL; return E_ACCESSDENIED; }
};
// Deliberately adversarial text storage: failure can follow a successful write.
// This verifies recovery rather than relying on CodeEdit's current internals.
class CFailingSourceDocument : public CBaseUnknown, public ITextDocument
{
	TStackRef<ITextDocument> m_document;
public:
	BOOL partial, failUndo, failLoad;
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(ITextDocument)
	END_UNK_MAP
	CFailingSourceDocument(ITextDocument* document) : m_document(document),partial(TRUE),failUndo(FALSE),failLoad(FALSE) {}
	STDMETHOD(Load)(PCWSTR text,INT count) { return failLoad ? E_OUTOFMEMORY : m_document->Load(text,count); }
	STDMETHOD_(VOID,Clear)() { m_document->Clear(); }
	STDMETHOD_(size_w,Size)() const { return m_document->Size(); }
	STDMETHOD_(bool,IsPrepared)() const { return m_document->IsPrepared(); }
	STDMETHOD_(bool,CanUndo)() const { return m_document->CanUndo(); }
	STDMETHOD_(bool,CanRedo)() const { return m_document->CanRedo(); }
	STDMETHOD(Insert)(size_w index,PCWSTR text,INT count) { return m_document->Insert(index,text,count); }
	STDMETHOD(Replace)(size_w index,PCWSTR text,INT count,size_w erased)
	{
		HRESULT hr = m_document->Replace(index,text,count,erased);
		if(SUCCEEDED(hr) && partial) { partial = FALSE; hr = E_OUTOFMEMORY; }
		return hr;
	}
	STDMETHOD(Erase)(size_w index,size_w count) { return m_document->Erase(index,count); }
	STDMETHOD_(VOID,Group)() { m_document->Group(); }
	STDMETHOD_(VOID,Ungroup)() { m_document->Ungroup(); }
	STDMETHOD_(VOID,Break)() { m_document->Break(); }
	STDMETHOD(Undo)(ULONG* first,ULONG* last) { return failUndo ? E_FAIL : m_document->Undo(first,last); }
	STDMETHOD(Redo)(ULONG* first,ULONG* last) { return m_document->Redo(first,last); }
	STDMETHOD_(bool,IsModified)() { return m_document->IsModified(); }
	STDMETHOD_(VOID,ResetModifiedSnapshot)() { m_document->ResetModifiedSnapshot(); }
	STDMETHOD_(ULONG,LineCount)() const { return m_document->LineCount(); }
	STDMETHOD_(size_w,LongestLine)() const { return m_document->LongestLine(); }
	STDMETHOD_(bool,GetLineFromOffset)(size_w index,ULONG* line,ULONG* column) { return m_document->GetLineFromOffset(index,line,column); }
	STDMETHOD_(size_w,LineOffset)(ULONG line) const { return m_document->LineOffset(line); }
	STDMETHOD_(size_w,LineLength)(ULONG line) const { return m_document->LineLength(line); }
	STDMETHOD(Render)(size_w index,seqchar_t* text,size_w length,size_w* copied) const { return m_document->Render(index,text,length,copied); }
	STDMETHOD(StreamOut)(ISequentialStream* stream) const { return m_document->StreamOut(stream); }
	STDMETHOD_(INT,GetTabWidth)() const { return m_document->GetTabWidth(); }
};

static HRESULT SourceCompilerContract()
{
	HRESULT hr;
	TStackRef<IJSONObject> metadata, definition;
	TStackRef<ISequentialStream> program;
	TStackRef<IQuadooInstanceLoader> loader;
	TStackRef<IQuadooVM> vm;
	TStackRef<IQuadooObject> exception;
	TStackRef<IQuadooInterface> methods;
	GUID unit = {0x72c15d19,0xbc00,0x4a18,{0x9a,0x11,0,0,0,0,0,1}};
	QuadooVM::QVARIANT object = {QuadooVM::Null}, value = {QuadooVM::Null}, output = {QuadooVM::Null};
	ULONG ordinal; DWORD parameters; HRESULT registered;
	CRString source, complete;
	StudioSource::Result result;
	CSourceCompilerStatus compilerStatus;
	REQUIRE(SUCCEEDED(StudioSource::ValidateIdentifier(RSTRING_CAST(L"Button_1"))));
	REQUIRE(FAILED(StudioSource::ValidateIdentifier(RSTRING_CAST(L"class"))));
	REQUIRE(FAILED(StudioSource::ValidateIdentifier(RSTRING_CAST(L"Foo Bar"))));
	REQUIRE(FAILED(StudioSource::ValidateIdentifier(RSTRING_CAST(L"Foo;"))));
	REQUIRE(FAILED(StudioSource::ValidateIdentifier(RSTRING_CAST(L""))));
	REQUIRE(FAILED(StudioSource::ValidateIdentifier(RSTRING_CAST(L"3Button"))));
	// Identifier syntax and length belong to the parser, including its handling
	// of whitespace/comments and characters outside Studio's former allowlist.
	{
		WCHAR longName[130];
		for(UINT n = 0; n < ARRAYSIZE(longName) - 1; n++) longName[n] = L'A';
		longName[ARRAYSIZE(longName) - 1] = L'\0';
		PCWSTR names[] = {L"Button_1",L"class",L"Foo Bar",L"Foo;",L"Foo()",L"",L"3Button",L" Foo ",L"Foo/* comment */",L"\x00E9" L"Control",longName};
		for(UINT n = 0; n < ARRAYSIZE(names); n++)
		{
			BOOL valid = FALSE;
			HRESULT identifierHr = QuadooIsValidIdentifier(names[n],TStrLenAssert(names[n]),&valid);
			HRESULT expected = FAILED(identifierHr) ? identifierHr : (valid ? S_OK : E_INVALIDARG);
			REQUIRE(StudioSource::ValidateIdentifier(RSTRING_CAST(names[n])) == expected);
		}
	}
	Check(JSONCreateObject(&metadata));
	PCWSTR definitionText = L"{\"id\":\"{72C15D19-BC00-4A18-9A11-000000000002}\",\"name\":\"MainForm\",\"type\":\"Form\",\"children\":[{\"id\":\"{72C15D19-BC00-4A18-9A11-000000000003}\",\"name\":\"Button1\",\"type\":\"Button\",\"events\":{\"OnClick\":\"Button1_Click\"}}]}";
	Check(QFD::Parse(definitionText,TStrLenAssert(definitionText),&definition));
	Check(RStrCreateW(LSP(L"// class MainForm { fake }\r\ninterface IMainForm\r\n{\r\n};\r\nclass MainForm : IMainForm\r\n{\r\n\tvar quoted = \"} // not a scope\";\r\n};\r\n"),&source));
	Check(StudioSource::Prepare(source,RSTRING_CAST(L"MainForm"),unit,metadata,definition,RSTRING_CAST(L"Button1_Click"),result));
	REQUIRE(result.edits.Length() == 3 && result.handlerOffset >= 0);
	REQUIRE(wcsstr(result.text.c_wstr(),L"property var Button1;") && wcsstr(result.text.c_wstr(),L"virtual function Button1_Click (oArgs) = 0;"));
	Check(RStrFormatW(&complete,L"%ls\r\nfunction Make () { return new MainForm(); }\r\n",result.text.c_wstr()));
	Check(QuadooAllocStream(&program));
	Check(QuadooParseTextToStream(L"SourceContract.quadoo",21,complete.c_wstr(),RStrLen(complete),0,program,NULL,&compilerStatus));
	Log(L"Generated script compiled.\r\n");
	Check(QVMCreateLoader(NULL,&loader));
	Check(loader->AddInstance(RSTRING_CAST(L"SourceContract"),NULL,program,QuadooStreamDataSize(program),NULL));
	Check(loader->LoadVM(RSTRING_CAST(L"SourceContract"),&registered,&vm));
	Check(vm->RunConstructor(&exception));
	REQUIRE(!exception);
	Check(vm->FindFunction(L"Make",&ordinal,&parameters));
	REQUIRE(parameters == 0);
	Check(vm->RunFunction(ordinal,&object));
	REQUIRE(object.eType == QuadooVM::Object && object.pObject);
	value.eType = QuadooVM::I4; value.lVal = 123;
	Check(object.pObject->SetProperty(NULL,RSTRING_CAST(L"Button1"),&value));
	Check(object.pObject->GetProperty(NULL,RSTRING_CAST(L"Button1"),&output));
	REQUIRE(output.eType == QuadooVM::I4 && output.lVal == 123); QVMClearVariant(&output);
	Check(object.pObject->GetProperty(NULL,RSTRING_CAST(L"__QuadooUnitId"),&output));
	REQUIRE(output.eType == QuadooVM::String); QVMClearVariant(&output);
	REQUIRE(FAILED(object.pObject->SetProperty(NULL,RSTRING_CAST(L"__QuadooUnitId"),&value)));
	Check(object.pObject->GetInterface(RSTRING_CAST(L"IMainForm"),&methods));
	Check(methods->ResolveMethod(RSTRING_CAST(L"Button1_Click"),&ordinal));
	{ QuadooVM::QVPARAMS arguments; ZeroMemory(&arguments,sizeof(arguments)); arguments.cArgs = 1; arguments.pqvArgs = &value;
		Check(methods->Invoke(ordinal,&arguments,&output));
	REQUIRE(output.eType == QuadooVM::Bool && output.fVal); QVMClearVariant(&output); }
	{
		StudioSource::Result unchanged;
		Check(StudioSource::Prepare(result.text,RSTRING_CAST(L"MainForm"),unit,result.metadata,definition,NULL,unchanged));
		REQUIRE(!unchanged.edits.Length());
	}
	{
		StudioSource::Result broken; CRString edited;
		Check(RStrFormatW(&edited,L"%ls ",result.text.c_wstr()));
		// Corrupting an owned declaration is detected rather than overwritten.
		PWSTR write; CRString copy;
		Check(RStrAllocW(RStrLen(edited),&copy,&write));
		CopyMemory(write,edited.c_wstr(),RStrLen(edited) * sizeof(WCHAR));
		PWSTR name = const_cast<PWSTR>(wcsstr(write,L"property var Button1")); REQUIRE(name != NULL); name[13] = L'X';
		REQUIRE(StudioSource::Prepare(copy,RSTRING_CAST(L"MainForm"),unit,result.metadata,definition,NULL,broken) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
		StudioSource::Result repaired;
		Check(StudioSource::Prepare(copy,RSTRING_CAST(L"MainForm"),unit,result.metadata,definition,NULL,repaired,TRUE));
		REQUIRE(wcsstr(repaired.text.c_wstr(),L"property var Button1;") != NULL);
	}
	{
		static PCWSTR invalid[] = {
			L"class Other { };",
			L"class MainForm { /* incomplete",
			L"class MainForm { var text = \"incomplete; };",
			L"#if DEBUG\nclass MainForm { };\n#endif",
			L"class MainForm { var Button1 = function (x) { return x; }; };",
			L"class MainForm { virtual function Button1_Click () { return true; } };",
			L"class MainForm { static function Button1_Click (args) { return true; } };",
			L"class MainForm { virtual bool Button1_Click (args) { return true; } };",
			L"class MainForm { var same; var same; };"
		};
		for(UINT n = 0; n < ARRAYSIZE(invalid); n++)
		{
			StudioSource::Result rejected;
			REQUIRE(FAILED(StudioSource::Prepare(RSTRING_CAST(invalid[n]),RSTRING_CAST(L"MainForm"),unit,metadata,definition,NULL,rejected)));
			REQUIRE(RStrLen(rejected.diagnostic) > 0);
		}
	}
	{
		StudioSource::Result reused;
		PCWSTR existing = L"interface IMainForm { virtual function Button1_Click (args) = 0; };\nclass MainForm : IMainForm {\nvar counter;\nMainForm (n) : counter(n) { }\nvirtual function Button1_Click (args) { return false; }\n};\n";
		Check(StudioSource::Prepare(RSTRING_CAST(existing),RSTRING_CAST(L"MainForm"),unit,metadata,definition,RSTRING_CAST(L"Button1_Click"),reused));
		REQUIRE(reused.handlerOffset >= 0 && !RStrLen(reused.diagnostic));
		REQUIRE(wcsstr(reused.text.c_wstr(),L"virtual function Button1_Click (args) { return false; }") != NULL);
		REQUIRE(wcsstr(reused.text.c_wstr(),L"virtual function Button1_Click (oArgs) = 0;") == NULL);
	}
	{
		StudioSource::Result included;
		Check(StudioSource::Prepare(RSTRING_CAST(L"#include \"DoesNotExist.quadoo\"\n// A lexer must not load an include to edit this unit.\nclass MainForm { };"),RSTRING_CAST(L"MainForm"),unit,metadata,definition,NULL,included));
		REQUIRE(wcsstr(included.text.c_wstr(),L"interface IMainForm") != NULL);
		REQUIRE(RStrLen(included.diagnostic) > 0);
	}
	hr = S_OK;
Cleanup:
	methods.Release(); QVMClearVariant(&output); QVMClearVariant(&value); QVMClearVariant(&object);
	if(vm) vm->Unload();
	return hr;
}

static HRESULT SourceSynchronization(PCWSTR parentFolder)
{
	HRESULT hr;
	TStackRef<CQuadooStudio> studio;
	TStackRef<IJSONObject> project, serialized;
	TStackRef<IJSONValue> value, copy;
	TStackRef<ITextDocument> document;
	TStackRef<IQuadooControlContainer> root, panel;
	TStackRef<IQuadooComponent> component;
	TStackRef<IQuadooControl> button;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooControlFactory> baseFactory;
	TStackRef<CRejectSourceFactory> rejectFactory;
	CRString folder, path, packages, before, after, disk, framePath;
	CQuadooProject* ide = NULL; CProjectFile *file = NULL, *frame = NULL;
	QFD::CDesignerEditor* editor = NULL;
	QFD::CDesignerEditor* frameEditor = NULL;
	GUID panelID, buttonID, timerID, rootID, pasted;
	UINT index;
	QuadooForm::Rect bounds = {8,8,160,80};
	ULONG breakpoint = 0, column = 0;
	HANDLE lock = INVALID_HANDLE_VALUE;
	Check(SourceCompilerContract());
	Check(RStrFormatW(&folder,L"%ls\\SourceSync",parentFolder));
	CheckIf(!CreateDirectoryW(folder.c_wstr(),NULL),HrEnsureLastError());
	Check(RStrFormatW(&path,L"%ls\\Application.qsproj",folder.c_wstr()));
	Check(RStrFormatW(&packages,L"%ls\\Packages",folder.c_wstr()));
	Check(JSONCreateObject(&project));
	Check(QFD::PutString(project,L"target",RSTRING_CAST(L"application")));
	Check(CreateApplicationFixture(project,path.c_wstr()));
	studio.Attach(__new CQuadooStudio(GetModuleHandle(NULL)));
	CheckAlloc(studio);
	Check(studio->Initialize(L"",1100,740,SW_SHOWNOACTIVATE,packages.c_wstr()));
	Check(studio->OpenProject(path.c_wstr()));
	Pump();
	ide = studio->Project();
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file));
	Check(ide->SourceDocument(file,&document));
	Check(StudioSource::ReadDocument(document,&before));
	REQUIRE(wcsstr(before.c_wstr(),L"<QuadooStudio:members") != NULL);
	editor = ide->ActiveDesigner(); REQUIRE(editor != NULL);
	Check(editor->Document().Root()->GetID(&rootID));
	Check(editor->Document().Root()->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&root)));
	Check(editor->AddControl(RSTRING_CAST(L"Panel"),root,&bounds,&panelID));
	Check(editor->Document().FindComponent(panelID,&component));
	Check(component->QueryInterface(__uuidof(IQuadooControlContainer),reinterpret_cast<PVOID*>(&panel)));
	component.Release();
	Check(editor->AddControl(RSTRING_CAST(L"Button"),panel,&bounds,&buttonID));
	Check(editor->AddComponent(RSTRING_CAST(L"Timer"),&timerID));
	Check(StudioSource::ReadDocument(document,&after));
	REQUIRE(wcsstr(after.c_wstr(),L"property var Panel1;") && wcsstr(after.c_wstr(),L"property var Button1;") && wcsstr(after.c_wstr(),L"property var Timer1;"));
	Check(editor->Document().FindComponent(buttonID,&component));
	Check(QFD::Property(component,L"Name",&properties,&index));
	Check(JSONCreateString(RSTRING_CAST(L"Foo Bar"),&value));
	REQUIRE(FAILED(editor->Document().SetProperty(component,index,value))); value.Release();
	before = after; after = static_cast<RSTRING>(NULL);
	Check(StudioSource::ReadDocument(document,&after));
	REQUIRE(before == static_cast<RSTRING>(after));
	Check(JSONCreateString(RSTRING_CAST(L"AcceptButton"),&value));
	Check(editor->Document().SetProperty(component,index,value));
	value.Release(); properties.Release();
	after = static_cast<RSTRING>(NULL);
	Check(StudioSource::ReadDocument(document,&after));
	REQUIRE(wcsstr(after.c_wstr(),L"property var AcceptButton;") && !wcsstr(after.c_wstr(),L"property var Button1;"));
	Check(QFD::Property(component,L"OnClick",&properties,&index));
	Check(JSONCreateString(RSTRING_CAST(L"Shared_Click"),&value));
	Check(editor->Document().SetProperty(component,index,value));
	value.Release(); properties.Release();
	REQUIRE(RStrLen(file->m_form->sourceDiagnostic) != 0 && SUCCEEDED(file->m_form->sourceError));
	Check(ide->OpenFormHandler(file,buttonID,RSTRING_CAST(L"OnClick")));
	Pump(); REQUIRE(!ide->DesignerActive());
	after = static_cast<RSTRING>(NULL);
	Check(StudioSource::ReadDocument(document,&after));
	REQUIRE(wcsstr(after.c_wstr(),L"virtual function Shared_Click (oArgs)\r\n\t{") != NULL);
	Check(ide->SetDesignView(TRUE));
	Pump();
	// Breakpoints follow user code below the generated region, including edits
	// to an inactive source document. Navigation also retains the same document.
	{
		PCWSTR body = wcsstr(after.c_wstr(),L"return true;"); REQUIRE(body != NULL);
		CheckIf(!document->GetLineFromOffset(static_cast<ULONG>(body - after.c_wstr()),&breakpoint,&column),E_FAIL);
		Check(file->ToggleBreakpoint(breakpoint));
		GUID temporary;
		Check(editor->AddComponent(RSTRING_CAST(L"Timer"),&temporary));
		REQUIRE(file->GetBreakpoint(0) == breakpoint + 1);
		TStackRef<IQuadooComponent> timer;
		Check(editor->Document().FindComponent(temporary,&timer));
		Check(editor->Document().Remove(timer));
		REQUIRE(file->GetBreakpoint(0) == breakpoint);
	}
	// Renaming does not rename the user-owned handler implementation.
	Check(QFD::Property(component,L"Name",&properties,&index));
	Check(JSONCreateString(RSTRING_CAST(L"RunButton"),&value));
	Check(editor->Document().SetProperty(component,index,value));
	value.Release(); properties.Release();
	after = static_cast<RSTRING>(NULL);
	Check(StudioSource::ReadDocument(document,&after));
	REQUIRE(wcsstr(after.c_wstr(),L"property var RunButton;") && wcsstr(after.c_wstr(),L"virtual function Shared_Click (oArgs)\r\n\t{"));
	// Failure checkpoints happen before native publication; source/metadata roll back.
	for(INT n = 0; n < 4; n++)
	{
		before = after;
		StudioSource::FailAfter(n);
		GUID unused; HRESULT failed = editor->AddComponent(RSTRING_CAST(L"Timer"),&unused);
		StudioSource::FailAfter(-1); REQUIRE(FAILED(failed));
		after = static_cast<RSTRING>(NULL);
		Check(StudioSource::ReadDocument(document,&after));
		REQUIRE(before == static_cast<RSTRING>(after));
	}
	// Paste retains method names; a destination unit reports missing implementations.
	Check(editor->Document().CopyDefinition(buttonID,&serialized));
	Check(JSONWrapObject(serialized,&copy));
	serialized.Release();
	Check(RStrFormatW(&framePath,L"%ls\\DetailsFrame.quadoo",folder.c_wstr()));
	Check(ide->NewFormUnit(FALSE,framePath.c_wstr()));
	Pump();
	Check(ide->FindSource(RSTRING_CAST(L".\\DetailsFrame.quadoo"),&frame));
	frameEditor = ide->ActiveDesigner(); REQUIRE(frameEditor != NULL);
	Check(frameEditor->Document().Root()->GetID(&rootID));
	Check(frameEditor->Document().PasteDefinition(rootID,copy,&pasted));
	REQUIRE(RStrLen(frame->m_form->sourceDiagnostic) && wcsstr(frame->m_form->sourceDiagnostic.c_wstr(),L"Shared_Click"));
	Check(ide->OpenFormHandler(frame,pasted,RSTRING_CAST(L"OnClick")));
	Pump();
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	REQUIRE(!file->IsModified() && !frame->IsModified());
	// A native factory failure after successful source preparation restores a
	// clean source buffer and does not publish a component declaration.
	Check(ide->FormRegistry()->FindFactory(RSTRING_CAST(L"Button"),&baseFactory));
	rejectFactory.Attach(__new CRejectSourceFactory(baseFactory));
	CheckAlloc(rejectFactory);
	Check(ide->FormRegistry()->RegisterFactory(rejectFactory));
	{
		GUID rejected;
		REQUIRE(editor->AddControl(RSTRING_CAST(L"SourceReject"),root,&bounds,&rejected) == E_ACCESSDENIED);
		REQUIRE(!document->IsModified() && !file->IsModified());
		CRString unchanged;
		Check(StudioSource::ReadDocument(document,&unchanged));
		REQUIRE(!wcsstr(unchanged.c_wstr(),L"property var SourceReject"));
	}
	Check(ide->FormRegistry()->UnregisterFactory(RSTRING_CAST(L"SourceReject")));
	rejectFactory.Release(); baseFactory.Release();
	for(UINT failure = 0; failure < 3; failure++)
	{
		TStackRef<CFailingSourceDocument> broken;
		CRString original, restored;
		GUID rejected;
		Check(StudioSource::ReadDocument(document,&original));
		broken.Attach(__new CFailingSourceDocument(document));
		CheckAlloc(broken);
		broken->failUndo = failure != 0; broken->failLoad = failure == 2;
		file->StoreDocument(broken);
		REQUIRE(FAILED(editor->AddComponent(RSTRING_CAST(L"Timer"),&rejected)));
		if(failure == 2)
		{
			REQUIRE(file->m_form->sourceRecovery == static_cast<RSTRING>(original) && file->IsModified());
			broken->failUndo = broken->failLoad = FALSE;
			Check(ide->SynchronizeFormSource(file,TRUE));
			REQUIRE(!RStrLen(file->m_form->sourceRecovery));
		}
		Check(StudioSource::ReadDocument(document,&restored));
		REQUIRE(original == static_cast<RSTRING>(restored) && !file->IsModified());
		file->StoreDocument(document);
	}
	// Undo/manual damage is diagnosed. Explicit reconciliation repairs only
	// owned declarations; user handler bodies remain in place.
	{
		CRString current;
		Check(StudioSource::ReadDocument(document,&current));
		PCWSTR member = wcsstr(current.c_wstr(),L"property var RunButton"); REQUIRE(member != NULL);
		Check(document->Replace(member - current.c_wstr() + 13,L"X",1,1));
		GUID rejected;
		REQUIRE(editor->AddComponent(RSTRING_CAST(L"Timer"),&rejected) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
		QuadooForm::Rect moved = {14,15,100,30};
		Check(editor->Document().Find(buttonID,&button));
		Check(editor->Document().SetBounds(button,&moved));
		button.Release();
		Check(ide->SynchronizeFormSource(file,TRUE));
		current = static_cast<RSTRING>(NULL);
		Check(StudioSource::ReadDocument(document,&current));
		REQUIRE(wcsstr(current.c_wstr(),L"property var RunButton;") && wcsstr(current.c_wstr(),L"virtual function Shared_Click (oArgs)\r\n\t{"));
	}
	// Work on an inactive unit without touching the currently displayed buffer.
	before = static_cast<RSTRING>(NULL);
	Check(StudioSource::ReadDocument(document,&before));
	Check(editor->AddComponent(RSTRING_CAST(L"Timer"),&timerID));
	after = static_cast<RSTRING>(NULL);
	Check(StudioSource::ReadDocument(document,&after));
	REQUIRE(wcsstr(after.c_wstr(),L"property var Timer2;") != NULL);
	Check(ide->Save());
	{
		TStackRef<IJSONObject> saved, entry, data, sync;
		TStackRef<IJSONValue> entries, field;
		CRString declarations;
		Check(ReadJSON(path.c_wstr(),&saved));
		Check(saved->FindNonNullValueW(L"files",&entries));
		Check(JSONFindArrayObjectIndirect(entries,RSTRING_CAST(L"path"),file->m_rstrPath,&entry,NULL));
		Check(entry->FindNonNullValueW(L"designer",&field));
		Check(field->GetObject(&data));
		field.Release();
		Check(data->FindNonNullValueW(L"sourceSync",&field));
		Check(field->GetObject(&sync));
		field.Release();
		Check(sync->FindNonNullValueW(L"membersText",&field));
		Check(field->GetString(&declarations));
		REQUIRE(!wcsstr(declarations.c_wstr(),L"property var Timer2;"));
		REQUIRE(file->IsModified());
		field.Release();
		Check(entry->FindNonNullValueW(L"breakpoints",&field));
		{
			TStackRef<IJSONArray> points;
			TStackRef<IJSONValue> first;
			INT line;
			Check(field->GetArray(&points));
			Check(points->GetValue(0,&first));
			Check(first->GetInteger(&line));
			REQUIRE(line == static_cast<INT>(breakpoint + 1)); // Saved lines are one-based.
			REQUIRE(file->GetBreakpoint(0) == breakpoint + 1); // Pending source gained a line.
		}
	}
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(ReadText(file->m_pwzAbsolutePath,disk));
	REQUIRE(disk == static_cast<RSTRING>(after));
	// Deleting a container removes all owned child declarations and the last
	// managed interface declaration, while preserving user implementations.
	component.Release();
	Check(editor->Document().FindComponent(panelID,&component));
	Check(editor->Document().Remove(component));
	component.Release(); panel.Release();
	after = static_cast<RSTRING>(NULL);
	Check(StudioSource::ReadDocument(document,&after));
	REQUIRE(!wcsstr(after.c_wstr(),L"property var Panel1;") && !wcsstr(after.c_wstr(),L"property var RunButton;"));
	REQUIRE(!wcsstr(after.c_wstr(),L"virtual function Shared_Click (oArgs) = 0;") && wcsstr(after.c_wstr(),L"virtual function Shared_Click (oArgs)\r\n\t{"));
	// Exercise the inspector's explicit command on a nonvisual event.
	Check(ide->SwitchToFile(file));
	Check(editor->SelectID(timerID));
	{
		HWND tabs = GetDlgItem(editor->Window(),103), list = GetDlgItem(editor->Window(),104);
		NMHDR notification; ZeroMemory(&notification,sizeof(notification)); notification.hwndFrom = tabs; notification.idFrom = 103; notification.code = TCN_SELCHANGE;
		TabCtrl_SetCurSel(tabs,1); SendMessageW(editor->Window(),WM_NOTIFY,103,reinterpret_cast<LPARAM>(&notification));
		INT row = ListRow(list,L"OnTimer"); REQUIRE(row >= 0);
		ListView_SetItemState(list,row,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
		REQUIRE(Enabled(ide,QFD::CreateOpenHandler));
		Check(ide->Exec(NULL,QFD::CreateOpenHandler,0,NULL,NULL));
		Pump();
		REQUIRE(!ide->DesignerActive());
	}
	// A locked project blocks the entire three-file save and preserves dirty data.
	Check(editor->AddComponent(RSTRING_CAST(L"Timer"),&timerID));
	lock = CreateFileW(path.c_wstr(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
	CheckIf(lock == INVALID_HANDLE_VALUE,HrEnsureLastError());
	REQUIRE(FAILED(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL))); REQUIRE(file->IsModified());
	CloseHandle(lock); lock = INVALID_HANDLE_VALUE;
	Check(studio->Exec(NULL,ID_FILE_SAVEALL,0,NULL,NULL));
	Check(studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL));
	Pump();
	Check(studio->OpenProject(path.c_wstr()));
	Pump(); ide = studio->Project();
	Check(ide->FindSource(RSTRING_CAST(L".\\MainForm.quadoo"),&file));
	REQUIRE(file->m_form->metadata->HasField(RSTRING_CAST(L"sourceSync")));
	Check(ide->SetDesignView(TRUE));
	REQUIRE(!RStrLen(file->m_form->sourceDiagnostic));
	hr = S_OK;
Cleanup:
	StudioSource::FailAfter(-1);
	if(FAILED(hr) && studio && studio->Project() == ide && file && file->m_form && RStrLen(file->m_form->sourceDiagnostic)) { Log(file->m_form->sourceDiagnostic.c_wstr()); Log(L"\r\n"); }
	if(lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
	if(studio && studio->Project()) { studio->Project()->CloseProject(FALSE); studio->Exec(NULL,ID_FILE_CLOSEPROJECT,0,NULL,NULL); }
	if(studio) studio->Destroy(); studio.Release(); Pump();
	return hr;
}
