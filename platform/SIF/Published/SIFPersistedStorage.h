#pragma once

#ifdef SIF_EXPORTS
	#define SIF_API __declspec(dllexport)
#else
	#define SIF_API __declspec(dllimport)
#endif

interface ISimbeyInterchangeFile;

interface __declspec(uuid("9D79CE0B-FDD0-4678-91CC-ECA8F375914F")) IPersistedFile : public ISequentialStream
{
	virtual HRESULT WINAPI Seek (DWORD dwPosition) = 0;
};

interface __declspec(uuid("6F28811F-5BFF-4ff8-A9CB-E7552C9E83F7")) IPersistedStorage : public IUnknown
{
	virtual HRESULT WINAPI Open (PCWSTR pcwzFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, __deref_out IPersistedFile** ppFile) = 0;
	virtual HRESULT WINAPI Replace (PCWSTR pcwzReplacedFile, PCWSTR pcwzReplacementFile) = 0;
	virtual HRESULT WINAPI Move (PCWSTR pcwzExistingFile, PCWSTR pcwzNewFileName) = 0;
	virtual HRESULT WINAPI Delete (PCWSTR pcwzFile) = 0;
};

SIF_API HRESULT WINAPI sifLoadFromStorage (__in IPersistedStorage* pStorage, __in_opt PCWSTR pcwzSIF, __deref_out ISimbeyInterchangeFile** ppSIF);