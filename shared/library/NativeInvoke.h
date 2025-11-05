#pragma once

#define	SIZE_NATIVE_INVOKE_PAGE		1024
#define	MAX_NATIVE_PARAMS			63

class CNativeInvoke
{
private:
	LPBYTE m_pbPage;
	INT m_nWritePtr;
	INT m_cParams;
	BOOL m_fStackCleanup;
	DWORD_PTR m_dwThisPtr;

public:
	CNativeInvoke (BOOL fStackCleanup = TRUE /* Defaulted for __cdecl */, DWORD_PTR dwThisPtr = 0);
	~CNativeInvoke ();

	HRESULT Initialize (VOID);
	VOID SetStackCleanup (BOOL fStackCleanup);
	VOID SetThisPtr (DWORD_PTR dwThisPtr);
	VOID Reset (VOID);

	HRESULT AddParam8 (BYTE bParam);
	HRESULT AddParam16 (WORD wParam);
	HRESULT AddParam32 (DWORD dwParam);

	HRESULT Call (DWORD_PTR dwPtr);
	HRESULT Call (DWORD_PTR dwPtr, DWORD* pdwReturn);
	HRESULT Call (DWORD_PTR dwPtr, DWORDLONG* pdwlReturn);

protected:
	VOID EmitCall (DWORD_PTR dwPtr);
	VOID EmitOpCode (BYTE bOpCode, DWORD dwValue);
	VOID EmitOpCode (BYTE bOpCode, BYTE bOperand, DWORD dwValue);
	HRESULT Execute (VOID);
};
