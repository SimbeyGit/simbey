#pragma once

#include "ContainerAllocators.h"

#ifndef	_MIN_ALLOC_ARRAY
	#define	_MIN_ALLOC_ARRAY			64
#endif

struct DoubleGrowth
{
	static const sysint initial_allocation = _MIN_ALLOC_ARRAY;

	static HRESULT Grow (sysint cItems, __out sysint& cNewMax)
	{
		HRESULT hr = S_OK;

		if(0 < cItems)
		{
			hr = HrSafeMultSysInt(cItems, static_cast<sysint>(2), &cNewMax);
		}
		else
		{
			cNewMax = initial_allocation;
		}

		return hr;
	}

	static HRESULT Shrink (sysint cItems, sysint cOldMax, __out sysint& cNewMax)
	{
		HRESULT hr = S_FALSE;
		sysint cHalfInitial = initial_allocation >> 1;
		sysint cHalfMax = cOldMax >> 1;
		sysint cThreshold = cHalfMax - cHalfInitial;

		// If the item count is less than half the maximum count minus half the initial allocation, and
		// the maximum count is greater than the initial allocation, then reduce the maximum by 50%.
		if(cItems < cThreshold && initial_allocation < cOldMax)
		{
			cNewMax = cHalfMax;
			hr = S_OK;
		}

		return hr;
	}
};

struct LinearGrowth
{
	static const sysint linear_allocation = 4;

	static HRESULT Grow (sysint cItems, __out sysint& cNewMax)
	{
		return HrSafeAdd(cItems, linear_allocation, &cNewMax);
	}

	static HRESULT Shrink (sysint cItems, sysint cOldMax, __out sysint& cNewMax)
	{
		return DoubleGrowth::Shrink(cItems, cOldMax, cNewMax);
	}
};

struct SingleGrowth
{
	static const sysint linear_allocation = 1;

	static HRESULT Grow (sysint cItems, __out sysint& cNewMax)
	{
		return HrSafeAdd(cItems, linear_allocation, &cNewMax);
	}

	static HRESULT Shrink (sysint cItems, sysint cOldMax, __out sysint& cNewMax)
	{
		return DoubleGrowth::Shrink(cItems, cOldMax, cNewMax);
	}
};

struct DefaultTraits
{
	typedef crt_heap THeap;
	typedef DoubleGrowth TUsage;
};

struct DefaultReallocateTraits : DefaultTraits
{
	typedef struct {} UseReallocate;
};

struct WindowsHeapTraits
{
	typedef win_heap THeap;
	typedef DoubleGrowth TUsage;
};

struct WindowsReallocateHeapTraits : WindowsHeapTraits
{
	typedef struct {} UseReallocate;
};
