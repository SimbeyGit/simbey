#pragma once

#include "MessageDigest.h"

typedef struct
{
	ULONG state[4];		// state (ABCD)
	ULONG count[2];		// number of bits, modulo 2^64 (lsb first)
	BYTE buffer[64];	// input buffer
} MD5_CTX, *LPMD5_CTX;

class CMd5 : public CMessageDigest
{
protected:
	MD5_CTX m_ctxKey;

public:
	CMd5 ();
	~CMd5 ();

	virtual VOID Reset (VOID);

	virtual VOID AddData (const BYTE* pcbData, UINT cData);

	virtual UINT GetDigestSize (VOID);
	virtual VOID GetDigest (LPBYTE lpDigest);

	virtual UINT GetHexKeySize (VOID);
	virtual VOID GetHexKey (LPSTR lpszKey);

protected:
	static VOID Update (LPMD5_CTX lpKey, const BYTE* pcbData, UINT cData);
	static VOID Transform (ULONG state[4], const BYTE block[64]);
	static VOID Encode (LPBYTE output, ULONG* input, UINT len);
	static VOID Decode (ULONG* output, const BYTE* input, UINT len);
	static VOID CompleteKey (LPMD5_CTX lpKey, BYTE Digest[16]);
};
