#include <malloc.h>
#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "Hmac.h"

///////////////////////////////////////////////////////////////////////////////
// CHmac
///////////////////////////////////////////////////////////////////////////////

CHmac::CHmac ()
{
}

CHmac::~CHmac ()
{
}

VOID CHmac::HmacInitMicrosoft (const BYTE* pcbKey, INT cKey)
{
	if(cKey > HMAC_PAD_SIZE)
		cKey = HMAC_PAD_SIZE;

	HmacInitialize(pcbKey, cKey);
}

VOID CHmac::Reset (VOID)
{
	m_pHash->Reset();
}

VOID CHmac::AddData (const BYTE* pcbData, UINT cData)
{
	m_pHash->AddData(pcbData, cData);
}

UINT CHmac::GetDigestSize (VOID)
{
	return m_pHash->GetDigestSize();
}

UINT CHmac::GetHexKeySize (VOID)
{
	return m_pHash->GetHexKeySize();
}

VOID CHmac::GetHexKey (LPSTR lpszKey)
{
	if(lpszKey)
	{
		UINT cbDigest = m_pHash->GetDigestSize();
		BYTE* pbDigest = __new BYTE[cbDigest];
		if(pbDigest)
		{
			GetDigest(pbDigest);	// Call the subclass to compute the Hmac digest
			KeyToHex(pbDigest,cbDigest,lpszKey);
			__delete_array pbDigest;
		}
		else
			*lpszKey = '\0';
	}
}

VOID CHmac::HmacInitialize (const BYTE* pcbKey, INT cKey)
{
	BYTE kipad[HMAC_PAD_SIZE];
	INT i;

	Assert(cKey <= sizeof(kipad));

	// Zero from cKey to the end
	for(i = cKey; i < HMAC_PAD_SIZE; i++)
	{
		kipad[i] = 0;
		m_kopad[i] = 0;
	}

	// Copy the key data
	for(i = 0; i < cKey; i++)
	{
		kipad[i] = pcbKey[i];
		m_kopad[i] = pcbKey[i];
	}

	for(i = 0; i < HMAC_PAD_SIZE; i++)
	{
		kipad[i] ^= 0x36;
		m_kopad[i] ^= 0x5c;
	}

	Reset();
	AddData(kipad, sizeof(kipad));
}

///////////////////////////////////////////////////////////////////////////////
// CHmacMd5
///////////////////////////////////////////////////////////////////////////////

CHmacMd5::CHmacMd5 ()
{
	m_pHash = &m_md5;
}

VOID CHmacMd5::HmacInitRfc2104 (const BYTE* pcbKey, INT cKey)
{
	BYTE TempKey[16];

	if(cKey > HMAC_PAD_SIZE)
	{
		CMd5 TempMd5;

		TempMd5.AddData(pcbKey,cKey);
		TempMd5.GetDigest(TempKey);

		pcbKey = TempKey;
		cKey = sizeof(TempKey);
	}

	HmacInitialize(pcbKey, cKey);
}

VOID CHmacMd5::GetDigest (LPBYTE lpDigest)
{
	CMd5 Outer;

	m_md5.GetDigest(lpDigest);

	Outer.AddData(m_kopad, sizeof(m_kopad));
	Outer.AddData(lpDigest, 16);
	Outer.GetDigest(lpDigest);
}

///////////////////////////////////////////////////////////////////////////////
// CHmacSHA1
///////////////////////////////////////////////////////////////////////////////

CHmacSHA1::CHmacSHA1 ()
{
	m_pHash = &m_sha1;
}

VOID CHmacSHA1::HmacInitRfc2104 (const BYTE* pcbKey, INT cKey)
{
	BYTE TempKey[20];

	if(cKey > HMAC_PAD_SIZE)
	{
		CSHA1 TempSHA1;

		TempSHA1.AddData(pcbKey,cKey);
		TempSHA1.GetDigest(TempKey);

		pcbKey = TempKey;
		cKey = sizeof(TempKey);
	}

	HmacInitialize(pcbKey, cKey);
}

VOID CHmacSHA1::GetDigest (LPBYTE lpDigest)
{
	CSHA1 Outer;

	m_sha1.GetDigest(lpDigest);

	Outer.AddData(m_kopad, sizeof(m_kopad));
	Outer.AddData(lpDigest, 20);
	Outer.GetDigest(lpDigest);
}

///////////////////////////////////////////////////////////////////////////////
// CHmacSHA256
///////////////////////////////////////////////////////////////////////////////

CHmacSHA256::CHmacSHA256 ()
{
	m_pHash = &m_sha256;
}

VOID CHmacSHA256::HmacInitRfc2104 (const BYTE* pcbKey, INT cKey)
{
	BYTE TempKey[SHA256_DIGEST_SIZE];

	if(cKey > HMAC_PAD_SIZE)
	{
		CSHA256 TempSHA256;

		TempSHA256.AddData(pcbKey,cKey);
		TempSHA256.GetDigest(TempKey);

		pcbKey = TempKey;
		cKey = sizeof(TempKey);
	}

	HmacInitialize(pcbKey, cKey);
}

VOID CHmacSHA256::GetDigest (LPBYTE lpDigest)
{
	CSHA256 Outer;

	m_sha256.GetDigest(lpDigest);

	Outer.AddData(m_kopad, sizeof(m_kopad));
	Outer.AddData(lpDigest, SHA256_DIGEST_SIZE);
	Outer.GetDigest(lpDigest);
}

///////////////////////////////////////////////////////////////////////////////
// CHmacSHA3
///////////////////////////////////////////////////////////////////////////////

CHmacSHA3::CHmacSHA3 (UINT nBitSize) :
	m_sha3(nBitSize)
{
	m_pHash = &m_sha3;
}

VOID CHmacSHA3::HmacInitRfc2104 (const BYTE* pcbKey, INT cKey)
{
	PBYTE pbTempKey = NULL;

	if(cKey > HMAC_PAD_SIZE)
	{
		CSHA3 TempSHA3(m_sha3.GetBitSize());
		UINT cbDigest = TempSHA3.GetDigestSize();

		pbTempKey = reinterpret_cast<PBYTE>(_malloca(cbDigest));

		TempSHA3.AddData(pcbKey,cKey);
		TempSHA3.GetDigest(pbTempKey);

		pcbKey = pbTempKey;
		cKey = cbDigest;
	}

	HmacInitialize(pcbKey, cKey);

	_freea(pbTempKey);
}

VOID CHmacSHA3::GetDigest (LPBYTE lpDigest)
{
	CSHA3 Outer(m_sha3.GetBitSize());

	m_sha3.GetDigest(lpDigest);

	Outer.AddData(m_kopad, sizeof(m_kopad));
	Outer.AddData(lpDigest, m_sha3.GetDigestSize());
	Outer.GetDigest(lpDigest);
}

///////////////////////////////////////////////////////////////////////////////
// CHmacWhirlpool
///////////////////////////////////////////////////////////////////////////////

CHmacWhirlpool::CHmacWhirlpool ()
{
	m_pHash = &m_whirlpool;
}

VOID CHmacWhirlpool::HmacInitRfc2104 (const BYTE* pcbKey, INT cKey)
{
	BYTE TempKey[DIGESTBYTES];

	if(cKey > HMAC_PAD_SIZE)
	{
		CWhirlpool TempWhirlpool;

		TempWhirlpool.AddData(pcbKey,cKey);
		TempWhirlpool.GetDigest(TempKey);

		pcbKey = TempKey;
		cKey = sizeof(TempKey);
	}

	HmacInitialize(pcbKey, cKey);
}

VOID CHmacWhirlpool::GetDigest (LPBYTE lpDigest)
{
	CWhirlpool Outer;

	m_whirlpool.GetDigest(lpDigest);

	Outer.AddData(m_kopad, sizeof(m_kopad));
	Outer.AddData(lpDigest, DIGESTBYTES);
	Outer.GetDigest(lpDigest);
}
