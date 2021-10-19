#pragma once

#include "..\Core\ISeekableStream.h"

class CFileStream : public ISeekableStream
{
private:
	ULONG m_cRef;

protected:
	HANDLE m_hFile;
	BOOL m_fCloseOnDelete;

public:
	CFileStream (HANDLE hFile, BOOL fCloseOnDelete = FALSE);
	virtual ~CFileStream ();

	// IUnknown (CFileStream can be created on the stack)
	virtual HRESULT WINAPI QueryInterface (REFIID iid, LPVOID* ppvObject);
	virtual ULONG WINAPI AddRef (VOID);
	virtual ULONG WINAPI Release (VOID);

	// ISequentialStream
	virtual HRESULT WINAPI Read (LPVOID pv, ULONG cb, ULONG* pcbRead);
	virtual HRESULT WINAPI Write (VOID CONST* pv, ULONG cb, ULONG* pcbWritten);

	// ISeekableStream
	virtual HRESULT WINAPI Seek (LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, __out_opt ULARGE_INTEGER* puliNewPosition);
	virtual HRESULT WINAPI Stat (__out STATSTG* pStatstg, DWORD grfStatFlag);
	virtual HRESULT WINAPI Duplicate (__deref_out ISeekableStream** ppDupStream);

	static HRESULT Open (LPCTSTR pctzFile, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, __deref_out CFileStream** ppstmFile, __out_opt ULARGE_INTEGER* puliSize);

	HRESULT GetFileTime (__out_opt FILETIME* pftCreation, __out_opt FILETIME* pftLastAccess, __out_opt FILETIME* pftLastWrite);

protected:
	virtual ISeekableStream* Alloc (HANDLE hFile);
};
