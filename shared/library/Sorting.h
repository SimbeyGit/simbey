#ifndef	_H_SORTING
#define	_H_SORTING

#include "Core\Array.h"

namespace Sorting
{
	typedef INT(WINAPI* COMPARECALLBACK)(LPVOID, LPVOID, LPVOID);

	VOID QuickSort (LPVOID lpBase, INT cItems, size_t nItemSize, COMPARECALLBACK lpfnCompare, LPVOID lpParam);
	LPVOID LinkSort (LPVOID p, UINT index, COMPARECALLBACK lpfnCompare, LPVOID lpParam, ULONG* pcount);

	template <typename T>
	void TQuickSort (T* pBase, int cItems, int (WINAPI* pfnCallback)(T* plhItem, T* prhItem, PVOID pParam), PVOID pParam)
	{
		QuickSort(pBase, cItems, sizeof(T), (COMPARECALLBACK)pfnCallback, pParam);
	}

	template <typename T>
	void TQuickSortTArray (TArray<T>* pArray, int (WINAPI* pfnCallback)(T* plhItem, T* prhItem, PVOID pParam), PVOID pParam)
	{
		T* pData;
		int cData;

		pArray->GetData(&pData, &cData);
		TQuickSort(pData, cData, pfnCallback, pParam);
	}

	template <typename T>
	void TQuickSortPtr (T** ppBase, int cItems, int (WINAPI* pfnCallback)(T** pplhItem, T** pprhItem, PVOID pParam), PVOID pParam)
	{
		QuickSort(ppBase, cItems, sizeof(T*), (COMPARECALLBACK)pfnCallback, pParam);
	}

	template <typename T>
	void TQuickSortTArrayPtr (TArray<T*>* pArray, int (WINAPI* pfnCallback)(T** pplhItem, T** pprhItem, PVOID pParam), PVOID pParam)
	{
		T** ppData;
		int cData;

		pArray->GetData(&ppData, &cData);
		TQuickSortPtr(ppData, cData, pfnCallback, pParam);
	}

	template <typename T>
	bool TBinaryFind (T* pvArray, INT cArray, T vValue, INT* piPosition)
	{
		// http://googleresearch.blogspot.com/2006/06/extra-extra-read-all-about-it-nearly.html
		INT iLeft = 0, iRight = cArray - 1, iMiddle;
		while(iLeft <= iRight)
		{
			iMiddle = ((unsigned)(iLeft + iRight)) >> 1;
			if(vValue > pvArray[iMiddle])
				iLeft = iMiddle + 1;
			else if(vValue < pvArray[iMiddle])
				iRight = iMiddle - 1;
			else
			{
				*piPosition = iMiddle;	// The item has been found at this position
				return true;
			}
		}
		*piPosition = iLeft;			// This is the point where the item should be inserted
		return false;
	}

	template <typename S, typename T, typename V>
	bool TBinaryFindUsing (T* pvArray, INT cArray, V vValue, INT* piPosition)
	{
		// http://googleresearch.blogspot.com/2006/06/extra-extra-read-all-about-it-nearly.html
		INT iLeft = 0, iRight = cArray - 1, iMiddle;
		while(iLeft <= iRight)
		{
			iMiddle = ((unsigned)(iLeft + iRight)) >> 1;

			V vSortValue = S::GetValue(pvArray[iMiddle]);
			if(vValue > vSortValue)
				iLeft = iMiddle + 1;
			else if(vValue < vSortValue)
				iRight = iMiddle - 1;
			else
			{
				*piPosition = iMiddle;	// The item has been found at this position
				return true;
			}
		}
		*piPosition = iLeft;			// This is the point where the item should be inserted
		return false;
	}

	template <typename S, typename T>
	bool TBinaryFindCustom (T* pvArray, INT cArray, T vValue, INT* piPosition)
	{
		// http://googleresearch.blogspot.com/2006/06/extra-extra-read-all-about-it-nearly.html
		INT iLeft = 0, iRight = cArray - 1, iMiddle;
		while(iLeft <= iRight)
		{
			iMiddle = ((unsigned)(iLeft + iRight)) >> 1;

			INT nCompare = S::Compare(pvArray[iMiddle], vValue);
			if(nCompare < 0)
				iLeft = iMiddle + 1;
			else if(nCompare > 0)
				iRight = iMiddle - 1;
			else
			{
				*piPosition = iMiddle;	// The item has been found at this position
				return true;
			}
		}
		*piPosition = iLeft;			// This is the point where the item should be inserted
		return false;
	}
};

#endif