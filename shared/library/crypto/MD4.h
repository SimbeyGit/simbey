#pragma once

#include "MessageDigest.h"

// http://www.faqs.org/rfcs/rfc1320.html

typedef struct
{
	ULONG state[4];		// state (ABCD)
	ULONG count[2];		// number of bits, modulo 2^64 (lsb first)
	BYTE buffer[64];	// input buffer
} MD4_CTX, *LPMD4_CTX;

class CMd4 : public CMessageDigest
{
protected:
	MD4_CTX m_ctxKey;

public:
	CMd4 ();
	~CMd4 ();

	virtual VOID Reset (VOID);

	virtual VOID AddData (const BYTE* pcbData, UINT cData);

	virtual UINT GetDigestSize (VOID);
	virtual VOID GetDigest (LPBYTE lpDigest);

	virtual UINT GetHexKeySize (VOID);
	virtual VOID GetHexKey (LPSTR lpszKey);

protected:
	static VOID Update (LPMD4_CTX lpKey, const BYTE* pcbData, UINT cData);
	static VOID Transform (ULONG state[4], const BYTE block[64]);
	static VOID Encode (LPBYTE output, ULONG* input, UINT len);
	static VOID Decode (ULONG* output, const BYTE* input, UINT len);
	static VOID CompleteKey (LPMD4_CTX lpKey, BYTE Digest[16]);
};
