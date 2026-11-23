#pragma once

interface __declspec(uuid("47276451-7539-48ae-809A-7A5BA4089AA1")) ILockableStream : public ISequentialStream
{
	virtual HRESULT WINAPI Lock (__deref_out_bcount(*pcbData) PBYTE* ppbData, __out DWORD* pcbData) = 0;
	virtual VOID WINAPI Unlock (VOID) = 0;
};
