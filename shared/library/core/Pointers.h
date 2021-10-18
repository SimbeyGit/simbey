#pragma once

#include <OCIdl.h>

template <typename T>
inline void SwapData (T& vA, T& vB)
{
	T vIntermediate = vA;
	vA = vB;
	vB = vIntermediate;
}

template <typename T>
inline void ReplaceInterface (T*& lpDest, T* lpSrc)
{
	if(lpSrc)
		lpSrc->AddRef();

	if(lpDest)
		lpDest->Release();

	lpDest = lpSrc;
}

template <typename T>
inline void SetInterface (T*& pDest, const T* pSrc)
{
	pDest = const_cast<T*>(pSrc);
	if(pDest)
		pDest->AddRef();
}

template <typename T>
inline HRESULT EnsureInterface (T*& pDest, const T* pSrc)
{
	pDest = const_cast<T*>(pSrc);
	if(pSrc)
	{
		pDest->AddRef();
		return S_OK;
	}
	return E_POINTER;
}

template <typename T>
inline void SafeRelease (T*& lpPtr)
{
	if(lpPtr)
	{
		T* lpTemp = lpPtr;
		lpPtr = NULL;
		lpTemp->Release();
	}
}

template <typename T>
inline void SafeReleaseAssertFinal (T*& p)
{
	if(p)
	{
		T* pTemp = p;
		p = NULL;

		// We're expecting to release the last reference here.
		SideAssertCompare(pTemp->Release(), 0);
	}
}

template <typename T>
inline void SafeDelete (T*& lpPtr)
{
	if(lpPtr)
	{
		T* lpTemp = lpPtr;
		lpPtr = NULL;
		__delete lpTemp;
	}
}

template <typename T>
inline void SafeDeleteArray (T*& lpArray)
{
	if(lpArray)
	{
		T* lpTemp = lpArray;
		lpArray = NULL;
		__delete_array lpTemp;
	}
}

template <typename T, typename C>
inline void SafeDeleteArrayCount (T*& lpArray, C& cArray)
{
	if(lpArray)
	{
		T* lpTemp = lpArray;
		cArray = 0;
		lpArray = NULL;
		__delete_array lpTemp;
	}
}

inline void SafeCloseHandle (HANDLE& hHandle)
{
	if(hHandle)
	{
		HANDLE hTemp = hHandle;
		hHandle = NULL;
		CloseHandle(hTemp);
	}
}

inline void SafeCloseFileHandle (HANDLE& hHandle)
{
	if(INVALID_HANDLE_VALUE != hHandle)
	{
		HANDLE hTemp = hHandle;
		hHandle = INVALID_HANDLE_VALUE;
		CloseHandle(hTemp);
	}
}

template<typename T>
inline void SafeDestroy (T*& lpPtr)
{
	if(lpPtr)
	{
		T* lpTemp = lpPtr;
		lpPtr = NULL;
		lpTemp->Destroy();
	}
}

inline void SafeDestroyWindow (HWND& hwnd)
{
	if(hwnd)
	{
		HWND hwndTemp = hwnd;
		hwnd = NULL;
		DestroyWindow(hwndTemp);
	}
}

// SafeDeleteGdiObject() safely deletes any GDI object that can be
// deleted using DeleteObject().  Objects that can be safely deleted
// using SafeDeleteGdiObject() include HBITMAP, HPEN, HBRUSH, HFONT.
template<typename T>
inline void SafeDeleteGdiObject(T& hObject)
{
	if(hObject)
	{
		// T can be HBITMAP, HPEN, HBRUSH, HFONT, but it must convert
		// to HGDIOBJ automatically.
		HGDIOBJ hTemp = hObject;
		hObject = NULL;
		::DeleteObject(hTemp);
	}
}

template <typename T>
inline VOID TSecureDeleteArray (T*& pArray, INT cbArray)
{
	if(pArray)
	{
		T* pTemp = pArray;
		pArray = NULL;
		SecureZeroMemory(pTemp, cbArray);
		__delete_array pTemp;
	}
}

template <typename T>
inline VOID TSecureDeleteString (T*& ptzString)
{
	if(ptzString)
		TSecureDeleteArray(ptzString, TStrLenAssert(ptzString) * sizeof(T));
}

template <class T>
class TStackRef
{
private:
	T* m_pObject;

public:
	TStackRef ()
	{
		m_pObject = NULL;
	}

	TStackRef (T* pObject)
	{
		SetInterface(m_pObject, pObject);
	}

	TStackRef (const TStackRef& srObject)
	{
		SetInterface(m_pObject, srObject.m_pObject);
	}

	~TStackRef ()
	{
		if(m_pObject)
			m_pObject->Release();
	}

	TStackRef<T>& operator= (const TStackRef<T>& srObject)
	{
		ReplaceInterface(m_pObject, srObject.m_pObject);
		return *this;
	}

	operator T* () const
	{
		return m_pObject;
	}

	T& operator* () const
	{
		Assert(m_pObject);
		return *m_pObject;
	}

	T** operator& ()
	{
		Assert(NULL == m_pObject);
		return &m_pObject;
	}

	T* operator-> () const
	{
		Assert(m_pObject);
		return m_pObject;
	}

	operator bool () const
	{
		return (NULL != m_pObject);
	}

	bool operator! () const
	{
		return (NULL == m_pObject);
	}

	bool operator== (__in_opt T* pObject) const
	{
		return m_pObject == pObject;
	}

	T* operator= (__in_opt T* pObject)
	{
		if(*this != pObject)
		{
			ReplaceInterface(m_pObject, pObject);
			return pObject;
		}
		return *this;
	}

	int ComparePtr (const T* pObject) const
	{
		int nCompare = 0;
		if(m_pObject > pObject)
			nCompare = 1;
		else if(m_pObject < pObject)
			nCompare = -1;
		return nCompare;
	}

	VOID Attach (__in_opt T* pObject)
	{
		// Release m_pObject and attach to (take ownership of) pObject.
		if(m_pObject)
			m_pObject->Release();
		m_pObject = pObject;
	}

	T* Detach (VOID)
	{
		// Detach and return (release ownership of) m_pObject.
		T* pObject = m_pObject;
		m_pObject = NULL;
		return pObject;
	}

	HRESULT CopyTo (__deref_out_opt T** ppObject) const
	{
		Assert(ppObject);
		if(NULL == ppObject)
			return E_POINTER;
		SetInterface(*ppObject, m_pObject);
		return S_OK;
	}

	VOID Set (__in_opt T* pObject)
	{
		ReplaceInterface(m_pObject, pObject);
	}

	template <class Q>
	HRESULT QueryInterface (__deref_out_opt Q** ppObject) const
	{
		Assert(ppObject);
		return m_pObject->QueryInterface(__uuidof(Q), reinterpret_cast<PVOID*>(ppObject));
	}

	VOID Release (VOID)
	{
		SafeRelease(m_pObject);
	}

	template <typename TCastType>
	TCastType* StaticCast (VOID)
	{
		return static_cast<TCastType*>(m_pObject);
	}

	template <typename TCastType>
	const TCastType* ConstStaticCast (VOID)
	{
		return static_cast<const TCastType*>(m_pObject);
	}

	__checkReturn HRESULT Connect (__in IUnknown* pUnk, __in const IID& iid, __out DWORD* pdwCookie)
	{
		return ComConnect(m_pObject, pUnk, iid, pdwCookie);
	}

	HRESULT Disconnect (const IID& iid, DWORD dwCookie)
	{
		return ComDisconnect(m_pObject, iid, dwCookie);
	}

	HRESULT CoCreateInstance (REFCLSID rclsid, __in_opt IUnknown* pUnkOuter = NULL, DWORD dwClsContext = CLSCTX_ALL)
	{
		Assert(NULL == m_pObject);
		return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, __uuidof(T), reinterpret_cast<PVOID*>(&m_pObject));
	}

	HRESULT CoCreateInstance (LPCOLESTR pcwzProgID, IUnknown* pUnkOuter = NULL, DWORD dwClsContext = CLSCTX_ALL)
	{
		CLSID clsid;
		HRESULT hr = CLSIDFromProgID(pcwzProgID, &clsid);
		if(SUCCEEDED(hr))
			hr = CoCreateInstance(clsid, pUnkOuter, dwClsContext);
		return hr;
	}
};

#define	CStackRef		TStackRef<IUnknown>

__forceinline PVOID LongToVoid (LONG x)
{
	return reinterpret_cast<PVOID>(static_cast<LONG_PTR>(x));
}

__forceinline PVOID ULongToVoid (ULONG x)
{
	return reinterpret_cast<PVOID>(static_cast<ULONG_PTR>(x));
}

template <typename TSignature>
inline HRESULT TGetFunction (HMODULE hModule, PCSTR pcszName, __deref_out TSignature* pfn)
{
	*pfn = reinterpret_cast<TSignature>(GetProcAddress(hModule, pcszName));
	if(NULL == *pfn)
		return HRESULT_FROM_WIN32(GetLastError());
	return S_OK;
}

#define	PTR_DIFF(x,y)				((LPBYTE)x - (LPBYTE)y)
#define	TOGGLE_INT_VALUE(v, a, b)	v = (v) ^ ((a) ^ (b))

#define	WIDEN2(x)					L ## x
#define	WIDEN(x)					WIDEN2(x)

#define	StaticLength(x)				(ARRAYSIZE(x) - 1)

#define	LSP(x)						StaticLength(x), x
#define	SLP(x)						x, StaticLength(x)

#define	MAKE_STRING(x)				#x
#define	MAKE_STRING_PASSTHRU(x)		MAKE_STRING(x)
