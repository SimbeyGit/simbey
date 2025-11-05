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
        union { ULONGLONG u64; float f32; double f64; } v;
        static Arg Int(ULONGLONG x)		{ Arg a; a.kind=ArgClass::GP64; a.v.u64=x; return a; }
        static Arg Ptr(const void* p)	{ Arg a; a.kind=ArgClass::GP64; a.v.u64=(ULONGLONG)reinterpret_cast<uintptr_t>(p); return a; }
        static Arg ByRef(void* p)		{ Arg a; a.kind=ArgClass::GP64; a.v.u64=(ULONGLONG)reinterpret_cast<uintptr_t>(p); return a; }
        static Arg Float(float x)		{ Arg a; a.kind=ArgClass::F32;  a.v.f32=x; return a; }
        static Arg Double(double x)		{ Arg a; a.kind=ArgClass::F64;  a.v.f64=x; return a; }
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
    HRESULT AddParam8   (BYTE v)		{ return AddArg(Arg::Int(v)); }
    HRESULT AddParam16  (WORD v)		{ return AddArg(Arg::Int(v)); }
    HRESULT AddParam32  (DWORD v)		{ return AddArg(Arg::Int(v)); }
    HRESULT AddParam64  (ULONGLONG v)	{ return AddArg(Arg::Int(v)); }
    HRESULT AddParamPtr (const void* p)	{ return AddArg(Arg::Ptr(p)); }
    HRESULT AddFloat    (float f)		{ return AddArg(Arg::Float(f)); }
    HRESULT AddDouble   (double d)		{ return AddArg(Arg::Double(d)); }

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
