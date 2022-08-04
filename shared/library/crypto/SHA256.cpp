// https://www.oryx-embedded.com/doc/sha256_8c_source.html
#include <windows.h>
#include "..\Core\Endian.h"
#include "SHA256.h"

// Rotate left operation
#define	ROL8(a, n) (((a) << (n)) | ((a) >> (8 - (n))))
#define	ROL16(a, n) (((a) << (n)) | ((a) >> (16 - (n))))
#define	ROL32(a, n) (((a) << (n)) | ((a) >> (32 - (n))))
#define	ROL64(a, n) (((a) << (n)) | ((a) >> (64 - (n))))

// Rotate right operation
#define	ROR8(a, n) (((a) >> (n)) | ((a) << (8 - (n))))
#define	ROR16(a, n) (((a) >> (n)) | ((a) << (16 - (n))))
#define	ROR32(a, n) (((a) >> (n)) | ((a) << (32 - (n))))
#define	ROR64(a, n) (((a) >> (n)) | ((a) << (64 - (n))))

// Shift left operation
#define	SHL8(a, n) ((a) << (n))
#define	SHL16(a, n) ((a) << (n))
#define	SHL32(a, n) ((a) << (n))
#define	SHL64(a, n) ((a) << (n))

// Shift right operation
#define	SHR8(a, n) ((a) >> (n))
#define	SHR16(a, n) ((a) >> (n))
#define	SHR32(a, n) ((a) >> (n))
#define	SHR64(a, n) ((a) >> (n))

// Macro to access the workspace as a circular buffer
#define	W(t)			w[(t) & 0x0F]

// SHA-256 auxiliary functions
#define	CH(x, y, z)		(((x) & (y)) | (~(x) & (z)))
#define	MAJ(x, y, z)	(((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define	SIGMA1(x)		(ROR32(x, 2) ^ ROR32(x, 13) ^ ROR32(x, 22))
#define	SIGMA2(x)		(ROR32(x, 6) ^ ROR32(x, 11) ^ ROR32(x, 25))
#define	SIGMA3(x)		(ROR32(x, 7) ^ ROR32(x, 18) ^ SHR32(x, 3))
#define	SIGMA4(x)		(ROR32(x, 17) ^ ROR32(x, 19) ^ SHR32(x, 10))

// SHA-256 padding
static const BYTE padding[64] =
{
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// SHA-256 constants
static const UINT32 k[64] =
{
	0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
	0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
	0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
	0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
	0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
	0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
	0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
	0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
};

CSHA256::CSHA256 ()
{
	Reset();
}

CSHA256::~CSHA256 ()
{
	SecureZeroMemory(&m_ctxKey, sizeof(SHA256_CTX));
}

VOID CSHA256::Reset (VOID)
{
	// Set initial hash value
	m_ctxKey.h[0] = 0x6A09E667;
	m_ctxKey.h[1] = 0xBB67AE85;
	m_ctxKey.h[2] = 0x3C6EF372;
	m_ctxKey.h[3] = 0xA54FF53A;
	m_ctxKey.h[4] = 0x510E527F;
	m_ctxKey.h[5] = 0x9B05688C;
	m_ctxKey.h[6] = 0x1F83D9AB;
	m_ctxKey.h[7] = 0x5BE0CD19;

	// Number of bytes in the buffer
	m_ctxKey.size = 0;

	// Total length of the message
	m_ctxKey.totalSize = 0;
}

VOID CSHA256::AddData (const BYTE* pcbData, UINT cData)
{
	Update(&m_ctxKey, pcbData, cData);
}

UINT CSHA256::GetDigestSize (VOID)
{
	return sizeof(m_ctxKey.digest);
}

VOID CSHA256::GetDigest (LPBYTE lpDigest)
{
	SHA256_CTX Key;
	CopyMemory(&Key, &m_ctxKey, sizeof(m_ctxKey));
	CompleteKey(&Key);
	CopyMemory(lpDigest, Key.digest, sizeof(Key.digest));
}

UINT CSHA256::GetHexKeySize (VOID)
{
	return sizeof(m_ctxKey.digest) * 2;
}

VOID CSHA256::GetHexKey (LPSTR lpszKey)
{
	BYTE Digest[SHA256_DIGEST_SIZE];
	GetDigest(Digest);
	KeyToHex(Digest, sizeof(Digest), lpszKey);
}

VOID sha256ProcessBlock (SHA256_CTX* context)
{
	UINT32 t;
	UINT32 temp1;
	UINT32 temp2;

	//Initialize the 8 working registers
	UINT32 a = context->h[0];
	UINT32 b = context->h[1];
	UINT32 c = context->h[2];
	UINT32 d = context->h[3];
	UINT32 e = context->h[4];
	UINT32 f = context->h[5];
	UINT32 g = context->h[6];
	UINT32 h = context->h[7];

	//Process message in 16-word blocks
	UINT32* w = context->w;

	if(!IsBigEndian())
	{
		//Convert from big-endian byte order to host byte order
		for(t = 0; t < 16; t++)
			EndianSwap(w[t]);
	}

	// SHA-256 hash computation (alternate method)
	for(t = 0; t < 64; t++)
	{
		// Prepare the message schedule
		if(t >= 16)
			W(t) += SIGMA4(W(t + 14)) + W(t + 9) + SIGMA3(W(t + 1));

		// Calculate T1 and T2
		temp1 = h + SIGMA2(e) + CH(e, f, g) + k[t] + W(t);
		temp2 = SIGMA1(a) + MAJ(a, b, c);

		// Update the working registers
		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}

	//Update the hash value
	context->h[0] += a;
	context->h[1] += b;
	context->h[2] += c;
	context->h[3] += d;
	context->h[4] += e;
	context->h[5] += f;
	context->h[6] += g;
	context->h[7] += h;
}

VOID CSHA256::Update (SHA256_CTX* pKey, const BYTE* pcbData, UINT cData)
{
	UINT32 n;

	//Process the incoming data
	while(0 < cData)
	{
		//The buffer can hold at most 64 bytes
		n = min(cData, 64 - pKey->size);

		//Copy the data to the buffer
		CopyMemory(pKey->buffer + pKey->size, pcbData, n);

		//Update the SHA-256 context
		pKey->size += n;
		pKey->totalSize += n;
		//Advance the data pointer
		pcbData = pcbData + n;
		//Remaining bytes to process
		cData -= n;

		//Process message in 16-word blocks
		if(64 == pKey->size)
		{
			//Transform the 16-word block
			sha256ProcessBlock(pKey);

			//Empty the buffer
			pKey->size = 0;
		}
	}
}

VOID CSHA256::CompleteKey (SHA256_CTX* pKey)
{
	UINT32 i;
	UINT32 paddingSize;
	UINT64 totalSize;

	// Length of the original message (before padding)
	totalSize = pKey->totalSize * 8;

	// Pad the message so that its length is congruent to 56 modulo 64
	if(pKey->size < 56)
	   paddingSize = 56 - pKey->size;
	else
	   paddingSize = 64 + 56 - pKey->size;

	// Append padding
	Update(pKey, padding, paddingSize);

	// Append the length of the original message
	pKey->w[14] = static_cast<UINT32>(totalSize >> 32);
	pKey->w[15] = static_cast<UINT32>(totalSize);
	if(!IsBigEndian())
	{
		EndianSwap(pKey->w[14]);
		EndianSwap(pKey->w[15]);
	}

	// Calculate the message digest
	sha256ProcessBlock(pKey);

	if(!IsBigEndian())
	{
		//Convert from host byte order to big-endian byte order
		for(i = 0; i < 8; i++)
			EndianSwap(pKey->h[i]);
	}
}
