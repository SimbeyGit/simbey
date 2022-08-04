#include <windows.h>
#include "Md4.h"

CMd4::CMd4 ()
{
	Reset();
}

CMd4::~CMd4 ()
{
	SecureZeroMemory(&m_ctxKey,sizeof(MD4_CTX));
}

VOID CMd4::Reset (VOID)
{
	SecureZeroMemory(&m_ctxKey,sizeof(MD4_CTX));
	m_ctxKey.state[0] = 0x67452301;
	m_ctxKey.state[1] = 0xefcdab89;
	m_ctxKey.state[2] = 0x98badcfe;
	m_ctxKey.state[3] = 0x10325476;
}

VOID CMd4::AddData (const BYTE* pcbData, UINT cData)
{
	Update(&m_ctxKey,pcbData,cData);
}

UINT CMd4::GetDigestSize (VOID)
{
	return 16;
}

VOID CMd4::GetDigest (LPBYTE lpDigest)
{
	MD4_CTX Key;
	CopyMemory(&Key,&m_ctxKey,sizeof(MD4_CTX));
	CompleteKey(&Key,lpDigest);
}

UINT CMd4::GetHexKeySize (VOID)
{
	return 32;
}

VOID CMd4::GetHexKey (LPSTR lpszKey)
{
	BYTE Digest[16];
	GetDigest(Digest);
	KeyToHex(Digest,16,lpszKey);
}


// Constants for MD4Transform routine.
#define S11 3
#define S12 7
#define S13 11
#define S14 19
#define S21 3
#define S22 5
#define S23 9
#define S24 13
#define S31 3
#define S32 9
#define S33 11
#define S34 15

static BYTE PADDING[64] =
{
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// F, G and H are basic MD4 functions.
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))

// ROTATE_LEFT rotates x left n bits.
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

// FF, GG and HH are transformations for rounds 1, 2 and 3
// Rotation is separate from addition to prevent recomputation
#define FF(a, b, c, d, x, s) { \
    (a) += F ((b), (c), (d)) + (x); \
    (a) = ROTATE_LEFT ((a), (s)); \
}

#define GG(a, b, c, d, x, s) { \
    (a) += G ((b), (c), (d)) + (x) + (ULONG)0x5a827999; \
    (a) = ROTATE_LEFT ((a), (s)); \
}

#define HH(a, b, c, d, x, s) { \
    (a) += H ((b), (c), (d)) + (x) + (ULONG)0x6ed9eba1; \
    (a) = ROTATE_LEFT ((a), (s)); \
}

VOID CMd4::Update (LPMD4_CTX lpKey, const BYTE* pcbData, UINT cData)
{
	ULONG i, index, partLen;

	// Compute number of bytes mod 64
	index = (ULONG)((lpKey->count[0] >> 3) & 0x3F);

	// Update number of bits
	if((lpKey->count[0] += ((ULONG)cData << 3)) < ((ULONG)cData << 3))
		lpKey->count[1]++;

	lpKey->count[1] += ((ULONG)cData >> 29);

	partLen = 64 - index;

	// Transform as many times as possible.
	if(cData >= partLen)
	{
		CopyMemory(&lpKey->buffer[index],pcbData,partLen);
		Transform(lpKey->state, lpKey->buffer);

		for(i = partLen; i + 63 < cData; i += 64)
			Transform(lpKey->state,&pcbData[i]);

		index = 0;
	}
	else
		i = 0;

	// Buffer remaining input
	CopyMemory(&lpKey->buffer[index],&pcbData[i],cData - i);
}

VOID CMd4::Transform (ULONG state[4], const BYTE block[64])
{
	ULONG a = state[0], b = state[1], c = state[2], d = state[3], x[16];

	Decode(x, block, 64);

	// Round 1
	FF (a, b, c, d, x[ 0], S11); /* 1 */
	FF (d, a, b, c, x[ 1], S12); /* 2 */
	FF (c, d, a, b, x[ 2], S13); /* 3 */
	FF (b, c, d, a, x[ 3], S14); /* 4 */
	FF (a, b, c, d, x[ 4], S11); /* 5 */
	FF (d, a, b, c, x[ 5], S12); /* 6 */
	FF (c, d, a, b, x[ 6], S13); /* 7 */
	FF (b, c, d, a, x[ 7], S14); /* 8 */
	FF (a, b, c, d, x[ 8], S11); /* 9 */
	FF (d, a, b, c, x[ 9], S12); /* 10 */
	FF (c, d, a, b, x[10], S13); /* 11 */
	FF (b, c, d, a, x[11], S14); /* 12 */
	FF (a, b, c, d, x[12], S11); /* 13 */
	FF (d, a, b, c, x[13], S12); /* 14 */
	FF (c, d, a, b, x[14], S13); /* 15 */
	FF (b, c, d, a, x[15], S14); /* 16 */

	// Round 2
	GG (a, b, c, d, x[ 0], S21); /* 17 */
	GG (d, a, b, c, x[ 4], S22); /* 18 */
	GG (c, d, a, b, x[ 8], S23); /* 19 */
	GG (b, c, d, a, x[12], S24); /* 20 */
	GG (a, b, c, d, x[ 1], S21); /* 21 */
	GG (d, a, b, c, x[ 5], S22); /* 22 */
	GG (c, d, a, b, x[ 9], S23); /* 23 */
	GG (b, c, d, a, x[13], S24); /* 24 */
	GG (a, b, c, d, x[ 2], S21); /* 25 */
	GG (d, a, b, c, x[ 6], S22); /* 26 */
	GG (c, d, a, b, x[10], S23); /* 27 */
	GG (b, c, d, a, x[14], S24); /* 28 */
	GG (a, b, c, d, x[ 3], S21); /* 29 */
	GG (d, a, b, c, x[ 7], S22); /* 30 */
	GG (c, d, a, b, x[11], S23); /* 31 */
	GG (b, c, d, a, x[15], S24); /* 32 */

	// Round 3
	HH (a, b, c, d, x[ 0], S31); /* 33 */
	HH (d, a, b, c, x[ 8], S32); /* 34 */
	HH (c, d, a, b, x[ 4], S33); /* 35 */
	HH (b, c, d, a, x[12], S34); /* 36 */
	HH (a, b, c, d, x[ 2], S31); /* 37 */
	HH (d, a, b, c, x[10], S32); /* 38 */
	HH (c, d, a, b, x[ 6], S33); /* 39 */
	HH (b, c, d, a, x[14], S34); /* 40 */
	HH (a, b, c, d, x[ 1], S31); /* 41 */
	HH (d, a, b, c, x[ 9], S32); /* 42 */
	HH (c, d, a, b, x[ 5], S33); /* 43 */
	HH (b, c, d, a, x[13], S34); /* 44 */
	HH (a, b, c, d, x[ 3], S31); /* 45 */
	HH (d, a, b, c, x[11], S32); /* 46 */
	HH (c, d, a, b, x[ 7], S33); /* 47 */
	HH (b, c, d, a, x[15], S34); /* 48 */

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;

	// Zeroize sensitive information.
	SecureZeroMemory(x, sizeof(x));
}

VOID CMd4::Encode (LPBYTE output, ULONG* input, UINT len)
{
	ULONG i, j;

	for(i = 0, j = 0; j < len; i++, j += 4)
	{
		output[j] = (BYTE)(input[i] & 0xff);
		output[j+1] = (BYTE)((input[i] >> 8) & 0xff);
		output[j+2] = (BYTE)((input[i] >> 16) & 0xff);
		output[j+3] = (BYTE)((input[i] >> 24) & 0xff);
	}
}

VOID CMd4::Decode (ULONG* output, const BYTE* input, UINT len)
{
	ULONG i, j;

	for(i = 0, j = 0; j < len; i++, j += 4)
		output[i] = ((ULONG)input[j]) | (((ULONG)input[j+1]) << 8) | (((ULONG)input[j+2]) << 16) | (((ULONG)input[j+3]) << 24);
}

VOID CMd4::CompleteKey (LPMD4_CTX lpKey, BYTE Digest[16])
{
	BYTE bits[8];
	ULONG index, padLen;

	// Save number of bits
	Encode(bits,lpKey->count,8);

	// Pad out to 56 mod 64.
	index = (ULONG)((lpKey->count[0] >> 3) & 0x3f);
	padLen = (index < 56) ? (56 - index) : (120 - index);
	Update(lpKey,PADDING,padLen);

	// Append length (before padding)
	Update(lpKey,bits,8);

	// Store state in digest
	Encode(Digest,lpKey->state,16);

	SecureZeroMemory(lpKey, sizeof(MD4_CTX));
}