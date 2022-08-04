#include <windows.h>
#include "..\Core\Endian.h"
#include "SHA1.h"

CSHA1::CSHA1 ()
{
	Reset();
}

CSHA1::~CSHA1 ()
{
	SecureZeroMemory(&m_ctxKey, sizeof(SHA1_CTX));
}

VOID CSHA1::Reset (VOID)
{
	m_ctxKey.Length_Low = 0;
	m_ctxKey.Length_High = 0;
	m_ctxKey.Message_Block_Index = 0;

	m_ctxKey.Message_Digest[0] = 0x67452301;
	m_ctxKey.Message_Digest[1] = 0xEFCDAB89;
	m_ctxKey.Message_Digest[2] = 0x98BADCFE;
	m_ctxKey.Message_Digest[3] = 0x10325476;
	m_ctxKey.Message_Digest[4] = 0xC3D2E1F0;

	m_ctxKey.Corrupted = 0;
}

VOID CSHA1::AddData (const BYTE* pcbData, UINT cData)
{
	Update(&m_ctxKey, pcbData, cData);
}

UINT CSHA1::GetDigestSize (VOID)
{
	return sizeof(m_ctxKey.Message_Digest);
}

VOID CSHA1::GetDigest (LPBYTE lpDigest)
{
	SHA1_CTX Key;
	CopyMemory(&Key, &m_ctxKey, sizeof(m_ctxKey));
	CompleteKey(&Key);

	if(!IsBigEndian())
	{
		for(INT i = 0; i < ARRAYSIZE(Key.Message_Digest); i++)
			EndianSwap(Key.Message_Digest[i]);
	}

	CopyMemory(lpDigest, Key.Message_Digest, sizeof(Key.Message_Digest));
}

UINT CSHA1::GetHexKeySize (VOID)
{
	return sizeof(m_ctxKey.Message_Digest) * 2;
}

VOID CSHA1::GetHexKey (LPSTR lpszKey)
{
	BYTE Digest[20];
	GetDigest(Digest);
	KeyToHex(Digest,sizeof(Digest),lpszKey);
}

/*
 *  Define the circular shift macro
 */
#define SHA1CircularShift(bits,word) \
				((((word) << (bits)) & 0xFFFFFFFF) | \
				((word) >> (32-(bits))))

VOID SHA1ProcessMessageBlock (SHA1_CTX* pKey)
{
	// Constants defined in SHA-1
	const unsigned K[] =
	{
		0x5A827999,
		0x6ED9EBA1,
		0x8F1BBCDC,
		0xCA62C1D6
	};
	int t;					// Loop counter
	unsigned temp;			// Temporary word value
	unsigned W[80];			// Word sequence
	unsigned A, B, C, D, E;	// Word buffers

	/*
	 *  Initialize the first 16 words in the array W
	 */
	for(t = 0; t < 16; t++)
	{
		W[t] = ((unsigned) pKey->Message_Block[t * 4]) << 24;
		W[t] |= ((unsigned) pKey->Message_Block[t * 4 + 1]) << 16;
		W[t] |= ((unsigned) pKey->Message_Block[t * 4 + 2]) << 8;
		W[t] |= ((unsigned) pKey->Message_Block[t * 4 + 3]);
	}

	for(t = 16; t < 80; t++)
	{
		W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
	}

	A = pKey->Message_Digest[0];
	B = pKey->Message_Digest[1];
	C = pKey->Message_Digest[2];
	D = pKey->Message_Digest[3];
	E = pKey->Message_Digest[4];

	for(t = 0; t < 20; t++)
	{
		temp =  SHA1CircularShift(5,A) +
				((B & C) | ((~B) & D)) + E + W[t] + K[0];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 20; t < 40; t++)
	{
		temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 40; t < 60; t++)
	{
		temp = SHA1CircularShift(5,A) +
			   ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 60; t < 80; t++)
	{
		temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
		temp &= 0xFFFFFFFF;
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	pKey->Message_Digest[0] =
						(pKey->Message_Digest[0] + A) & 0xFFFFFFFF;
	pKey->Message_Digest[1] =
						(pKey->Message_Digest[1] + B) & 0xFFFFFFFF;
	pKey->Message_Digest[2] =
						(pKey->Message_Digest[2] + C) & 0xFFFFFFFF;
	pKey->Message_Digest[3] =
						(pKey->Message_Digest[3] + D) & 0xFFFFFFFF;
	pKey->Message_Digest[4] =
						(pKey->Message_Digest[4] + E) & 0xFFFFFFFF;

	pKey->Message_Block_Index = 0;
}

VOID CSHA1::Update (SHA1_CTX* pKey, const BYTE* pcbData, UINT cData)
{
	while(cData-- && !pKey->Corrupted)
	{
		pKey->Message_Block[pKey->Message_Block_Index++] = (*pcbData & 0xFF);

		pKey->Length_Low += 8;

		/* Force it to 32 bits */
		pKey->Length_Low &= 0xFFFFFFFF;

		if(pKey->Length_Low == 0)
		{
			pKey->Length_High++;

			/* Force it to 32 bits */
			pKey->Length_High &= 0xFFFFFFFF;

			if(pKey->Length_High == 0)
			{
				/* Message is too long */
				pKey->Corrupted = 1;
			}
		}

		if(pKey->Message_Block_Index == 64)
		{
			SHA1ProcessMessageBlock(pKey);
		}

		pcbData++;
	}
}

VOID CSHA1::CompleteKey (SHA1_CTX* pKey)
{
/*
 *	  According to the standard, the message must be padded to an even
 *	  512 bits.  The first padding bit must be a '1'.  The last 64
 *	  bits represent the length of the original message.  All bits in
 *	  between should be 0.  This function will pad the message
 *	  according to those rules by filling the Message_Block array
 *	  accordingly.  It will also call SHA1ProcessMessageBlock()
 *	  appropriately.  When it returns, it can be assumed that the
 *	  message digest has been computed.
 */

	/*
	 *  Check to see if the current message block is too small to hold
	 *  the initial padding bits and length.  If so, we will pad the
	 *  block, process it, and then continue padding into a second
	 *  block.
	 */
	if(pKey->Message_Block_Index > 55)
	{
		pKey->Message_Block[pKey->Message_Block_Index++] = 0x80;
		while(pKey->Message_Block_Index < 64)
		{
			pKey->Message_Block[pKey->Message_Block_Index++] = 0;
		}

		SHA1ProcessMessageBlock(pKey);

		while(pKey->Message_Block_Index < 56)
		{
			pKey->Message_Block[pKey->Message_Block_Index++] = 0;
		}
	}
	else
	{
		pKey->Message_Block[pKey->Message_Block_Index++] = 0x80;
		while(pKey->Message_Block_Index < 56)
		{
			pKey->Message_Block[pKey->Message_Block_Index++] = 0;
		}
	}

	/*
	 *  Store the message length as the last 8 octets
	 */
	pKey->Message_Block[56] = (pKey->Length_High >> 24) & 0xFF;
	pKey->Message_Block[57] = (pKey->Length_High >> 16) & 0xFF;
	pKey->Message_Block[58] = (pKey->Length_High >> 8) & 0xFF;
	pKey->Message_Block[59] = (pKey->Length_High) & 0xFF;
	pKey->Message_Block[60] = (pKey->Length_Low >> 24) & 0xFF;
	pKey->Message_Block[61] = (pKey->Length_Low >> 16) & 0xFF;
	pKey->Message_Block[62] = (pKey->Length_Low >> 8) & 0xFF;
	pKey->Message_Block[63] = (pKey->Length_Low) & 0xFF;

	SHA1ProcessMessageBlock(pKey);
}
