#include "..\..\..\TestPaths.h"

static HRESULT ParserEOFCase(PCWSTR script, INT length, BOOL fileInput, DWORD flags, BOOL valid, LONG expected)
{
	HRESULT hr;
	CMemoryStream code, debug;
	CCompilerStatus compiler;
	TStackRef<IQuadooInstanceLoader> loader;
	TStackRef<IQuadooVM> vm;
	TStackRef<IQuadooObject> exception;
	TStackRef<IQuadooDebugTree> tree;
	QuadooVM::QVARIANT result = {QuadooVM::Null};
	WCHAR directory[MAX_PATH];
	WCHAR path[MAX_PATH] = {0};
	HRESULT registered;

	if(fileInput)
	{
		Check(TestScratchDirectory(L"QuadooParser",directory,ARRAYSIZE(directory)));
		CheckIf(!GetTempFileNameW(directory,L"QPE",0,path),HrEnsureLastError());
		Check(Text::SaveToFileNoBOM(script,length,CP_UTF8,path));
		hr = QuadooParseToStream(path,flags,&code,&debug,&compiler,flags ? &tree : NULL);
	}
	else
	{
		hr = QuadooParseTextToStream(SLP(L"ParserEOF.quadoo"),script,length,flags,&code,&debug,&compiler,flags ? &tree : NULL);
	}
	if(!valid)
	{
		REQUIRE(FAILED(hr));
		hr = S_OK;
		goto Cleanup;
	}
	Check(hr);
	REQUIRE(code.DataRemaining() != 0);
	Check(QVMCreateLoader(NULL,&loader));
	Check(loader->AddInstance(RSTRING_CAST(L"ParserEOF"),NULL,&code,code.DataRemaining(),flags ? &debug : NULL));
	Check(loader->LoadVM(RSTRING_CAST(L"ParserEOF"),&registered,&vm));
	Check(vm->RunConstructor(&exception));
	REQUIRE(!exception);
	Check(Call(vm,L"Probe",&result));
	REQUIRE(result.eType == QuadooVM::I4 && result.lVal == expected);

Cleanup:
	QVMClearVariant(&result);
	if(vm)
	{
		vm->Unload();
	}
	if(path[0])
	{
		DeleteFileW(path);
	}
	return hr;
}

static HRESULT ParserEOFTests()
{
	HRESULT hr;
	struct Case
	{
		PCWSTR script;
		BOOL valid;
		LONG expected;
	};
	static const Case cases[] =
	{
		{L"function Probe(){return 1;}",TRUE,1},
		{L"var value = 7; function Probe(){return value;}",TRUE,7},
		{L"int value; function Probe(){return value;}",TRUE,0},
		{L"var values = {1,2,3}; function Probe(){return values[1];}",TRUE,2},
		{L"class Shared{static int value = 11;}; function Probe(){return Shared.value;}",TRUE,11},
		{L"bool flag; string text; double real; function Probe(){if(flag || text != \"\" || real != 0.0){return -1;} return 23;}",TRUE,23},
		{L"function Probe(){return 1;",FALSE,0},
		{L"var value = ;",FALSE,0},
		{L"var value = 1 +",FALSE,0}
	};
	static PCWSTR endings[] = {L"",L"\n",L"\r\n"};
	static const DWORD options[] = {0,QUADOO_COMPILE_LINE_NUMBER_MAP | QUADOO_COMPILE_FOR_DEBUGGING};

	for(UINT n = 0; n < ARRAYSIZE(cases); n++)
	{
		for(UINT e = 0; e < ARRAYSIZE(endings); e++)
		{
			for(UINT f = 0; f < 2; f++)
			{
				for(UINT d = 0; d < ARRAYSIZE(options); d++)
				{
					CRString script;
					Check(RStrFormatW(&script,L"%ls%ls",cases[n].script,endings[e]));
					Check(ParserEOFCase(script.c_wstr(),script.Length(),f,options[d],cases[n].valid,cases[n].expected));
				}
			}
		}
	}
	hr = S_OK;

Cleanup:
	return hr;
}
