#pragma once

#include "MessageDigest.h"

//SHA-256 block size
#define	SHA256_BLOCK_SIZE	64

//SHA-256 digest size
#define	SHA256_DIGEST_SIZE	32

struct SHA256_CTX
{
	union
	{
		UINT32 h[SHA256_DIGEST_SIZE / 4];
		BYTE digest[SHA256_DIGEST_SIZE];
	};
	union
	{
		UINT32 w[SHA256_BLOCK_SIZE / 4];
		BYTE buffer[SHA256_BLOCK_SIZE];
	};
	UINT32 size;
	UINT64 totalSize;
};

class CSHA256 : public CMessageDigest
{
private:
	SHA256_CTX m_ctxKey;

public:
	CSHA256 ();
	~CSHA256 ();

	virtual VOID Reset (VOID);

	virtual VOID AddData (const BYTE* pcbData, UINT cData);

	virtual UINT GetDigestSize (VOID);
	virtual VOID GetDigest (LPBYTE lpDigest);

	virtual UINT GetHexKeySize (VOID);
	virtual VOID GetHexKey (LPSTR lpszKey);

private:
	static VOID Update (SHA256_CTX* ppKey, const BYTE* pcbData, UINT cData);
	static VOID CompleteKey (SHA256_CTX* pKey);
};
