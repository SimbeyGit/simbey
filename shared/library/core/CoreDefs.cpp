#include <windows.h>
#include "CoreDefs.h"

#if !defined(_DEBUG) || defined(__VIRTUAL_DBGMEM)
	const VS6_NEW_T VS6_NEW;	// constant for placement new tag

	#if defined(__VIRTUAL_DBGMEM)
		static size_t allocation_count = 0;

		// Verify that the sentinel values past the user portion of the allocation have NOT been touched
		//
		static void check_dbg_alloction (size_t* pBase)
		{
			size_t total_bytes = *pBase; 
			size_t partial_page	= (total_bytes%4096)?1:0;
			size_t pages = (total_bytes/4096 + partial_page);
			unsigned char* end_of_pages = reinterpret_cast<unsigned char*>(pBase) + (pages*4096);

			for(unsigned char* w = reinterpret_cast<unsigned char*>(pBase) + total_bytes; w < end_of_pages; w++ )
			{
				if(*w != static_cast<unsigned char>(0xEE))
				{
					DebugBreak();
				}
			}
		}

		// Allocate, commit, and back fill a memory page (4k)
		//
		void* virtual_dbg_alloc (size_t user_bytes)
		{
			size_t total_bytes = user_bytes + sizeof(size_t) + sizeof(size_t);
			size_t allocation_num = InterlockedIncrement(reinterpret_cast<LONG_PTR*>(&allocation_count));
			size_t* pMem = (size_t*)VirtualAlloc(NULL, total_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

			if(pMem)
			{
				*pMem = total_bytes;
				pMem++;

				*pMem = allocation_num;
				pMem++;

				size_t partial_page	= (total_bytes%4096)?1:0;
				size_t filler = ((total_bytes / 4096 + partial_page) * 4096) - total_bytes;

				memset( reinterpret_cast<char*>(pMem) + user_bytes, 0xEE, filler );
			}

			return pMem;
		}

		// Release the physical memory, but leave the address space off limits.
		//
		void virtual_dbg_free (void* ptr)
		{
			if(ptr)
			{
				size_t* pBase = reinterpret_cast<size_t*>(ptr) - 2;

				check_dbg_alloction(pBase);

				SideAssert(VirtualFree(pBase, *pBase, MEM_DECOMMIT));
			}
		}

		void* virtual_dbg_realloc (void* ptr, size_t size)
		{
			void* pMem = NULL;

			if(ptr)
			{
				size_t* pBase = reinterpret_cast<size_t*>(ptr) - 2;
				MEMORY_BASIC_INFORMATION mbi;

				SideAssert(0 != VirtualQuery(pBase, &mbi, sizeof(mbi)));

				pMem = virtual_dbg_alloc(size);
				if(pMem)
				{
					CopyMemory(pMem, ptr, min(size, *pBase - sizeof(size_t) * 2));
					virtual_dbg_free(ptr);
				}
			}
			else
				pMem = virtual_dbg_alloc(size);

			return pMem;
		}

	#endif

	__bcount_opt(cb) void* __cdecl operator new(size_t cb, const VS6_NEW_T&) throw()
	{
		return __malloc(cb);
	}

	__bcount_opt(cb) void* __cdecl operator new[](size_t cb, const VS6_NEW_T&) throw()
	{
		return __malloc(cb);
	}

	void __cdecl operator delete(void* p, const VS6_NEW_T&) throw()
	{
		__free(p);
	}

	void __cdecl operator delete[](void* p, const VS6_NEW_T&) throw()
	{
		__free(p);
	}

	void __cdecl operator delete(void* p) throw()
	{
		__free(p);
	}

	void __cdecl operator delete[](void* p) throw()
	{
		__free(p);
	}

#endif

HRESULT WINAPI ComConnect (IUnknown* pUnkCP, IUnknown* pUnk, const IID& iid, LPDWORD pdwCookie)
{
	HRESULT hr;

	Assert(NULL != pUnkCP);

	if(pUnkCP)
	{
		IConnectionPointContainer* pCPC;
		IConnectionPoint* pCP;

		hr = pUnkCP->QueryInterface(__uuidof(IConnectionPointContainer), (PVOID*)&pCPC);
		if(SUCCEEDED(hr))
		{
			hr = pCPC->FindConnectionPoint(iid, &pCP);
			if(SUCCEEDED(hr))
			{
				hr = pCP->Advise(pUnk, pdwCookie);
				pCP->Release();
			}
			pCPC->Release();
		}
	}
	else
		hr = E_INVALIDARG;

	return hr;
}

HRESULT WINAPI ComDisconnect (IUnknown* pUnkCP, const IID& iid, DWORD dw)
{
	HRESULT hr;

	if(pUnkCP)
	{
		IConnectionPointContainer* pCPC;
		IConnectionPoint* pCP;

		hr = pUnkCP->QueryInterface(__uuidof(IConnectionPointContainer), (void**)&pCPC);
		if(SUCCEEDED(hr))
		{
			hr = pCPC->FindConnectionPoint(iid, &pCP);
			if(SUCCEEDED(hr))
			{
				hr = pCP->Unadvise(dw);
				pCP->Release();
			}
			pCPC->Release();
		}
	}
	else
		hr = E_INVALIDARG;

	return hr;
}
