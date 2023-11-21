#include <windows.h>
#include "Core\Assert.h"
#include "Sorting.h"

// Always compile this module for speed, not size
#pragma optimize("t", on)

// Testing shows that this is good value
#define CUTOFF		8

#define STKSIZ		(8 * sizeof(void*) - 2)

/***
*swap(a, b, width) - swap two elements
*
*Purpose:
*       swaps the two array elements of size width
*
*Entry:
*       char *a, *b = pointer to two elements to swap
*       size_t width = width in bytes of each array element
*
*Exit:
*       returns void
*
*Exceptions:
*
*******************************************************************************/
inline VOID WINAPI swap (LPBYTE a, LPBYTE b, size_t nItemSize)
{
	BYTE t;

	if(a != b)
	{
		// Do the swap one character at a time to avoid potential alignment problems.
        while(nItemSize--)
		{
            t = *a;
            *a++ = *b;
            *b++ = t;
        }
	}
}

/***
*shortsort(hi, lo, width, comp) - insertion sort for sorting short arrays
*shortsort_s(hi, lo, width, comp, context) - insertion sort for sorting short arrays
*
*Purpose:
*       sorts the sub-array of elements between lo and hi (inclusive)
*       side effects:  sorts in place
*       assumes that lo < hi
*
*Entry:
*       char *lo = pointer to low element to sort
*       char *hi = pointer to high element to sort
*       size_t width = width in bytes of each array element
*       int (*comp)() = pointer to function returning analog of strcmp for
*               strings, but supplied by user for comparing the array elements.
*               it accepts 2 pointers to elements, together with a pointer to a context
*               (if present). Returns neg if 1<2, 0 if 1=2, pos if 1>2.
*       void *context - pointer to the context in which the function is
*               called. This context is passed to the comparison function.
*
*Exit:
*       returns void
*
*Exceptions:
*
*******************************************************************************/
static void WINAPI ShortSort (LPBYTE lo, LPBYTE hi, size_t width, Sorting::COMPARECALLBACK lpfnCompare, LPVOID lpParam)
{
	BYTE *p, *max;

	// Note: in assertions below, i and j are alway inside original bound of array to sort.

	while(hi > lo)
	{
        /* A[i] <= A[j] for i <= j, j > hi */
        max = lo;
        for (p = lo+width; p <= hi; p += width)
		{
            /* A[i] <= A[max] for lo <= i < p */
            if (lpfnCompare(p, max, lpParam) > 0)
                max = p;

            /* A[i] <= A[max] for lo <= i <= p */
        }

        /* A[i] <= A[max] for lo <= i <= hi */

        swap(max, hi, width);

        /* A[i] <= A[hi] for i <= hi, so A[i] <= A[j] for i <= j, j >= hi */

        hi -= width;

        /* A[i] <= A[j] for i <= j, j > hi, loop top condition established */
    }
    /* A[i] <= A[j] for i <= j, j > lo, which implies A[i] <= A[j] for i < j,
       so array is sorted */
}

namespace Sorting
{
/***
*qsort(base, num, wid, comp) - quicksort function for sorting arrays
*
*Purpose:
*   quicksort the array of elements
*   side effects:  sorts in place
*   maximum array size is number of elements times size of elements,
*   but is limited by the virtual address space of the processor
*
*Entry:
*   char *base = pointer to base of array
*   size_t num  = number of elements in the array
*   size_t width = width in bytes of each array element
*   int (*comp)() = pointer to function returning analog of strcmp for
*           strings, but supplied by user for comparing the array elements.
*           it accepts 2 pointers to elements.
*           Returns neg if 1<2, 0 if 1=2, pos if 1>2.
*
*Exit:
*   returns void
*
*Exceptions:
*   Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

	VOID QuickSort (LPVOID lpBase, sysint cItems, size_t nItemSize, COMPARECALLBACK lpfnCompare, LPVOID lpParam)
	{
		// Note: the number of stack entries required is no more than
		// 1 + log2(num), so 30 is sufficient for any array
	    BYTE *lo, *hi;              // ends of sub-array currently sorting
		BYTE *mid;                  // points to middle of subarray
	    BYTE *loguy, *higuy;        // traveling pointers for partition step
		size_t size;                // size of the sub-array
	    BYTE *lostk[STKSIZ], *histk[STKSIZ];
		INT stkptr;                 // stack for saving sub-array to be processed

		Assert(lpBase && nItemSize > 0 && lpfnCompare);

	    if(cItems < 2)
		    return;                 // nothing to do

		stkptr = 0;                 // initialize stack

	    lo = (LPBYTE)lpBase;
		hi = (LPBYTE)lpBase + nItemSize * (cItems - 1);        // initialize limits

		// this entry point is for pseudo-recursion calling: setting
		// lo and hi and jumping to here is like recursion, but stkptr is
		// preserved, locals aren't, so we preserve stuff on the stack
recurse:

	    size = (hi - lo) / nItemSize + 1;        // number of el's to sort

		// below a certain size, it is faster to use a O(n^2) sorting method
	    if (size <= CUTOFF)
			ShortSort(lo, hi, nItemSize, lpfnCompare, lpParam);
	    else
		{
        /* First we pick a partitioning element.  The efficiency of the
           algorithm demands that we find one that is approximately the median
           of the values, but also that we select one fast.  We choose the
           median of the first, middle, and last elements, to avoid bad
           performance in the face of already sorted data, or data that is made
           up of multiple sorted runs appended together.  Testing shows that a
           median-of-three algorithm provides better performance than simply
           picking the middle element for the latter case. */

	        mid = lo + (size / 2) * nItemSize;      /* find middle element */

		    /* Sort the first, middle, last elements into order */
			if (lpfnCompare(lo, mid, lpParam) > 0)
				swap(lo, mid, nItemSize);
			if (lpfnCompare(lo, hi, lpParam) > 0)
				swap(lo, hi, nItemSize);
		    if (lpfnCompare(mid, hi, lpParam) > 0)
				swap(mid, hi, nItemSize);

        /* We now wish to partition the array into three pieces, one consisting
           of elements <= partition element, one of elements equal to the
           partition element, and one of elements > than it.  This is done
           below; comments indicate conditions established at every step. */

	        loguy = lo;
		    higuy = hi;

        /* Note that higuy decreases and loguy increases on every iteration,
           so loop must terminate. */
	        for(;;)
			{
            /* lo <= loguy < hi, lo < higuy <= hi,
               A[i] <= A[mid] for lo <= i <= loguy,
               A[i] > A[mid] for higuy <= i < hi,
               A[hi] >= A[mid] */

            /* The doubled loop is to avoid calling comp(mid,mid), since some
               existing comparison funcs don't work when passed the same
               value for both pointers. */

	            if (mid > loguy)
				{
		            do
					{
			            loguy += nItemSize;
				    } while (loguy < mid && lpfnCompare(loguy, mid, lpParam) <= 0);
	            }
		        if (mid <= loguy)
				{
			        do
					{
				        loguy += nItemSize;
					} while (loguy <= hi && lpfnCompare(loguy, mid, lpParam) <= 0);
	            }

            /* lo < loguy <= hi+1, A[i] <= A[mid] for lo <= i < loguy,
               either loguy > hi or A[loguy] > A[mid] */

		        do
				{
			        higuy -= nItemSize;
				} while (higuy > mid && lpfnCompare(higuy, mid, lpParam) > 0);

            /* lo <= higuy < hi, A[i] > A[mid] for higuy < i < hi,
               either higuy == lo or A[higuy] <= A[mid] */

	            if (higuy < loguy)
		            break;

            /* if loguy > hi or higuy == lo, then we would have exited, so
               A[loguy] > A[mid], A[higuy] <= A[mid],
               loguy <= hi, higuy > lo */

	            swap(loguy, higuy, nItemSize);

            /* If the partition element was moved, follow it.  Only need
               to check for mid == higuy, since before the swap,
               A[loguy] > A[mid] implies loguy != mid. */

		        if (mid == higuy)
			        mid = loguy;

            /* A[loguy] <= A[mid], A[higuy] > A[mid]; so condition at top
               of loop is re-established */
	        }

        /*     A[i] <= A[mid] for lo <= i < loguy,
               A[i] > A[mid] for higuy < i < hi,
               A[hi] >= A[mid]
               higuy < loguy
           implying:
               higuy == loguy-1
               or higuy == hi - 1, loguy == hi + 1, A[hi] == A[mid] */

        /* Find adjacent elements equal to the partition element.  The
           doubled loop is to avoid calling comp(mid,mid), since some
           existing comparison funcs don't work when passed the same value
           for both pointers. */

	        higuy += nItemSize;
		    if (mid < higuy)
			{
			    do  {
				    higuy -= nItemSize;
	            } while (higuy > mid && lpfnCompare(higuy, mid, lpParam) == 0);
		    }
			if (mid >= higuy)
			{
				do  {
					higuy -= nItemSize;
	            } while (higuy > lo && lpfnCompare(higuy, mid, lpParam) == 0);
		    }

        /* OK, now we have the following:
              higuy < loguy
              lo <= higuy <= hi
              A[i]  <= A[mid] for lo <= i <= higuy
              A[i]  == A[mid] for higuy < i < loguy
              A[i]  >  A[mid] for loguy <= i < hi
              A[hi] >= A[mid] */

        /* We've finished the partition, now we want to sort the subarrays
           [lo, higuy] and [loguy, hi].
           We do the smaller one first to minimize stack usage.
           We only sort arrays of length 2 or more.*/

	        if ( higuy - lo >= hi - loguy )
			{
		        if (lo < higuy)
				{
			        lostk[stkptr] = lo;
				    histk[stkptr] = higuy;
					++stkptr;
	            }                           /* save big recursion for later */

		        if (loguy < hi)
				{
			        lo = loguy;
				    goto recurse;           /* do small recursion */
	            }
		    }
			else
			{
				if (loguy < hi)
				{
	                lostk[stkptr] = loguy;
		            histk[stkptr] = hi;
			        ++stkptr;               /* save big recursion for later */
				}

	            if (lo < higuy)
				{
		            hi = higuy;
			        goto recurse;           /* do small recursion */
				}
	        }
		}

		// We have sorted the array, except for any pending sorts on the stack.
		// Check if there are any, and do them.

		--stkptr;
		if (stkptr >= 0)
		{
			lo = lostk[stkptr];
	        hi = histk[stkptr];
		    goto recurse;           /* pop subarray from stack */
		}
		else
			return;                 /* all subarrays done */
	}

// ************************************
// A Linked-List Memory Sort
// by Philip J. Erdelsky
// pje@efgh.com
// http://www.alumni.caltech.edu/~pje/
// http://efgh.com/software/index.html
// July 31, 1998
//
// A C function to sort a linked list in memory, using an
// arbitrary comparison function and a tape-merge algorithm,
// which is O(n log n) in all cases. The function is
// non-recursive and re-entrant, and changes only the links.
//
// ************************************
	LPVOID LinkSort (LPVOID p, UINT index, COMPARECALLBACK lpfnCompare, LPVOID lpParam, ULONG* pcount)
	{
		UINT base;
		ULONG block_size;

		struct record
		{
			struct record* next[1];
			/* other members not directly accessed by this function */
		};

		struct tape
		{
			struct record* first, *last;
			ULONG count;
		} tape[4];

		/* Distribute the records alternately to tape[0] and tape[1]. */

		tape[0].count = tape[1].count = 0L;
		tape[0].first = NULL;
		base = 0;
		while(p != NULL)
		{
			struct record* next = ((struct record*)p)->next[index];
			((struct record*)p)->next[index] = tape[base].first;
			tape[base].first = ((struct record*)p);
			tape[base].count++;
			p = next;
			base ^= 1;
		}

		/* If the list is empty or contains only a single record, then */
		/* tape[1].count == 0L and this part is vacuous.               */

		for(base = 0, block_size = 1L; tape[base+1].count != 0L; base ^= 2, block_size <<= 1)
		{
			INT dest;
			struct tape* tape0, *tape1;
			tape0 = tape + base;
			tape1 = tape + base + 1;
			dest = base ^ 2;
			tape[dest].count = tape[dest+1].count = 0;
			for(; tape0->count != 0; dest ^= 1)
			{
				ULONG n0, n1;
				struct tape* output_tape = tape + dest;
				n0 = n1 = block_size;
				while(1)
				{
					struct record* chosen_record;
					struct tape* chosen_tape;
					if(n0 == 0 || tape0->count == 0)
					{
						if(n1 == 0 || tape1->count == 0)
							break;
						chosen_tape = tape1;
						n1--;
					}
					else if(n1 == 0 || tape1->count == 0)
					{
						chosen_tape = tape0;
						n0--;
					}
					else if(lpfnCompare(tape0->first,tape1->first,lpParam) > 0)
					{
						chosen_tape = tape1;
						n1--;
					}
					else
					{
						chosen_tape = tape0;
						n0--;
					}
					chosen_tape->count--;
					chosen_record = chosen_tape->first;
					chosen_tape->first = chosen_record->next[index];
					if(output_tape->count == 0)
						output_tape->first = chosen_record;
					else
						output_tape->last->next[index] = chosen_record;
					output_tape->last = chosen_record;
					output_tape->count++;
				}
			}
		}

		if(tape[base].count > 1L)
			tape[base].last->next[index] = NULL;
		if(pcount)
			*pcount = tape[base].count;
		return tape[base].first;
	}
}