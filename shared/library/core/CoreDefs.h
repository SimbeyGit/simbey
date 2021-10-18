#pragma once

// sysint
//
// Like size_t, sysint is 32-bit when built on a 32-bit platform and 64-bit when built
// on a 64-bit platform.  Unlike size_t, sysint is signed.  unsigned sysint is valid.

#ifdef	_WIN64 
	#define	sysint	__int64
#else
	#define	sysint	_W64 INT
#endif

// Memory Management
//
// Instead of using new and delete, __new and __delete should be used.  Instead of
// using malloc() and free(), __malloc() and __free() should be used.  These macros
// enable tracking memory leaks in debug builds and suppress C++ exceptions.

#if !defined(_DEBUG) || defined(__VIRTUAL_DBGMEM)
	struct VS6_NEW_T { };			// placement new tag type to suppress exceptions
	extern const VS6_NEW_T VS6_NEW;	// constant for placement new tag

	__bcount_opt(cb) void* __cdecl operator new(size_t cb, const VS6_NEW_T&) throw();
	__bcount_opt(cb) void* __cdecl operator new[](size_t cb, const VS6_NEW_T&) throw();
	void __cdecl operator delete(void*, const VS6_NEW_T&) throw();
	void __cdecl operator delete[](void*, const VS6_NEW_T&) throw();
	void __cdecl operator delete(void*) throw();
	void __cdecl operator delete[](void*) throw();

	#define	__new					::new(VS6_NEW)
	#define	__new_placement(...)	::new(__VA_ARGS__)

	#define	__delete				::delete
	#define	__delete_array			::delete[]

	#if defined(__VIRTUAL_DBGMEM)
		#if !defined(_DEBUG)
			#error Only debug builds are supported with __VIRTUAL_DBGMEM
		#endif

		void* virtual_dbg_alloc (size_t);
		void virtual_dbg_free (void*);
		void* virtual_dbg_realloc (void*, size_t);

		#define __malloc(size)			virtual_dbg_alloc(size) 
		#define __calloc(num, size)		virtual_dbg_alloc(num * size)
		#define __realloc(ptr, size)	virtual_dbg_realloc(ptr, size)
		#define __free(ptr)				virtual_dbg_free(ptr)
	#else
		#define	__malloc(size)			malloc(size)
		#define	__calloc(num,size)		calloc_dbg(num,size)
		#define	__realloc(ptr,size)		realloc(ptr,size)
		#define	__free(ptr)				free(ptr)
	#endif
#else
	#ifndef __DEBUG_HEAP_SUB_TYPE
		#define _USER_SUBTYPE		MAKELONG(_CLIENT_BLOCK, 0)
	#else
		#define _USER_SUBTYPE		MAKELONG(_CLIENT_BLOCK, __DEBUG_HEAP_SUB_TYPE)
	#endif

	#include <crtdbg.h>

	#define	__new					::new(_USER_SUBTYPE,__FILE__,__LINE__)
	#define	__new_placement(...)	::new(__VA_ARGS__)

	#define	__delete				::delete
	#define	__delete_array			::delete[]

	#define	__malloc(size)			_malloc_dbg(size,_USER_SUBTYPE,__FILE__,__LINE__)
	#define	__calloc(num,size)		_calloc_dbg(num,size,_USER_SUBTYPE,__FILE__,__LINE__)
	#define	__realloc(ptr,size)		_realloc_dbg(ptr,size,_USER_SUBTYPE,__FILE__,__LINE__)
	#define	__free(ptr)				_free_dbg(ptr,_USER_SUBTYPE)
#endif

#ifndef __PLACEMENT_NEW_INLINE
	#define __PLACEMENT_NEW_INLINE

	inline void* __cdecl operator new(size_t, void* pvPlacement) throw()
	{
		return pvPlacement;
	}

	inline void __cdecl operator delete(void*, void*) throw()
	{
	}
#endif

// COM Helper Functions
HRESULT WINAPI ComConnect (IUnknown* pUnkCP, IUnknown* pUnk, const IID& iid, LPDWORD pdwCookie);
HRESULT WINAPI ComDisconnect (IUnknown* pUnkCP, const IID& iid, DWORD dw);

#include "Assert.h"
#include "Check.h"
#include "Pointers.h"

template <class T>
struct TRemovePtr
{
	typedef T type;
};

template <class T>
struct TRemovePtr<T*>
{
	typedef T type;
};

template <class T>
struct TRemovePtr<T* const volatile>
{
	typedef T type;
};
