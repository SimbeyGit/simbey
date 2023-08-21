#pragma once

interface __declspec(uuid("A4F14C27-0EF6-44ee-91EF-FDCF9472113E")) IAttachable : public IUnknown
{
	virtual VOID AttachReference (IUnknown* punkOwner) = 0;
	virtual VOID DetachReference (IUnknown* punkOwner) = 0;
};

template <typename T>
inline void SafeAttach (T*& pDest, const T* pSrc, IUnknown* punkOwner)
{
	pDest = const_cast<T*>(pSrc);
	if(pDest)
		pDest->AttachReference(punkOwner);
}

template <typename T>
inline void SafeDetach (T*& pReference, IUnknown* punkOwner)
{
	if(pReference)
	{
		T* pPtr = pReference;
		pReference = NULL;
		pPtr->DetachReference(punkOwner);
	}
}
