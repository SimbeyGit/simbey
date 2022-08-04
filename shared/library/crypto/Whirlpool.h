#ifndef _H_WHIRLPOOL
#define _H_WHIRLPOOL

#include "MessageDigest.h"

#define DIGESTBYTES 64
#define DIGESTBITS  (8*DIGESTBYTES) /* 512 */

#define WBLOCKBYTES 64
#define WBLOCKBITS  (8*WBLOCKBYTES) /* 512 */

#define LENGTHBYTES 32
#define LENGTHBITS  (8*LENGTHBYTES) /* 256 */

typedef struct
{
	BYTE bitLength[LENGTHBYTES];	// global number of hashed bits (256-bit counter)
	BYTE buffer[WBLOCKBYTES];		// buffer of data to hash
	INT bufferBits;					// current number of bits on the buffer
	INT bufferPos;					// current (possibly incomplete) byte slot on the buffer
	ULONGLONG hash[DIGESTBYTES/8];	// the hashing state
} WPKEY, *LPWPKEY;

class CWhirlpool : public CMessageDigest
{
protected:
	WPKEY m_wpKey;

public:
	CWhirlpool ();
	~CWhirlpool ();

	virtual VOID Reset (VOID);

	virtual VOID AddData (const BYTE* pcbData, UINT cData);

	virtual UINT GetDigestSize (VOID);
	virtual VOID GetDigest (LPBYTE lpDigest);

	virtual UINT GetHexKeySize (VOID);
	virtual VOID GetHexKey (LPSTR lpszKey);

protected:
	VOID Update (LPWPKEY lpKey, const BYTE* pcbBits, UINT cBits);
	VOID Transform (LPWPKEY lpKey);
	VOID CompleteKey (LPWPKEY lpKey, LPBYTE lpDigest);
};

#endif