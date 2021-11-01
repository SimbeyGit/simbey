#pragma once

#include "Assert.h"
#include "Array.h"

/////////////////////////////////////////////////////////////////////////////////////
// STACK
/////////////////////////////////////////////////////////////////////////////////////

template<typename TItem>
class TStack
{
protected:
	TArray<TItem> m_aStack;

public:
	TStack () {}
	~TStack () {}

	HRESULT Push (const TItem* pcvItem)
	{
		return m_aStack.Append(pcvItem);
	}

	HRESULT Push (const TItem& cvItem)
	{
		return m_aStack.Append(cvItem);
	}

	HRESULT Pop (TItem* pvItem)
	{
		return m_aStack.RemoveChecked(m_aStack.Length() - 1, pvItem);
	}

	HRESULT Pop (VOID)
	{
		return m_aStack.RemoveChecked(m_aStack.Length() - 1, NULL);
	}

	TItem* Top (VOID)
	{
		return m_aStack.GetItemPtr(m_aStack.Length() - 1);
	}

	sysint Length (VOID)
	{
		return m_aStack.Length();
	}

	TItem* operator[] (sysint n)
	{
		if(m_aStack.Length() > n && n >= 0)
			return m_aStack.GetItemPtr(n);
		else
			return NULL;
	}

	const TItem* GetStack (VOID)
	{
		return m_aStack.GetItemPtr(0);
	}
};
