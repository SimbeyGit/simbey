#pragma once

#include "Library\Core\Array.h"

// Keep parity with CNativeInvoke naming/style, but adapted for Win64 ABI.
// This class preserves the public "build params, then Call()" design while
// rewriting the stub emitter for RCX/RDX/R8/R9 + XMM0..3, 32B shadow space,
// and 16-byte stack alignment at the call site.

#define SIZE_NATIVE_INVOKE64_PAGE   4096
#define MAX_NATIVE_PARAMS64         64

class CNativeInvoke
{
public:
    enum ArgClass
	{
		GP64, F32, F64
	};

    struct Arg
    {
        ArgClass kind;
        union { ULONGLONG u64; float f32; double f64; };

		Arg (const Arg& o) : kind(o.kind), u64(o.u64) {}
		Arg (BYTE x) : kind(GP64), u64(x) {}
		Arg (WORD x) : kind(GP64), u64(x) {}
		Arg (DWORD x) : kind(GP64), u64(x) {}
		Arg (ULONGLONG x) : kind(GP64), u64(x) {}
		Arg (const VOID* pcv) : kind(GP64), u64(reinterpret_cast<uintptr_t>(pcv)) {}
		Arg (VOID* pv) : kind(GP64), u64(reinterpret_cast<uintptr_t>(pv)) {}
		Arg (FLOAT x) : kind(F32), f32(x) {}
		Arg (DOUBLE x) : kind(F64), f64(x) {}
    };

private:
    // Execution page buffer (like CNativeInvoke)
    LPBYTE         m_pbPage;
    INT            m_nWritePtr;        // number of code bytes written (for Execute sizing)

    // Collected arguments in left-to-right order
    TArray<Arg>    m_args;

    // Optional implicit "this" (placed in RCX before other GP args if set)
    ULONGLONG      m_qwThisPtr;       // 0 means unused

public:
    CNativeInvoke (ULONGLONG qwThisPtr = 0) :
		m_qwThisPtr(qwThisPtr),
		m_pbPage(NULL),
		m_nWritePtr(0)
	{
	}

    ~CNativeInvoke ()
	{
		if(m_pbPage)
			VirtualFree(m_pbPage, SIZE_NATIVE_INVOKE64_PAGE, MEM_RELEASE);
	}

    HRESULT Initialize();
    void    Reset();

    // Parity-style adder methods
    HRESULT AddParam8   (BYTE v)		{ return AddArg(Arg(v)); }
    HRESULT AddParam16  (WORD v)		{ return AddArg(Arg(v)); }
    HRESULT AddParam32  (DWORD v)		{ return AddArg(Arg(v)); }
    HRESULT AddParam64  (ULONGLONG v)	{ return AddArg(Arg(v)); }
    HRESULT AddParamPtr (const void* p)	{ return AddArg(Arg(p)); }
    HRESULT AddFloat    (float f)		{ return AddArg(Arg(f)); }
    HRESULT AddDouble   (double d)		{ return AddArg(Arg(d)); }

    void SetThisPtr (ULONGLONG qwThisPtr) { m_qwThisPtr = qwThisPtr; }

    // Call entry points. These *execute the generated stub* immediately like the x86 version.
    HRESULT Call (DWORD_PTR pfnTarget);
    HRESULT Call (DWORD_PTR pfnTarget, ULONGLONG* pReturnU64);
    HRESULT Call (DWORD_PTR pfnTarget, DOUBLE* pReturnF64);

private:
    HRESULT AddArg (const Arg& a);
    HRESULT Execute ();

    // Stub generator
    HRESULT GenerateStub_x64 (DWORD_PTR pfnTarget, ULONGLONG* pReturnU64, DOUBLE* pReturnF64);

    void emit8(BYTE b);
    void emit32(DWORD v);
	void emit64(ULONGLONG q);
};
