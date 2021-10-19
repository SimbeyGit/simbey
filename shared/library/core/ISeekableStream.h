#pragma once

interface __declspec(uuid("C8694CCC-413C-4a8d-B083-69613DB1B446")) ISeekableStream : public ISequentialStream
{
	virtual HRESULT WINAPI Seek (LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, __out_opt ULARGE_INTEGER* puliNewPosition) = 0;
	virtual HRESULT WINAPI Stat (__out STATSTG* pStatstg, DWORD grfStatFlag) = 0;
	virtual HRESULT WINAPI Duplicate (__deref_out ISeekableStream** ppDupStream) = 0;
};
