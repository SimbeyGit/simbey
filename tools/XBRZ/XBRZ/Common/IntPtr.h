#pragma once

class CIntPtr
{
public:
	int* m_pData;
	int m_nPtr;

public:
	CIntPtr (int* pData) :
		m_pData(pData),
		m_nPtr(0)
	{
	}

	void Position (int nPtr)
	{
		m_nPtr = nPtr;
	}

	int Get ()
	{
		return m_pData[m_nPtr];
	}

	void Set (int nValue)
	{
		m_pData[m_nPtr] = nValue;
	}
};
