#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "NativeInvoke64.h"

HRESULT CNativeInvoke::Initialize ()
{
	HRESULT hr;

	CheckIf(m_pbPage, S_FALSE);
	m_pbPage = (LPBYTE)VirtualAlloc(NULL, SIZE_NATIVE_INVOKE64_PAGE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	CheckIfGetLastError(NULL == m_pbPage);
	hr = S_OK;

Cleanup:
	return hr;
}

void CNativeInvoke::Reset ()
{
	m_args.Clear();
}

HRESULT CNativeInvoke::AddArg (const Arg& a)
{
	HRESULT hr;

	CheckIf(m_args.Length() >= MAX_NATIVE_PARAMS64, E_FAIL);
	Check(m_args.Append(a));

Cleanup:
	return hr;
}

void CNativeInvoke::emit8 (BYTE b)
{
	m_pbPage[m_nWritePtr++] = b;
}

void CNativeInvoke::emit32 (DWORD v)
{
	CopyMemory(m_pbPage + m_nWritePtr, &v, 4);
	m_nWritePtr += 4;
}

void CNativeInvoke::emit64 (ULONGLONG q)
{
	CopyMemory(m_pbPage + m_nWritePtr, &q, 8);
	m_nWritePtr += 8;
}

HRESULT CNativeInvoke::GenerateStub_x64 (DWORD_PTR pfnTarget, ULONGLONG* pReturnU64, DOUBLE* pReturnF64)
{
	HRESULT hr;
	TArray<Arg> args;

	// 0) Build ordered argument list (prepend implicit this if present)
	if(m_qwThisPtr)
		Check(args.Append(Arg::Int(m_qwThisPtr)));
	for(sysint i = 0; i < m_args.Length(); ++i)
		Check(args.Append(m_args[i]));

	const sysint n = args.Length();
	m_nWritePtr = 0;

	// 1) First four arguments by *position*, not by class
	static const BYTE mov64_gp[4][2] = { {0x48,0xB9}, {0x48,0xBA}, {0x49,0xB8}, {0x49,0xB9} }; // RCX,RDX,R8,R9
	static const BYTE movq_xmm_from_rax[4] = { 0xC0, 0xC8, 0xD0, 0xD8 };                         // XMM0..3 (MOVQ)

	const sysint regCount = (n < 4) ? n : 4;
	for(sysint i = 0; i < regCount; i++)
	{
		const Arg &a = args[i];
		if(a.kind == F32 || a.kind == F64)
		{
			// Load XMM{i} with 64-bit FP payload via RAX (no inline data in code stream)
			ULONGLONG q;
			if (a.kind == F64)
				CopyMemory(&q, &a.v.f64, 8);
			else
			{
				DWORD lo;
				CopyMemory(&lo, &a.v.f32, 4);
				q = (ULONGLONG)lo;
			}

			// mov rax, imm64
			emit8(0x48); emit8(0xB8);
			emit64(q);

			// movq xmm{i}, rax   (66 48 0F 6E C0|C8|D0|D8)
			emit8(0x66); emit8(0x48); emit8(0x0F); emit8(0x6E);
			emit8(movq_xmm_from_rax[i]);
		}
		else
		{
			// mov RCX/RDX/R8/R9, imm64
			emit8(mov64_gp[i][0]); emit8(mov64_gp[i][1]);
			emit64(a.v.u64);
		}
	}

	// 2) Compute stack frame size and allocate once (no pushes)
	const sysint k = (n > 4) ? (n - 4) : 0;       // # stack arguments
	BYTE pad  = (BYTE)((k & 1) ? 0x00 : 0x08);    // even k -> need +8 to make total % 16 == 8
	DWORD total = 0x20 + (DWORD)(8 * k) + pad;    // shadow + stack slots + pad

	// sub rsp, total  (use imm32 form in case it grows)
	emit8(0x48); emit8(0x81); emit8(0xEC);
	emit32(total);

	// 3) Store stack args directly at [rsp + 0x20 + 8*idx]
	//    idx 0 corresponds to arg #4 (the 5th overall argument)
	for(sysint i = 4; i < n; ++i)
	{
		const Arg& a = args[i];
		ULONGLONG q;

		if(a.kind == GP64)
			q = a.v.u64;
		else if(a.kind == F64)
			CopyMemory(&q, &a.v.f64, 8);
		else // F32 -> low dword holds the bits, high dword = 0
		{
			DWORD lo; CopyMemory(&lo, &a.v.f32, 4);
			q = (ULONGLONG)lo;
		}

		// mov rax, imm64
		emit8(0x48); emit8(0xB8);
		emit64(q);

		// mov [rsp + (0x20 + 8*(i-4))], rax
		// encoding: 48 89 84 24 <disp32>
		DWORD disp = 0x20 + (DWORD)((i - 4) * 8);
		emit8(0x48); emit8(0x89); emit8(0x84); emit8(0x24);
		emit32(disp);
	}

	// 4) Call target
	emit8(0x48); emit8(0xB8);
	emit64(pfnTarget);
	emit8(0xFF); emit8(0xD0); // call rax

	// 5) Epilogue: add rsp, total
	emit8(0x48); emit8(0x81); emit8(0xC4);
	emit32(total);

	// 6) Optional return captures
	if(pReturnU64)
	{
		// mov [abs64], rax  -> 48 A3 imm64
		emit8(0x48); emit8(0xA3);
		emit64(reinterpret_cast<ULONGLONG>(pReturnU64));
	}
	else if(pReturnF64)
	{
		// mov rax, imm64
		emit8(0x48); emit8(0xB8);
		emit64(reinterpret_cast<ULONGLONG>(pReturnF64));

		// movsd [rax], xmm0
		emit8(0xF2); emit8(0x0F); emit8(0x11); emit8(0x00);
	}

	emit8(0xC3);
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CNativeInvoke::Execute ()
{
	typedef void (WINAPI* CALL_STUB)(void);

	HRESULT hr;
	DWORD oldProt;

	if(VirtualProtect(m_pbPage, SIZE_NATIVE_INVOKE64_PAGE, PAGE_EXECUTE_READ, &oldProt))
	{
		CALL_STUB pfnStub = reinterpret_cast<CALL_STUB>(m_pbPage);
		if(FlushInstructionCache(GetCurrentProcess(), pfnStub, (SIZE_T)m_nWritePtr))
		{
			pfnStub();
			hr = S_OK;
		}
		else
			hr = HRESULT_FROM_WIN32(GetLastError());

		VirtualProtect(m_pbPage, SIZE_NATIVE_INVOKE64_PAGE, oldProt, &oldProt);
	}
	else
		hr = HRESULT_FROM_WIN32(GetLastError());

	return hr;
}

HRESULT CNativeInvoke::Call (DWORD_PTR pfnTarget)
{
    HRESULT hr = GenerateStub_x64(pfnTarget, NULL, NULL);
	if(SUCCEEDED(hr))
		hr = Execute();
	return hr;
}

HRESULT CNativeInvoke::Call (DWORD_PTR pfnTarget, ULONGLONG* pReturnU64)
{
    HRESULT hr = GenerateStub_x64(pfnTarget, pReturnU64, NULL);
	if(SUCCEEDED(hr))
		hr = Execute();
	return hr;
}

HRESULT CNativeInvoke::Call (DWORD_PTR pfnTarget, DOUBLE* pReturnF64)
{
    HRESULT hr = GenerateStub_x64(pfnTarget, NULL, pReturnF64);
	if(SUCCEEDED(hr))
		hr = Execute();
	return hr;
}
