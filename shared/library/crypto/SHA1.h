#pragma once

#include "MessageDigest.h"

struct SHA1_CTX
{
    UINT32 Message_Digest[5];   /* Message Digest (output)          */

    UINT32 Length_Low;          /* Message length in bits           */
    UINT32 Length_High;         /* Message length in bits           */

    BYTE Message_Block[64];     /* 512-bit message blocks           */
    int Message_Block_Index;    /* Index into message block array   */

    int Corrupted;              /* Is the message digest corruped?  */
};

class CSHA1 : public CMessageDigest
{
private:
    SHA1_CTX m_ctxKey;

public:
	CSHA1 ();
	~CSHA1 ();

	virtual VOID Reset (VOID);

	virtual VOID AddData (const BYTE* pcbData, UINT cData);

	virtual UINT GetDigestSize (VOID);
	virtual VOID GetDigest (LPBYTE lpDigest);

	virtual UINT GetHexKeySize (VOID);
	virtual VOID GetHexKey (LPSTR lpszKey);

private:
	static VOID Update (SHA1_CTX* ppKey, const BYTE* pcbData, UINT cData);
	static VOID CompleteKey (SHA1_CTX* pKey);
};
