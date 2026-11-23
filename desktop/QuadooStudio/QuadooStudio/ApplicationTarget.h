#pragma once

#include "..\..\..\platform\QuadooForm\ScriptAdapter\ApplicationResources.h"
#include "Library\Core\Array.h"

interface IQuadooDebugTree;
interface IStartDebuggerStatus;

namespace StudioApplication
{
	class CApplicationDebugData
	{
		TArray<RSTRING> m_dependencies;

	public:
		CRString program;
		CMemoryStream bundle;

		~CApplicationDebugData();
		HRESULT AddDependency(RSTRING path);
		HRESULT PrepareDependencies(PCWSTR debugger, IStartDebuggerStatus* status, CRString& diagnostic);
	};

	HRESULT CompileDebug(IJSONObject* project, RSTRING projectPath, CApplicationDebugData& data,
		CMemoryStream& byteCode, CMemoryStream& debug, IQuadooDebugTree** tree, CRString& diagnostic);

	// Build from saved project inputs. This never launches or writes startup source.
	HRESULT Build(IJSONObject* project, RSTRING projectPath, RSTRING runtimeDirectory,
		CRString& executable, CRString& diagnostic);
	VOID FailAfter(INT checkpoint);
}
