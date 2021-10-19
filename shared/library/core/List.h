#pragma once

#include "CoreDefs.h"

/////////////////////////////////////////////////////////////////////////////////////
// GENERIC LIST
/////////////////////////////////////////////////////////////////////////////////////

template<typename TItem>
class TList
{
public:
	struct LIST_NODE
	{
		LIST_NODE* pNext;
		LIST_NODE* pPrev;
		TItem vItem;
	};

protected:
	LIST_NODE* m_pHead;
	LIST_NODE* m_pTail;
	sysint m_cNodes;

public:
	TList () : m_pHead(NULL), m_pTail(NULL), m_cNodes(0)
	{
	}

	virtual ~TList ()
	{
		Clear();
	}

	TItem& operator[](sysint n)
	{
		return GetNode(n)->vItem;
	}

	const TItem& operator[](sysint n) const
	{
		return const_cast<TList<TItem>*>(this)->GetNode(n)->vItem;
	}

	inline sysint Length (VOID) const
	{
		return m_cNodes;
	}

	HRESULT AddBefore (LIST_NODE* pInsertPoint, const TItem* pvItem)
	{
		HRESULT hr = E_OUTOFMEMORY;
		LIST_NODE* p = __new LIST_NODE;
		if(p)
		{
			p->vItem = *pvItem;
			AddNodeBefore(pInsertPoint, p);
			hr = S_OK;
		}
		return hr;
	}

	HRESULT Append (const TItem* pvItem)
	{
		HRESULT hr = E_OUTOFMEMORY;
		LIST_NODE* p = __new LIST_NODE;
		if(p)
		{
			p->pPrev = m_pTail;
			p->pNext = NULL;
			p->vItem = *pvItem;

			if(m_pTail)
				m_pTail->pNext = p;
			else
				m_pHead = p;

			m_pTail = p;

			m_cNodes++;
			hr = S_OK;
		}
		return hr;
	}

	inline HRESULT Append (const TItem& vItem)
	{
		return Append(&vItem);
	}

	HRESULT Push (const TItem* pvItem)
	{
		return AddBefore(NULL, pvItem);
	}

	inline HRESULT Push (const TItem& vItem)
	{
		return Push(&vItem);
	}

	VOID Remove (LIST_NODE* p)
	{
		Assert(p && 0 < m_cNodes);

		if(p->pPrev)
			p->pPrev->pNext = p->pNext;
		else
			m_pHead = p->pNext;

		if(p->pNext)
			p->pNext->pPrev = p->pPrev;
		else
			m_pTail = p->pPrev;

		__delete p;

		m_cNodes--;
	}

	HRESULT Remove (sysint nPosition, TItem* pvItem)
	{
		HRESULT hr = E_FAIL;
		if(0 <= nPosition && nPosition < m_cNodes)
		{
			LIST_NODE* p = GetNode(nPosition);
			Assert(p);

			if(pvItem)
				*pvItem = p->vItem;

			Remove(p);

			hr = S_OK;
		}
		return hr;
	}

	VOID Clear (VOID)
	{
		while(m_pHead)
		{
			LIST_NODE* pTemp = m_pHead;
			m_pHead = m_pHead->pNext;
			__delete pTemp;
		}
		m_pTail = NULL;
		m_cNodes = 0;
	}

	LIST_NODE* GetHead (VOID)
	{
		return m_pHead;
	}

	const LIST_NODE* GetHead (VOID) const
	{
		return m_pHead;
	}

	LIST_NODE* GetTail (VOID)
	{
		return m_pTail;
	}

	const LIST_NODE* GetTail (VOID) const
	{
		return m_pTail;
	}

	inline HRESULT Dequeue (TItem* pvItem)
	{
		return Remove(m_cNodes - 1, pvItem);
	}

	// Direct node manipulation

	inline VOID PushNode (LIST_NODE* pNode)
	{
		return AddNodeBefore(NULL, pNode);
	}

	inline VOID AppendNode (LIST_NODE* pNode)
	{
		Assert(pNode && NULL == pNode->pNext);

		if(m_pTail)
			m_pTail->pNext = pNode;
		else
			m_pHead = pNode;
		pNode->pPrev = m_pTail;
		m_pTail = pNode;
		m_cNodes++;
	}

	inline VOID AddNodeBefore (LIST_NODE* pInsertPoint, LIST_NODE* p)
	{
		if(pInsertPoint)
			pInsertPoint = pInsertPoint->pPrev;

		if(pInsertPoint)
		{
			p->pPrev = pInsertPoint;
			p->pNext = pInsertPoint->pNext;

			if(p->pNext)
				p->pNext->pPrev = p;
			pInsertPoint->pNext = p;
		}
		else
		{
			p->pPrev = NULL;
			p->pNext = m_pHead;

			if(m_pHead)
				m_pHead->pPrev = p;
			else
				m_pTail = p;

			m_pHead = p;
		}

		m_cNodes++;
	}

	inline VOID UnlinkNode (LIST_NODE* pNode)
	{
		Assert(pNode);

		if(m_pTail == pNode)
			m_pTail = pNode->pPrev;
		if(m_pHead == pNode)
			m_pHead = pNode->pNext;

		if(pNode->pNext)
			pNode->pNext->pPrev = pNode->pPrev;
		if(pNode->pPrev)
			pNode->pPrev->pNext = pNode->pNext;

		m_cNodes--;
	}

	LIST_NODE* UnlinkHead (VOID)
	{
		LIST_NODE* pHead = m_pHead;

		if(pHead)
			UnlinkNode(pHead);

		return pHead;
	}

	LIST_NODE* UnlinkTail (VOID)
	{
		LIST_NODE* pTail = m_pTail;

		if(pTail)
			UnlinkNode(pTail);

		return pTail;
	}

	LIST_NODE* Detach (VOID)
	{
		LIST_NODE* pHead = m_pHead;
		m_cNodes = 0;
		m_pHead = NULL;
		return pHead;
	}

protected:
	LIST_NODE* GetNode (const sysint nPosition)
	{
		LIST_NODE* p;

		Assert(0 <= nPosition && nPosition < m_cNodes);

		if(nPosition == m_cNodes - 1)	// This is a shortcut for Dequeue()
			p = m_pTail;
		else
		{
			p = m_pHead;
			for(sysint i = 0; i < nPosition; i++)
				p = p->pNext;
		}

		return p;
	}
};