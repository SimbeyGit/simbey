#pragma once

#include "MessageDigest.h"

#define	SHA3_KECCAK_SPONGE_WORDS	(((1600)/8/*bits to byte*/) / sizeof(ULONGLONG))

struct SHA3_CONTEXT
{
	ULONGLONG saved;		/* the portion of the input message that we didn't consume yet */
	union					/* Keccak's state */
	{
		ULONGLONG s[SHA3_KECCAK_SPONGE_WORDS];
		BYTE sb[SHA3_KECCAK_SPONGE_WORDS * 8];
	};
	UINT byteIndex;			/* 0..7--the next byte after the set one
							* (starts from 0; 0--none are buffered) */
	UINT wordIndex;			/* 0..24--the next word to integrate input
							* (starts from 0) */
	UINT capacityWords;		/* the double size of the hash output in
							* words (e.g. 16 for Keccak 512) */
};

class CSHA3 : public CMessageDigest
{
protected:
	SHA3_CONTEXT m_ctxKey;
	UINT m_nBitSize;

public:
	CSHA3 (UINT nBitSize, bool fUseKeccak = false);
	~CSHA3 ();

	inline UINT GetBitSize (VOID) { return m_nBitSize; }

	virtual VOID Reset (VOID);

	virtual VOID AddData (const BYTE* pcbData, UINT cData);

	virtual UINT GetDigestSize (VOID);
	virtual VOID GetDigest (LPBYTE lpDigest);

	virtual UINT GetHexKeySize (VOID);
	virtual VOID GetHexKey (LPSTR lpszKey);

protected:
	VOID Finalize (SHA3_CONTEXT* pctx);
};
