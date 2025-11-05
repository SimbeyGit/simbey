#include <windows.h>
#include "Core\Assert.h"
#include "NativeInvoke.h"

#define	DWORDPTR(p)		(DWORD)(DWORD_PTR)(p)

CNativeInvoke::CNativeInvoke (BOOL fStackCleanup, DWORD_PTR dwThisPtr)
{
	m_pbPage = NULL;
	m_nWritePtr = 0;
	m_cParams = 0;
	m_fStackCleanup = fStackCleanup;
	m_dwThisPtr = dwThisPtr;
}

CNativeInvoke::~CNativeInvoke ()
{
	if(m_pbPage)
		VirtualFree(m_pbPage,SIZE_NATIVE_INVOKE_PAGE,MEM_RELEASE);
}

HRESULT CNativeInvoke::Initialize (VOID)
{
	HRESULT hr;

	Assert(NULL == m_pbPage);

	m_pbPage = (LPBYTE)VirtualAlloc(NULL,SIZE_NATIVE_INVOKE_PAGE,MEM_COMMIT | MEM_RESERVE,PAGE_EXECUTE_READWRITE);
	if(m_pbPage)
	{
		Reset();
		hr = S_OK;
	}
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	return hr;
}

VOID CNativeInvoke::SetStackCleanup (BOOL fStackCleanup)
{
	// TRUE means the caller cleans up the stack, and this means adding a value to esp
	// to cover for the passed parameters.  If FALSE (or if there are no parameters,
	// it is assumed that the callee will clean up the stack.
	//
	// If using __cdecl or __thiscall (with variable parameter lists), you must set
	// the stack cleanup to TRUE.
	//
	// If using __stdcall or __thiscall (with a fixed parameter list), you must set
	// the stack cleanup to FALSE.
	m_fStackCleanup = fStackCleanup;
}

VOID CNativeInvoke::SetThisPtr (DWORD_PTR dwThisPtr)
{
	m_dwThisPtr = dwThisPtr;
}

VOID CNativeInvoke::Reset (VOID)
{
	m_pbPage[0] = 0x55;									// push ebp
	m_pbPage[1] = 0x8B;									// mov ebp, esp
	m_pbPage[2] = 0xEC;
	m_nWritePtr = 3;
	m_cParams = 0;
}

HRESULT CNativeInvoke::AddParam8 (BYTE bParam)
{
	HRESULT hr;
	if(m_nWritePtr + 5 < SIZE_NATIVE_INVOKE_PAGE && m_cParams < MAX_NATIVE_PARAMS)
	{
		m_pbPage[m_nWritePtr] = 0x68;					// push dword <n>
		m_pbPage[m_nWritePtr + 1] = bParam;
		m_pbPage[m_nWritePtr + 2] = 0;
		m_pbPage[m_nWritePtr + 3] = 0;
		m_pbPage[m_nWritePtr + 4] = 0;
		m_nWritePtr += 5;
		m_cParams++;
		hr = S_OK;
	}
	else
		hr = E_FAIL;
	return hr;
}

HRESULT CNativeInvoke::AddParam16 (WORD wParam)
{
	HRESULT hr;
	if(m_nWritePtr + 5 < SIZE_NATIVE_INVOKE_PAGE && m_cParams < MAX_NATIVE_PARAMS)
	{
		m_pbPage[m_nWritePtr] = 0x68;					// push dword <n>
		m_pbPage[m_nWritePtr + 1] = (BYTE)(wParam & 0xFF);
		m_pbPage[m_nWritePtr + 2] = (BYTE)(wParam >> 8);
		m_pbPage[m_nWritePtr + 3] = 0;
		m_pbPage[m_nWritePtr + 4] = 0;
		m_nWritePtr += 5;
		m_cParams++;
		hr = S_OK;
	}
	else
		hr = E_FAIL;
	return hr;
}

HRESULT CNativeInvoke::AddParam32 (DWORD dwParam)
{
	HRESULT hr;
	if(m_nWritePtr + 5 < SIZE_NATIVE_INVOKE_PAGE && m_cParams < MAX_NATIVE_PARAMS)
	{
		m_pbPage[m_nWritePtr] = 0x68;					// push dword <n>
		m_pbPage[m_nWritePtr + 1] = (BYTE)(dwParam & 0xFF);
		m_pbPage[m_nWritePtr + 2] = (BYTE)(dwParam >> 8);
		m_pbPage[m_nWritePtr + 3] = (BYTE)(dwParam >> 16);
		m_pbPage[m_nWritePtr + 4] = (BYTE)(dwParam >> 24);
		m_nWritePtr += 5;
		m_cParams++;
		hr = S_OK;
	}
	else
		hr = E_FAIL;
	return hr;
}

HRESULT CNativeInvoke::Call (DWORD_PTR dwPtr)
{
	HRESULT hr;
	if(m_nWritePtr + 17 < SIZE_NATIVE_INVOKE_PAGE)
	{
		INT nWritePtr = m_nWritePtr;

		EmitCall(dwPtr);	// Write up to 15 bytes.

		m_pbPage[m_nWritePtr] = 0x5D;					// pop ebp
		m_pbPage[m_nWritePtr + 1] = 0xC3;				// ret
		m_nWritePtr += 2;

		hr = Execute();

		m_nWritePtr = nWritePtr;
	}
	else
		hr = E_FAIL;
	return hr;
}

HRESULT CNativeInvoke::Call (DWORD_PTR dwPtr, DWORD* pdwReturn)
{
	HRESULT hr;
	if(m_nWritePtr + 22 < SIZE_NATIVE_INVOKE_PAGE)
	{
		INT nWritePtr = m_nWritePtr;

		EmitCall(dwPtr);	// Write up to 15 bytes.

		EmitOpCode(0xA3, DWORDPTR(pdwReturn));			// mov [pdwReturn], eax

		m_pbPage[m_nWritePtr] = 0x5D;					// pop ebp
		m_pbPage[m_nWritePtr + 1] = 0xC3;				// ret
		m_nWritePtr += 2;

		hr = Execute();

		m_nWritePtr = nWritePtr;
	}
	else
		hr = E_FAIL;
	return hr;
}

HRESULT CNativeInvoke::Call (DWORD_PTR dwPtr, DWORDLONG* pdwlReturn)
{
	HRESULT hr;
	if(m_nWritePtr + 28 < SIZE_NATIVE_INVOKE_PAGE)
	{
		DWORD* pdwReturn = (DWORD*)pdwlReturn;
		INT nWritePtr = m_nWritePtr;

		EmitCall(dwPtr);	// Write up to 15 bytes.

		EmitOpCode(0xA3, DWORDPTR(pdwReturn));			// mov [pdwReturn],   eax
		EmitOpCode(0x89, 0x15, DWORDPTR(pdwReturn + 1));// mov [pdwReturn+1], edx

		m_pbPage[m_nWritePtr] = 0x5D;					// pop ebp
		m_pbPage[m_nWritePtr + 1] = 0xC3;				// ret
		m_nWritePtr += 2;

		hr = Execute();

		m_nWritePtr = nWritePtr;
	}
	else
		hr = E_FAIL;
	return hr;
}

VOID CNativeInvoke::EmitCall (DWORD_PTR dwPtr)
{
	Assert(m_nWritePtr + 15 < SIZE_NATIVE_INVOKE_PAGE);

	if(0 != m_dwThisPtr)
		EmitOpCode(0xB9,(DWORD)m_dwThisPtr);			// mov ecx, m_dwThisPtr

	m_pbPage[m_nWritePtr] = 0xB8;						// mov eax, <address>
	m_pbPage[m_nWritePtr + 1] = (BYTE)(dwPtr & 0xFF);
	m_pbPage[m_nWritePtr + 2] = (BYTE)(dwPtr >> 8);
	m_pbPage[m_nWritePtr + 3] = (BYTE)(dwPtr >> 16);
	m_pbPage[m_nWritePtr + 4] = (BYTE)(dwPtr >> 24);
	m_pbPage[m_nWritePtr + 5] = 0xFF;					// call eax
	m_pbPage[m_nWritePtr + 6] = 0xD0;
	m_nWritePtr += 7;

	if(0 < m_cParams && m_fStackCleanup)
	{
		m_pbPage[m_nWritePtr] = 0x83;					// add esp, byte +<n>
		m_pbPage[m_nWritePtr + 1] = 0xC4;

		// This is why MAX_NATIVE_PARAMS is 63.
		// The value of the product below will always fit in one BYTE.
		m_pbPage[m_nWritePtr + 2] = (BYTE)(m_cParams * sizeof(DWORD));

		m_nWritePtr += 3;
	}
}

VOID CNativeInvoke::EmitOpCode (BYTE bOpCode, DWORD dwValue)
{
	Assert(m_nWritePtr + 5 < SIZE_NATIVE_INVOKE_PAGE);

	m_pbPage[m_nWritePtr] = bOpCode;
	m_pbPage[m_nWritePtr + 1] = (BYTE)(dwValue & 0xFF);
	m_pbPage[m_nWritePtr + 2] = (BYTE)(dwValue >> 8);
	m_pbPage[m_nWritePtr + 3] = (BYTE)(dwValue >> 16);
	m_pbPage[m_nWritePtr + 4] = (BYTE)(dwValue >> 24);
	m_nWritePtr += 5;
}

VOID CNativeInvoke::EmitOpCode (BYTE bOpCode, BYTE bOperand, DWORD dwValue)
{
	Assert(m_nWritePtr + 6 < SIZE_NATIVE_INVOKE_PAGE);

	m_pbPage[m_nWritePtr] = bOpCode;
	m_pbPage[m_nWritePtr + 1] = bOperand;
	m_pbPage[m_nWritePtr + 2] = (BYTE)(dwValue & 0xFF);
	m_pbPage[m_nWritePtr + 3] = (BYTE)(dwValue >> 8);
	m_pbPage[m_nWritePtr + 4] = (BYTE)(dwValue >> 16);
	m_pbPage[m_nWritePtr + 5] = (BYTE)(dwValue >> 24);
	m_nWritePtr += 6;
}

HRESULT CNativeInvoke::Execute (VOID)
{
	HRESULT hr;
	DWORD dwPrevProtection;

	if(VirtualProtect(m_pbPage, m_nWritePtr, PAGE_EXECUTE, &dwPrevProtection))
	{
		VOID (WINAPI* pfnCall)(VOID) = (VOID(WINAPI*)(VOID))m_pbPage;
		if(FlushInstructionCache(GetCurrentProcess(), pfnCall, m_nWritePtr))
		{
			pfnCall();
			hr = S_OK;
		}
		else
			hr = HRESULT_FROM_WIN32(GetLastError());

		VirtualProtect(m_pbPage, m_nWritePtr, dwPrevProtection, &dwPrevProtection);
	}
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	return hr;
}
