#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Util\TextHelpers.h"
#include "Library\Util\Formatting.h"
#include "Library\Core\StringCore.h"
#include "Library\IconTypes.h"
#include "Published\QuadooParser.h"
#include "ApplicationTarget.h"
#include "ApplicationSettingsDlg.h"
#include "QuadooProject.h"
#include "FormSourceSync.h"
#include "DebugSession.h"

namespace StudioApplication
{
	static INT s_failure = -1;

	VOID FailAfter(INT checkpoint)
	{
		s_failure = checkpoint;
	}

	static HRESULT Checkpoint()
	{
		HRESULT hr = S_OK;
		if(s_failure >= 0 && !s_failure--)
		{
			s_failure = -1;
			hr = E_OUTOFMEMORY;
		}
		return hr;
	}

	static HRESULT Absolute(RSTRING directory, RSTRING path, RSTRING* absolute)
	{
		HRESULT hr;
		PWSTR combined = NULL;
		CheckIf(!path || !RStrLen(path),E_INVALIDARG);
		Check(Formatting::TBuildDirectory(RStrToWide(directory),RStrLen(directory),RStrToWide(path),RStrLen(path),&combined,NULL));
		Check(RStrCreateW(TStrLenAssert(combined),combined,absolute));
	Cleanup:
		SafeDeleteArray(combined);
		return hr;
	}

	static HRESULT LoadText(RSTRING path, CRString& result, UINT codePage = CP_ACP)
	{
		HRESULT hr;
		PWSTR text = NULL;
		INT length;
		Check(Text::LoadFromFile(RStrToWide(path),&text,&length,codePage));
		CheckIf(length > 8 * 1024 * 1024,E_INVALIDARG);
		Check(RStrCreateW(length,text,&result));
	Cleanup:
		SafeDeleteArray(text);
		return hr;
	}

	struct SourceSnapshot
	{
		CRString path, text;
		UINT codePage;
	};

	class CCompileStatus : public IQuadooCompilerStatus
	{
		TArray<SourceSnapshot*> m_inputs;
		CRString m_directory;
	public:
		CRString diagnostic;
		CRString virtualPath;
		HRESULT inputError;

		CCompileStatus(RSTRING directory) : m_directory(directory), inputError(S_OK)
		{
		}

		VOID SetDirectory(RSTRING directory)
		{
			m_directory = directory;
		}

		~CCompileStatus()
		{
			m_inputs.DeleteAll();
		}

		HRESULT Capture(RSTRING path, UINT codePage = CP_ACP)
		{
			HRESULT hr = S_OK;
			SourceSnapshot* snapshot = NULL;
			for(sysint n = 0; n < m_inputs.Length(); n++)
			{
				if(!lstrcmpiW(m_inputs[n]->path.c_wstr(),RStrToWide(path)))
				{
					goto Cleanup;
				}
			}
			snapshot = __new SourceSnapshot;
			CheckAlloc(snapshot);
			snapshot->path = path;
			snapshot->codePage = codePage;
			Check(LoadText(path,snapshot->text,codePage));
			Check(m_inputs.Append(snapshot));
			snapshot = NULL;
		Cleanup:
			__delete snapshot;
			return hr;
		}

		HRESULT VerifyInputs()
		{
			HRESULT hr;
			Check(inputError);
			for(sysint n = 0; n < m_inputs.Length(); n++)
			{
				CRString current;
				Check(LoadText(m_inputs[n]->path,current,m_inputs[n]->codePage));
				CheckIf(!(current == static_cast<RSTRING>(m_inputs[n]->text)),HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
			}
		Cleanup:
			return hr;
		}

		VOID STDMETHODCALLTYPE OnCompilerAddFile(PCWSTR path, INT length)
		{
			CRString name;
			HRESULT hr = RStrCreateW(length,path,&name);
			if(SUCCEEDED(hr) && name == static_cast<RSTRING>(virtualPath))
			{
				return;
			}
			if(SUCCEEDED(hr))
			{
				hr = Capture(name);
			}
			if(FAILED(hr) && SUCCEEDED(inputError))
			{
				inputError = hr;
			}
		}

		VOID STDMETHODCALLTYPE OnCompilerStatus(PCWSTR)
		{
		}

		VOID STDMETHODCALLTYPE OnCompilerError(HRESULT error, INT line, PCWSTR file, PCWSTR detail)
		{
			if(!RStrLen(diagnostic))
			{
				RStrFormatW(&diagnostic,L"%ls(%d): %ls (0x%.8x)",file,line,detail,error);
			}
		}

		STDMETHOD(OnCompilerResolvePath)(PCWSTR path, PWSTR absolute, INT maximum)
		{
			HRESULT hr;
			CRString combined;
			Check(Absolute(m_directory,RSTRING_CAST(path),&combined));
			Check(RStrCopyToW(combined,maximum,absolute,NULL));
		Cleanup:
			return hr;
		}
	};

	struct UnitDefinition
	{
		CRString name, kind;
		TStackRef<IJSONObject> definition;
	};

	static HRESULT Embed(HANDLE update, PCWSTR type, PCWSTR name, IJSONObject* object)
	{
		HRESULT hr;
		CMemoryStream json;
		Check(JSONSerializeObject(object,&json));
		CheckIf(json.DataRemaining() > AQVM::MaximumResourceBytes,E_INVALIDARG);
		CheckIf(!UpdateResourceW(update,type,name,0,const_cast<PBYTE>(json.GetReadPtr()),json.DataRemaining()),HrEnsureLastError());
	Cleanup:
		return hr;
	}

	static HRESULT FilesEqual(PCWSTR first, PCWSTR second, BOOL* equal)
	{
		HRESULT hr;
		HANDLE a = INVALID_HANDLE_VALUE, b = INVALID_HANDLE_VALUE;
		LARGE_INTEGER sizeA, sizeB;
		BYTE bufferA[16384], bufferB[16384];
		DWORD readA, readB;
		*equal = FALSE;
		a = CreateFileW(first,GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
		CheckIf(a == INVALID_HANDLE_VALUE,HrEnsureLastError());
		b = CreateFileW(second,GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
		if(b == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			hr = S_OK;
			goto Cleanup;
		}
		CheckIf(b == INVALID_HANDLE_VALUE,HrEnsureLastError());
		CheckIf(!GetFileSizeEx(a,&sizeA) || !GetFileSizeEx(b,&sizeB),HrEnsureLastError());
		if(sizeA.QuadPart != sizeB.QuadPart)
		{
			hr = S_OK;
			goto Cleanup;
		}
		do
		{
			CheckIf(!ReadFile(a,bufferA,sizeof(bufferA),&readA,NULL) || !ReadFile(b,bufferB,sizeof(bufferB),&readB,NULL),HrEnsureLastError());
			if(readA != readB || memcmp(bufferA,bufferB,readA))
			{
				hr = S_OK;
				goto Cleanup;
			}
		} while(readA);
		*equal = TRUE;
		hr = S_OK;
	Cleanup:
		SafeCloseFileHandle(a);
		SafeCloseFileHandle(b);
		return hr;
	}

	static HRESULT ValidateTemplate(PCWSTR path)
	{
		HRESULT hr;
		HANDLE file = INVALID_HANDLE_VALUE;
		HMODULE image = NULL;
		IMAGE_DOS_HEADER dos;
		IMAGE_FILE_HEADER header;
		DWORD signature, read;
		LARGE_INTEGER position, size;
		HRSRC resource;
		HGLOBAL data;
		const DWORD* marker;
		file = CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
		CheckIf(file == INVALID_HANDLE_VALUE,HrEnsureLastError());
		CheckIf(!GetFileSizeEx(file,&size),HrEnsureLastError());
		CheckIf(!ReadFile(file,&dos,sizeof(dos),&read,NULL),HrEnsureLastError());
		CheckIf(read != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < sizeof(dos) || dos.e_lfanew > size.QuadPart - sizeof(signature) - sizeof(header),HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT));
		position.QuadPart = dos.e_lfanew;
		CheckIf(!SetFilePointerEx(file,position,NULL,FILE_BEGIN),HrEnsureLastError());
		CheckIf(!ReadFile(file,&signature,sizeof(signature),&read,NULL),HrEnsureLastError());
		CheckIf(read != sizeof(signature) || signature != IMAGE_NT_SIGNATURE,HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT));
		CheckIf(!ReadFile(file,&header,sizeof(header),&read,NULL),HrEnsureLastError());
#ifdef _WIN64
		CheckIf(read != sizeof(header) || header.Machine != IMAGE_FILE_MACHINE_AMD64,HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT));
#else
		CheckIf(read != sizeof(header) || header.Machine != IMAGE_FILE_MACHINE_I386,HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT));
#endif
		image = LoadLibraryExW(path,NULL,LOAD_LIBRARY_AS_DATAFILE);
		CheckIf(!image,HrEnsureLastError());
		resource = FindResourceW(image,MAKEINTRESOURCEW(AQVM::TemplateResource),RT_RCDATA);
		CheckIf(!resource || SizeofResource(image,resource) != 2 * sizeof(DWORD),HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT));
		data = LoadResource(image,resource);
		CheckIf(!data,HrEnsureLastError());
		marker = static_cast<const DWORD*>(LockResource(data));
		CheckIf(!marker || marker[0] != AQVM::TemplateSignature || marker[1] != AQVM::CatalogVersion,HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
		hr = S_OK;
	Cleanup:
		if(image)
		{
			FreeLibrary(image);
		}
		SafeCloseFileHandle(file);
		return hr;
	}

	struct OutputFile
	{
		CRString target, temporary, backup;
		BOOL backed, published;
		HANDLE previousLock, stagedLock;
		OutputFile() : backed(FALSE), published(FALSE), previousLock(INVALID_HANDLE_VALUE), stagedLock(INVALID_HANDLE_VALUE)
		{
		}
		~OutputFile()
		{
			SafeCloseFileHandle(previousLock);
			SafeCloseFileHandle(stagedLock);
			if(RStrLen(temporary))
			{
				DeleteFileW(temporary.c_wstr());
			}
			// A failed rollback deliberately retains its backup for recovery.
		}
	};

	static HRESULT PrepareCopy(RSTRING directory, PCWSTR source, RSTRING target, TArray<OutputFile*>& outputs)
	{
		HRESULT hr;
		OutputFile* file = __new OutputFile;
		WCHAR temporary[MAX_PATH] = {0};
		CheckAlloc(file);
		file->target = target;
		CheckIf(!GetTempFileNameW(RStrToWide(directory),L"AQT",0,temporary),HrEnsureLastError());
		Check(RStrCreateW(TStrLenAssert(temporary),temporary,&file->temporary));
		CheckIf(!CopyFileW(source,temporary,FALSE),HrEnsureLastError());
		Check(outputs.Append(file));
		file = NULL;
	Cleanup:
		if(file && !RStrLen(file->temporary) && temporary[0])
		{
			DeleteFileW(temporary);
		}
		__delete file;
		return hr;
	}

	static HRESULT Publish(RSTRING directory, TArray<OutputFile*>& outputs, CRString& diagnostic)
	{
		HRESULT hr;
		// Writable handles prevent image activation until publication completes,
		// and reject already running output images before replacing anything.
		for(sysint n = 0; n < outputs.Length(); n++)
		{
			OutputFile* file = outputs[n];
			file->stagedLock = CreateFileW(file->temporary.c_wstr(),GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
			CheckIf(file->stagedLock == INVALID_HANDLE_VALUE,HrEnsureLastError());
			if(GetFileAttributesW(file->target.c_wstr()) != INVALID_FILE_ATTRIBUTES)
			{
				file->previousLock = CreateFileW(file->target.c_wstr(),GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
				CheckIf(file->previousLock == INVALID_HANDLE_VALUE,HrEnsureLastError());
			}
		}
		for(sysint n = 0; n < outputs.Length(); n++)
		{
			OutputFile* file = outputs[n];
			if(GetFileAttributesW(file->target.c_wstr()) != INVALID_FILE_ATTRIBUTES)
			{
				WCHAR backup[MAX_PATH];
				CheckIf(!GetTempFileNameW(RStrToWide(directory),L"AQB",0,backup),HrEnsureLastError());
				hr = RStrCreateW(TStrLenAssert(backup),backup,&file->backup);
				if(FAILED(hr))
				{
					DeleteFileW(backup);
				}
				Check(hr);
				if(!MoveFileExW(file->target.c_wstr(),backup,MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
				{
					hr = HrEnsureLastError();
					DeleteFileW(backup);
					Check(hr);
				}
				file->backed = TRUE;
			}
			CheckIf(!MoveFileExW(file->temporary.c_wstr(),file->target.c_wstr(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),HrEnsureLastError());
			file->published = TRUE;
			Check(Checkpoint());
		}
		hr = S_OK;
	Cleanup:
		for(sysint n = outputs.Length(); n > 0; n--)
		{
			OutputFile* file = outputs[n - 1];
			if(FAILED(hr))
			{
				// MoveFileEx cannot replace a destination with an outstanding
				// writable handle. Keep the old image guarded while restoring it.
				SafeCloseFileHandle(file->stagedLock);
				if(file->backed)
				{
					if(!MoveFileExW(file->backup.c_wstr(),file->target.c_wstr(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
					{
						diagnostic = static_cast<RSTRING>(NULL);
						RStrFormatW(&diagnostic,L"Output rollback failed. Recover %ls from %ls.",file->target.c_wstr(),file->backup.c_wstr());
					}
				}
				else if(file->published)
				{
					DeleteFileW(file->target.c_wstr());
				}
			}
			else if(file->backed)
			{
				DeleteFileW(file->backup.c_wstr());
			}
		}
		return hr;
	}

	static HRESULT EmbedApplicationIcon(HANDLE update, CMemoryStream& bytes)
	{
		HRESULT hr = S_OK;
		ICONHEAD header;
		CMemoryStream group;
		ULONG written;
		if(bytes.DataRemaining())
		{
			// DecodeApplicationIcon has already validated every ICO image and offset.
			CopyMemory(&header,bytes.GetReadPtr(),sizeof(header));
			Check(group.Write(&header,sizeof(header),&written));
			for(UINT n = 0; n < header.wCount; n++)
			{
				ICONDIRENTRY entry;
				WORD identity = static_cast<WORD>(n + 1);
				CopyMemory(&entry,bytes.GetReadPtr() + sizeof(header) + n * sizeof(entry),sizeof(entry));
				CheckIf(!UpdateResourceW(update,RT_ICON,MAKEINTRESOURCEW(identity),0,const_cast<PBYTE>(bytes.GetReadPtr() + entry.dwImageOffset),entry.dwBytesInRes),HrEnsureLastError());
				// A group entry replaces the file's DWORD offset with a WORD ID.
				Check(group.Write(&entry,FIELD_OFFSET(ICONDIRENTRY,dwImageOffset),&written));
				Check(group.Write(&identity,sizeof(identity),&written));
			}
			CheckIf(!UpdateResourceW(update,RT_GROUP_ICON,MAKEINTRESOURCEW(1),0,const_cast<PBYTE>(group.GetReadPtr()),group.DataRemaining()),HrEnsureLastError());
		}
	Cleanup:
		return hr;
	}

	static PCWSTR const RuntimeDependencies[] = {L"QuadooForm.dll",L"QuadooVM.dll",L"QuadooParser.dll",L"JSON.dll",L"SimbeyCore.dll"};

	static HRESULT AddModuleName(RSTRING name, PCWSTR executableName, IJSONArray* deployed, CRString& diagnostic)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		BOOL duplicate = !lstrcmpiW(RStrToWide(name),executableName);
		for(UINT n = 0; n < ARRAYSIZE(RuntimeDependencies); n++)
		{
			duplicate |= !lstrcmpiW(RStrToWide(name),RuntimeDependencies[n]);
		}
		for(sysint n = 0; n < deployed->Count(); n++)
		{
			CRString previous;
			Check(deployed->GetString(n,&previous));
			duplicate |= !lstrcmpiW(RStrToWide(name),previous.c_wstr());
		}
		if(duplicate)
		{
			Check(RStrFormatW(&diagnostic,L"Module file name '%r' conflicts with another module or runtime file.",name));
			Check(HRESULT_FROM_WIN32(ERROR_DUP_NAME));
		}
		Check(JSONCreateString(name,&value));
		Check(deployed->Add(value));
	Cleanup:
		return hr;
	}

	static HRESULT PrepareModules(IJSONArray* modules, RSTRING directory, RSTRING outputDirectory, RSTRING executable,
		TArray<OutputFile*>& outputs, IJSONArray* deployed, CRString& diagnostic)
	{
		HRESULT hr = S_OK;
		for(sysint n = 0; n < modules->Count(); n++)
		{
			CRString path;
			CRString source;
			CRString name;
			CRString target;
			BOOL equal;
			DWORD attributes;
			Check(modules->GetString(n,&path));
			hr = ResolveModulePath(directory,path,source,name);
			if(FAILED(hr))
			{
				Check(RStrFormatW(&diagnostic,L"Invalid module path: %r.",static_cast<RSTRING>(path)));
				Check(E_INVALIDARG);
			}
			Check(RStrFormatW(&target,L"%r\\%r",outputDirectory,static_cast<RSTRING>(name)));
			Check(AddModuleName(name,TStrRChr(RStrToWide(executable),L'\\') + 1,deployed,diagnostic));
			attributes = GetFileAttributesW(source.c_wstr());
			if(attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				Check(RStrFormatW(&diagnostic,L"Module dependency could not be read: %r.",static_cast<RSTRING>(source)));
				Check(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
			}
			hr = FilesEqual(source.c_wstr(),target.c_wstr(),&equal);
			if(SUCCEEDED(hr) && !equal)
			{
				hr = PrepareCopy(outputDirectory,source.c_wstr(),target,outputs);
			}
			if(FAILED(hr))
			{
				RStrFormatW(&diagnostic,L"Could not prepare module dependency '%r' (0x%.8x).",static_cast<RSTRING>(source),hr);
				Check(hr);
			}
		}
	Cleanup:
		return hr;
	}

	class CApplicationInputs
	{
	public:
		TStackRef<IJSONObject> catalog;
		TStackRef<IJSONArray> modules;
		TArray<UnitDefinition*> definitions;
		CRString directory, fileTag;
		CCompileStatus status;

		CApplicationInputs() : status(NULL)
		{
		}

		~CApplicationInputs()
		{
			definitions.DeleteAll();
		}

		HRESULT Compile(IJSONObject* project, RSTRING projectPath, DWORD flags, CMemoryStream& byteCode,
			CMemoryStream* debug, IQuadooDebugTree** debugTree)
		{
			HRESULT hr;
			TStackRef<IJSONArray> files, units;
			TStackRef<IJSONValue> value;
			TArray<RSTRING> included;
			CRString startupClass;
			CMemoryStream startup;
			UINT defaults = 0;
			PCWSTR path = RStrToWide(projectPath), leaf;
			ULONG written;

			CheckIf(!project || !path,E_INVALIDARG);
			leaf = TStrRChr(path,L'\\');
			CheckIf(!leaf,E_INVALIDARG);
			Check(RStrCreateW(static_cast<INT>(leaf - path),path,&directory));
			status.SetDirectory(directory);
			Check(status.Capture(projectPath));
			Check(ReadModuleSettings(project,CurrentModuleArchitecture(),&modules));
			Check(project->FindNonNullValueW(L"files",&value));
			Check(value->GetArray(&files));
			value.Release();
			CheckIf(files->Count() < 1 || files->Count() > AQVM::MaximumUnits,E_INVALIDARG);
			Check(JSONCreateObject(&catalog));
			Check(AQVM::PutInt(catalog,L"schemaVersion",AQVM::CatalogVersion));
			Check(JSONCreateArray(&units));
			for(sysint n = 0; n < files->Count(); n++)
			{
				TStackRef<IJSONObject> file, metadata, entry, definition;
				TStackRef<IJSONValue> field;
				CRString relative, sourcePath, kind, className, interfaceName, identity;
				BOOL duplicate = FALSE;
				bool isDefault = false;
				Check(files->GetObject(n,&file));
				Check(AQVM::StringField(file,L"path",&relative));
				Check(Absolute(directory,relative,&sourcePath));
				if(file->HasField(RSTRING_CAST(L"default")))
				{
					Check(file->FindNonNullValueW(L"default",&field));
					Check(field->GetBoolean(&isDefault));
					field.Release();
				}
				for(sysint p = 0; p < included.Length(); p++)
				{
					if(!lstrcmpiW(RStrToWide(included[p]),sourcePath.c_wstr()))
					{
						duplicate = TRUE;
					}
				}
				CheckIf(duplicate,HRESULT_FROM_WIN32(ERROR_DUP_NAME));
				Check(status.Capture(sourcePath));
				Check(included.Append(sourcePath));
				RStrAddRef(sourcePath);
				{
					CRString include;
					CheckIf(TStrChr(sourcePath.c_wstr(),L'"') || TStrChr(sourcePath.c_wstr(),L'\r') || TStrChr(sourcePath.c_wstr(),L'\n'),E_INVALIDARG);
					Check(RStrFormatW(&include,L"#include \"%ls\"\r\n",sourcePath.c_wstr()));
					Check(startup.TWrite(include.c_wstr(),RStrLen(include),&written));
				}
				if(!file->HasField(RSTRING_CAST(L"designer")))
				{
					CheckIf(isDefault,E_INVALIDARG);
					continue;
				}
				Check(file->FindNonNullValueW(L"designer",&field));
				Check(field->GetObject(&metadata));
				field.Release();
				Check(AQVM::StringField(metadata,L"class",&className));
				Check(AQVM::StringField(metadata,L"kind",&kind));
				Check(AQVM::StringField(metadata,L"unitId",&identity));
				Check(AQVM::StringField(metadata,L"eventsInterface",&interfaceName));
				Check(StudioSource::ValidateIdentifier(className));
				Check(StudioSource::ValidateIdentifier(interfaceName));
				{
					CRString expectedName, expectedInterface;
					Check(CStudioFormUnit::SourceName(sourcePath,&expectedName));
					Check(RStrFormatW(&expectedInterface,L"I%ls",expectedName.c_wstr()));
					CheckIf(!(expectedName == static_cast<RSTRING>(className)) || !(expectedInterface == static_cast<RSTRING>(interfaceName)),E_INVALIDARG);
				}
				relative = static_cast<RSTRING>(NULL);
				Check(AQVM::StringField(metadata,L"definition",&relative));
				{
					CRString definitionPath, text, source;
					StudioSource::Result synchronized;
					GUID unitID;
					Check(Absolute(directory,relative,&definitionPath));
					Check(status.Capture(definitionPath,CP_UTF8));
					Check(LoadText(definitionPath,text,CP_UTF8));
					Check(AQVM::ParseObject(text.c_wstr(),RStrLen(text),&definition));
					Check(LoadText(sourcePath,source));
					Check(CLSIDFromString(const_cast<PWSTR>(identity.c_wstr()),&unitID));
					Check(StudioSource::Prepare(source,className,unitID,metadata,definition,NULL,synchronized));
					if(synchronized.edits.Length())
					{
						status.diagnostic = RSTRING_CAST(L"Synchronize Script Declarations and save the Form/Frame sources before running.");
						Check(E_FAIL);
					}
				}
				{
					CRString nativeType;
					Check(AQVM::StringField(definition,L"type",&nativeType));
					CheckIf(lstrcmpW(nativeType.c_wstr(),kind == RSTRING_CAST(L"form") ? L"Form" : L"Frame"),E_INVALIDARG);
				}
				Check(JSONCreateObject(&entry));
				Check(AQVM::PutString(entry,L"class",className));
				Check(AQVM::PutString(entry,L"kind",kind));
				Check(AQVM::PutString(entry,L"unitId",identity));
				Check(AQVM::PutString(entry,L"eventsInterface",interfaceName));
				Check(JSONWrapObject(entry,&field));
				Check(units->Add(field));
				{
					UnitDefinition* saved = __new UnitDefinition;
					CheckAlloc(saved);
					saved->name = className;
					saved->kind = kind;
					saved->definition = definition;
					hr = definitions.Append(saved);
					if(FAILED(hr))
					{
						__delete saved;
					}
					Check(hr);
				}
				if(isDefault)
				{
					CheckIf(!(kind == RSTRING_CAST(L"form")),E_INVALIDARG);
					defaults++;
					startupClass = className;
				}
			}
			CheckIf(defaults != 1,E_INVALIDARG);
			Check(AQVM::PutString(catalog,L"startupClass",startupClass));
			Check(JSONWrapArray(units,&value));
			Check(catalog->AddValueW(L"units",value));
			Check(AQVM::ValidateCatalog(catalog));
			{
				CRString startupSourceText;
				Check(AQVM::GenerateStartup(startupClass, &startupSourceText));
				Check(startup.TWrite(startupSourceText.c_wstr(),RStrLen(startupSourceText),&written));
			}
			Check(RStrFormatW(&fileTag,L"%ls\\[Application startup]",directory.c_wstr()));
			status.virtualPath = fileTag;
			Check(QuadooParseTextToStream(fileTag.c_wstr(),RStrLen(fileTag),startup.TGetReadPtr<WCHAR>(),static_cast<INT>(startup.TDataRemaining<WCHAR>()),flags,&byteCode,debug,&status,debugTree));
			CheckIf(!byteCode.DataRemaining() || byteCode.DataRemaining() > AQVM::MaximumResourceBytes,E_INVALIDARG);
			Check(status.VerifyInputs());
		Cleanup:
			for(sysint n = 0; n < included.Length(); n++)
			{
				RStrRelease(included[n]);
			}
			return hr;
		}
	};

	CApplicationDebugData::~CApplicationDebugData()
	{
		for(sysint n = 0; n < m_dependencies.Length(); n++)
		{
			RStrRelease(m_dependencies[n]);
		}
	}

	HRESULT CApplicationDebugData::AddDependency(RSTRING path)
	{
		HRESULT hr;
		Check(m_dependencies.Append(path));
		RStrAddRef(path);
	Cleanup:
		return hr;
	}

	static HRESULT CopyMissingDependency(PCWSTR source, PCWSTR target, PCWSTR directory)
	{
		HRESULT hr = S_OK;
		WCHAR temporary[MAX_PATH] = {};
		DWORD attributes = GetFileAttributesW(target);
		if(attributes != INVALID_FILE_ATTRIBUTES)
		{
			CheckIf(attributes & FILE_ATTRIBUTE_DIRECTORY,HRESULT_FROM_WIN32(ERROR_DIRECTORY));
			goto Cleanup;
		}
		CheckIf(GetLastError() != ERROR_FILE_NOT_FOUND,HrEnsureLastError());
		CheckIf(!lstrcmpiW(source,target),HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
		CheckIfGetLastError(!GetTempFileNameW(directory,L"aqd",0,temporary));
		CheckIfGetLastError(!CopyFileW(source,temporary,FALSE));
		if(!MoveFileW(temporary,target))
		{
			hr = HrEnsureLastError();
			attributes = GetFileAttributesW(target);
			Check(hr = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY) ? S_OK : hr);
		}
		else
		{
			temporary[0] = 0;
		}
	Cleanup:
		if(temporary[0])
		{
			SetFileAttributesW(temporary,FILE_ATTRIBUTE_NORMAL);
			DeleteFileW(temporary);
		}
		return hr;
	}

	HRESULT CApplicationDebugData::PrepareDependencies(PCWSTR debugger, IStartDebuggerStatus* status, CRString& diagnostic)
	{
		HRESULT hr;
		PCWSTR leaf = TStrRChr(debugger,L'\\');
		CRString directory, source, target;
		WCHAR studioPath[MAX_PATH];
		DWORD length;
		CheckIf(!leaf,E_INVALIDARG);
		Check(RStrCreateW(static_cast<INT>(leaf - debugger),debugger,&directory));
		length = GetModuleFileNameW(NULL,studioPath,ARRAYSIZE(studioPath));
		CheckIf(!length || length >= ARRAYSIZE(studioPath),HrEnsureLastError());
		{
			PWSTR end = const_cast<PWSTR>(TStrRChr(studioPath,L'\\'));
			CheckIf(!end,E_INVALIDARG);
			*end = 0;
		}
		for(UINT n = 0; n < ARRAYSIZE(RuntimeDependencies); n++)
		{
			CheckIfIgnore(status->CheckAbortFlag(),E_ABORT);
			source = static_cast<RSTRING>(NULL);
			target = static_cast<RSTRING>(NULL);
			Check(RStrFormatW(&source,L"%ls\\%ls",studioPath,RuntimeDependencies[n]));
			Check(RStrFormatW(&target,L"%ls\\%ls",directory.c_wstr(),RuntimeDependencies[n]));
			Check(CopyMissingDependency(source.c_wstr(),target.c_wstr(),directory.c_wstr()));
		}
		for(sysint n = 0; n < m_dependencies.Length(); n++)
		{
			CheckIfIgnore(status->CheckAbortFlag(),E_ABORT);
			source = m_dependencies[n];
			target = static_cast<RSTRING>(NULL);
			leaf = TStrRChr(source.c_wstr(),L'\\');
			CheckIf(!leaf,E_INVALIDARG);
			Check(RStrFormatW(&target,L"%ls%ls",directory.c_wstr(),leaf));
			Check(CopyMissingDependency(source.c_wstr(),target.c_wstr(),directory.c_wstr()));
		}
	Cleanup:
		if(FAILED(hr) && hr != E_ABORT)
		{
			diagnostic = static_cast<RSTRING>(NULL);
			RStrFormatW(&diagnostic,L"Could not prepare a debugger dependency (0x%.8x).\r\nSource: %ls\r\nDestination: %ls",
				hr,source.Length() ? source.c_wstr() : L"",target.Length() ? target.c_wstr() : L"");
		}
		return hr;
	}

	static HRESULT AppendDebugRecord(CMemoryStream& bundle, IJSONObject* object, DWORD count, RSTRING key = NULL)
	{
		HRESULT hr;
		CMemoryStream wide;
		INT jsonBytes, keyBytes = 0;
		DWORD payload;
		PBYTE output;
		ULONG written;
		Check(JSONSerializeObject(object,&wide));
		jsonBytes = WideCharToMultiByte(CP_UTF8,0,wide.TGetReadPtr<WCHAR>(),wide.TDataRemaining<WCHAR>(),NULL,0,NULL,NULL);
		CheckIfGetLastError(!jsonBytes);
		CheckIf(static_cast<DWORD>(jsonBytes) > QUADOO_APP_DEBUG_MAX_JSON,HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
		if(key)
		{
			keyBytes = WideCharToMultiByte(CP_UTF8,0,RStrToWide(key),RStrLen(key),NULL,0,NULL,NULL);
			CheckIfGetLastError(!keyBytes);
			CheckIf(static_cast<DWORD>(keyBytes) > QUADOO_APP_DEBUG_MAX_JSON,HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
		}
		payload = sizeof(DWORD) + keyBytes + jsonBytes;
		CheckIf(payload > QUADOO_DEBUG_MAX_MESSAGE || bundle.DataRemaining() > QUADOO_APP_DEBUG_MAX_BUNDLE - payload - sizeof(DWORD),
			HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE));
		Check(bundle.TWrite(&payload,1,&written));
		Check(bundle.WriteAdvance(&output,payload));
		DWORD prefix = key ? static_cast<DWORD>(keyBytes) : count;
		CopyMemory(output,&prefix,sizeof(prefix));
		output += sizeof(prefix);
		if(key)
		{
			CheckIfGetLastError(keyBytes != WideCharToMultiByte(CP_UTF8,0,RStrToWide(key),RStrLen(key),
				reinterpret_cast<LPSTR>(output),keyBytes,NULL,NULL));
			output += keyBytes;
		}
		CheckIfGetLastError(jsonBytes != WideCharToMultiByte(CP_UTF8,0,wide.TGetReadPtr<WCHAR>(),wide.TDataRemaining<WCHAR>(),
			reinterpret_cast<LPSTR>(output),jsonBytes,NULL,NULL));
	Cleanup:
		return hr;
	}

	HRESULT CompileDebug(IJSONObject* project, RSTRING projectPath, CApplicationDebugData& data,
		CMemoryStream& byteCode, CMemoryStream& debug, IQuadooDebugTree** tree, CRString& diagnostic)
	{
		HRESULT hr;
		CApplicationInputs inputs;
		TStackRef<IJSONArray> deployed;
		TStackRef<IJSONValue> value;
		Check(inputs.Compile(project,projectPath,QUADOO_COMPILE_LINE_NUMBER_MAP | QUADOO_COMPILE_FOR_DEBUGGING,byteCode,&debug,tree));
		Check(JSONCreateArray(&deployed));
		for(sysint n = 0; n < inputs.modules->Count(); n++)
		{
			CRString path, source, name;
			Check(inputs.modules->GetString(n,&path));
			hr = ResolveModulePath(inputs.directory,path,source,name);
			if(FAILED(hr))
			{
				RStrFormatW(&diagnostic,L"Invalid module path: %r.",static_cast<RSTRING>(path));
				Check(hr);
			}
			Check(AddModuleName(name,L"AQVMDbg.exe",deployed,diagnostic));
			Check(data.AddDependency(source));
		}
		Check(JSONWrapArray(deployed,&value));
		Check(inputs.catalog->AddValueW(L"modules",value));
		Check(AppendDebugRecord(data.bundle,inputs.catalog,static_cast<DWORD>(inputs.definitions.Length())));
		for(sysint n = 0; n < inputs.definitions.Length(); n++)
		{
			UnitDefinition* definition = inputs.definitions[n];
			Check(AppendDebugRecord(data.bundle,definition->definition,0,definition->name));
		}
		Check(inputs.status.VerifyInputs());
		data.program = inputs.fileTag;
	Cleanup:
		if(FAILED(hr) && !diagnostic.Length())
		{
			if(inputs.status.diagnostic.Length())
				diagnostic = inputs.status.diagnostic;
			else
				RStrFormatW(&diagnostic,L"Could not prepare the Application debug snapshot (0x%.8x).",hr);
		}
		return hr;
	}


	HRESULT Build(IJSONObject* project, RSTRING projectPath, RSTRING runtimeDirectory,
		CRString& executable, CRString& diagnostic)
	{
		HRESULT hr;
		CApplicationInputs inputs;
		TStackRef<IJSONArray> deployed;
		TStackRef<IJSONValue> value;
		TArray<OutputFile*> outputs;
		CRString templatePath, target, outputDirectory, stem;
		CMemoryStream byteCode;
		CMemoryStream icon;
		HANDLE update = NULL;
		PCWSTR path = RStrToWide(projectPath), leaf, extension;
		executable = static_cast<RSTRING>(NULL);
		diagnostic = static_cast<RSTRING>(NULL);
		CheckIf(!project || !path || !RStrToWide(runtimeDirectory),E_INVALIDARG);
		leaf = TStrRChr(path,L'\\');
		CheckIf(!leaf,E_INVALIDARG);
		Check(DecodeApplicationIcon(project,icon));
		Check(JSONCreateArray(&deployed));
		extension = TStrRChr(leaf,L'.');
		CheckIf(!extension || extension <= leaf + 1,E_INVALIDARG);
		Check(RStrCreateW(static_cast<INT>(extension - leaf - 1),leaf + 1,&stem));
		Check(inputs.Compile(project,projectPath,QUADOO_COMPILE_LINE_NUMBER_MAP,byteCode,NULL,NULL));
		Check(Checkpoint());
		Check(RStrFormatW(&templatePath,L"%ls\\AQVMT.bin",RStrToWide(runtimeDirectory)));
		hr = ValidateTemplate(templatePath.c_wstr());
		if(FAILED(hr))
		{
			RStrFormatW(&diagnostic,L"Could not read or validate the Application executable template (0x%.8x):\r\n%ls\r\n\r\nPlace the matching AQVMT.bin beside QuadooStudio.exe.",hr,templatePath.c_wstr());
			Check(hr);
		}
		Check(RStrFormatW(&outputDirectory,L"%ls\\bin",inputs.directory.c_wstr()));
		CheckIf(!CreateDirectoryW(outputDirectory.c_wstr(),NULL) && GetLastError() != ERROR_ALREADY_EXISTS,HrEnsureLastError());
		{
			CRString architecture;
#ifdef _WIN64
			Check(RStrFormatW(&architecture,L"%ls\\x64",outputDirectory.c_wstr()));
#else
			Check(RStrFormatW(&architecture,L"%ls\\Win32",outputDirectory.c_wstr()));
#endif
			outputDirectory = architecture;
		}
		CheckIf(!CreateDirectoryW(outputDirectory.c_wstr(),NULL) && GetLastError() != ERROR_ALREADY_EXISTS,HrEnsureLastError());
		Check(RStrFormatW(&target,L"%ls\\%ls.exe",outputDirectory.c_wstr(),stem.c_wstr()));
		{
			for(UINT n = 0; n < ARRAYSIZE(RuntimeDependencies); n++)
			{
				CRString source, destination;
				BOOL equal;
				Check(RStrFormatW(&source,L"%ls\\%ls",RStrToWide(runtimeDirectory),RuntimeDependencies[n]));
				Check(RStrFormatW(&destination,L"%ls\\%ls",outputDirectory.c_wstr(),RuntimeDependencies[n]));
				hr = FilesEqual(source.c_wstr(),destination.c_wstr(),&equal);
				if(SUCCEEDED(hr) && !equal)
				{
					hr = PrepareCopy(outputDirectory,source.c_wstr(),destination,outputs);
				}
				if(FAILED(hr))
				{
					RStrFormatW(&diagnostic,L"Could not prepare an Application runtime dependency (0x%.8x).\r\n\r\nSource: %ls\r\nOutput: %ls\r\n\r\nKeep the matching runtime DLLs beside QuadooStudio.exe and ensure the output directory is writable.",hr,source.c_wstr(),destination.c_wstr());
					Check(hr);
				}
			}
		}
		Check(PrepareModules(inputs.modules,inputs.directory,outputDirectory,target,outputs,deployed,diagnostic));
		value.Release();
		Check(JSONWrapArray(deployed,&value));
		Check(inputs.catalog->AddValueW(L"modules",value));
		Check(PrepareCopy(outputDirectory,templatePath.c_wstr(),target,outputs));
		update = BeginUpdateResourceW(outputs[outputs.Length() - 1]->temporary.c_wstr(),FALSE);
		CheckIf(!update,HrEnsureLastError());
		CheckIf(!UpdateResourceW(update,L"QBC",MAKEINTRESOURCEW(AQVM::ByteCodeResource),0,const_cast<PBYTE>(byteCode.GetReadPtr()),byteCode.DataRemaining()),HrEnsureLastError());
		Check(Embed(update,AQVM::CatalogType,MAKEINTRESOURCEW(AQVM::CatalogResource),inputs.catalog));
		Check(EmbedApplicationIcon(update,icon));
		for(sysint n = 0; n < inputs.definitions.Length(); n++)
		{
			UnitDefinition* definition = inputs.definitions[n];
			CRString resourceName;
			Check(AQVM::ClassResourceName(definition->name,&resourceName));
			Check(Embed(update,definition->kind == RSTRING_CAST(L"form") ? AQVM::FormType : AQVM::FrameType,resourceName.c_wstr(),definition->definition));
		}
		Check(Checkpoint());
		{
			BOOL ended = EndUpdateResourceW(update,FALSE);
			update = NULL;
			CheckIf(!ended,HrEnsureLastError());
		}
		Check(Checkpoint());
		Check(inputs.status.VerifyInputs());
		Check(Publish(outputDirectory,outputs,diagnostic));
		executable = target;
		hr = S_OK;
	Cleanup:
		if(update)
		{
			EndUpdateResourceW(update,TRUE);
		}
		if(FAILED(hr) && !RStrLen(diagnostic))
		{
			if(RStrLen(inputs.status.diagnostic))
			{
				diagnostic = inputs.status.diagnostic;
			}
			else
			{
				RStrFormatW(&diagnostic,L"Application build failed (0x%.8x). Check Default form, saved declarations, resources, runtime files and output access.",hr);
			}
		}
		outputs.DeleteAll();
		return hr;
	}
}

HRESULT CQuadooProject::SaveApplicationInputs()
{
	HRESULT hr;
	CheckIf(!IsApplication(),E_INVALIDARG);
	if(ActiveDesigner())
	{
		Check(ActiveDesigner()->CommitPropertyEdit());
	}
	for(sysint n = 0; n < m_mapFiles.Length(); n++)
	{
		CProjectFile* file = static_cast<CProjectFile*>(*m_mapFiles.GetValuePtr(n));
		if(file->m_form)
		{
			Check(SynchronizeFormSource(file));
		}
	}
	Check(SaveAll());
Cleanup:
	return hr;
}

HRESULT CQuadooProject::CompileApplicationDebug(StudioApplication::CApplicationDebugData& data, CMemoryStream& byteCode,
	CMemoryStream& debug, IQuadooDebugTree** tree, CRString& diagnostic)
{
	return StudioApplication::CompileDebug(m_pProject,m_rstrProject,data,byteCode,debug,tree,diagnostic);
}

HRESULT CQuadooProject::RunApplication()
{
	HRESULT hr;
	CRString executable, diagnostic, runtimeDirectory, arguments, workingDirectory, relativeDirectory, command;
	TStackRef<IJSONValue> value;
	WCHAR runtimePath[MAX_PATH];
	STARTUPINFOW startup = {sizeof(startup)};
	PROCESS_INFORMATION process = {0};
	Check(SaveApplicationInputs());
	CheckIf(!GetModuleFileNameW(m_hInstance,runtimePath,ARRAYSIZE(runtimePath)),HrEnsureLastError());
	{
		PCWSTR leaf = TStrRChr(runtimePath,L'\\');
		CheckIf(!leaf,E_INVALIDARG);
		Check(RStrCreateW(static_cast<INT>(leaf - runtimePath),runtimePath,&runtimeDirectory));
	}
	Check(StudioApplication::Build(m_pProject,m_rstrProject,runtimeDirectory,executable,diagnostic));
	Check(AQVM::StringField(m_pProject,L"args",&arguments));
	Check(AQVM::StringField(m_pProject,L"startDir",&relativeDirectory));
	Check(StudioApplication::Absolute(m_rstrProjectDir,relativeDirectory,&workingDirectory));
	Check(RStrFormatW(&command,L"\"%ls\" %ls",executable.c_wstr(),arguments.c_wstr()));
	startup.dwFlags = STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_SHOW;
	{
		PWSTR writable = NULL;
		Check(TDuplicateStringAssert(command.c_wstr(),&writable));
		BOOL started = CreateProcessW(executable.c_wstr(),writable,NULL,NULL,FALSE,0,NULL,workingDirectory.c_wstr(),&startup,&process);
		if(!started)
		{
			hr = HrEnsureLastError();
		}
		SafeDeleteArray(writable);
		Check(hr);
	}
Cleanup:
	if(process.hThread)
	{
		CloseHandle(process.hThread);
	}
	if(process.hProcess)
	{
		CloseHandle(process.hProcess);
	}
	if(FAILED(hr) && hr != E_ABORT)
	{
		if(!RStrLen(diagnostic))
		{
			RStrFormatW(&diagnostic,L"Application Run failed (0x%.8x). Check the saved sources and run parameters.",hr);
		}
		MessageBoxW(m_hwnd,diagnostic.c_wstr(),L"Run Error",MB_OK | MB_ICONERROR);
	}
	return hr;
}
