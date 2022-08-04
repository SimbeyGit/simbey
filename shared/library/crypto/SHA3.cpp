// https://raw.githubusercontent.com/brainhub/SHA3IUF/master/sha3.c
#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "SHA3.h"

#define	SHA3_ASSERT(x)

/* 
 * This flag is used to configure "pure" Keccak, as opposed to NIST SHA3.
 */
#define	SHA3_USE_KECCAK_FLAG	0x80000000
#define	SHA3_CW(x)				((x) & (~SHA3_USE_KECCAK_FLAG))

#if defined(_MSC_VER)
	#define	SHA3_CONST(x)		x
#else
	#define	SHA3_CONST(x)		x##L
#endif

#define	SHA3_ROTL64(x, y)		(((x) << (y)) | ((x) >> ((sizeof(ULONGLONG) * 8) - (y))))

static const ULONGLONG keccakf_rndc[24] =
{
	SHA3_CONST(0x0000000000000001UL), SHA3_CONST(0x0000000000008082UL),
	SHA3_CONST(0x800000000000808aUL), SHA3_CONST(0x8000000080008000UL),
	SHA3_CONST(0x000000000000808bUL), SHA3_CONST(0x0000000080000001UL),
	SHA3_CONST(0x8000000080008081UL), SHA3_CONST(0x8000000000008009UL),
	SHA3_CONST(0x000000000000008aUL), SHA3_CONST(0x0000000000000088UL),
	SHA3_CONST(0x0000000080008009UL), SHA3_CONST(0x000000008000000aUL),
	SHA3_CONST(0x000000008000808bUL), SHA3_CONST(0x800000000000008bUL),
	SHA3_CONST(0x8000000000008089UL), SHA3_CONST(0x8000000000008003UL),
	SHA3_CONST(0x8000000000008002UL), SHA3_CONST(0x8000000000000080UL),
	SHA3_CONST(0x000000000000800aUL), SHA3_CONST(0x800000008000000aUL),
	SHA3_CONST(0x8000000080008081UL), SHA3_CONST(0x8000000000008080UL),
	SHA3_CONST(0x0000000080000001UL), SHA3_CONST(0x8000000080008008UL)
};

static const unsigned keccakf_rotc[24] =
{
	1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62,
	18, 39, 61, 20, 44
};

static const unsigned keccakf_piln[24] =
{
	10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20,
	14, 22, 9, 6, 1
};

/* generally called after SHA3_KECCAK_SPONGE_WORDS-m_ctxKey.capacityWords words 
 * are XORed into the state s 
 */
static void keccakf (ULONGLONG s[25])
{
    int i, j, round;
    ULONGLONG t, bc[5];
#define KECCAK_ROUNDS 24

    for(round = 0; round < KECCAK_ROUNDS; round++)
	{
        /* Theta */
        for(i = 0; i < 5; i++)
            bc[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^ s[i + 20];

        for(i = 0; i < 5; i++)
		{
            t = bc[(i + 4) % 5] ^ SHA3_ROTL64(bc[(i + 1) % 5], 1);
            for(j = 0; j < 25; j += 5)
                s[j + i] ^= t;
        }

        /* Rho Pi */
        t = s[1];
        for(i = 0; i < 24; i++)
		{
            j = keccakf_piln[i];
            bc[0] = s[j];
            s[j] = SHA3_ROTL64(t, keccakf_rotc[i]);
            t = bc[0];
        }

        /* Chi */
        for(j = 0; j < 25; j += 5)
		{
            for(i = 0; i < 5; i++)
                bc[i] = s[j + i];
            for(i = 0; i < 5; i++)
                s[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }

        /* Iota */
        s[0] ^= keccakf_rndc[round];
    }
}

CSHA3::CSHA3 (UINT nBitSize, bool fUseKeccak) :
	m_nBitSize(nBitSize)
{
	m_ctxKey.capacityWords = fUseKeccak ? SHA3_USE_KECCAK_FLAG : 0;
	Reset();
}

CSHA3::~CSHA3 ()
{
	SecureZeroMemory(&m_ctxKey, sizeof(m_ctxKey));
}

VOID CSHA3::Reset (VOID)
{
	bool fUseKeccak = SHA3_USE_KECCAK_FLAG == (m_ctxKey.capacityWords & SHA3_USE_KECCAK_FLAG);

	SecureZeroMemory(&m_ctxKey, sizeof(m_ctxKey));
	m_ctxKey.capacityWords = 2 * m_nBitSize / (8 * sizeof(ULONGLONG));
	if(fUseKeccak)
		m_ctxKey.capacityWords |= SHA3_USE_KECCAK_FLAG;
}

VOID CSHA3::AddData (const BYTE* pcbData, UINT cData)
{
    /* 0...7 -- how much is needed to have a word */
    unsigned old_tail = (8 - m_ctxKey.byteIndex) & 7;

    size_t words;
    unsigned tail;
    size_t i;

    /* SHA3_TRACE_BUF("called to update with:", pcbData, len); */

    SHA3_ASSERT(m_ctxKey.byteIndex < 8);
    SHA3_ASSERT(m_ctxKey.wordIndex < sizeof(m_ctxKey.s) / sizeof(m_ctxKey.s[0]));

    if(cData < old_tail)	/* have no complete word or haven't started the word yet */
	{
        /* SHA3_TRACE("because %d<%d, store it and return", (unsigned)len, (unsigned)old_tail); */
        /* endian-independent code follows: */
        while(cData--)
            m_ctxKey.saved |= (ULONGLONG) (*(pcbData++)) << ((m_ctxKey.byteIndex++) * 8);
        SHA3_ASSERT(m_ctxKey.byteIndex < 8);
        return;
    }

    if(old_tail)			/* will have one word to process */
	{
        /* SHA3_TRACE("completing one word with %d bytes", (unsigned)old_tail); */
        /* endian-independent code follows: */
        cData -= old_tail;
        while(old_tail--)
            m_ctxKey.saved |= (ULONGLONG) (*(pcbData++)) << ((m_ctxKey.byteIndex++) * 8);

        /* now ready to add saved to the sponge */
        m_ctxKey.s[m_ctxKey.wordIndex] ^= m_ctxKey.saved;
        SHA3_ASSERT(m_ctxKey.byteIndex == 8);
        m_ctxKey.byteIndex = 0;
        m_ctxKey.saved = 0;
        if(++m_ctxKey.wordIndex ==
			(SHA3_KECCAK_SPONGE_WORDS - SHA3_CW(m_ctxKey.capacityWords)))
		{
            keccakf(m_ctxKey.s);
            m_ctxKey.wordIndex = 0;
        }
    }

	/* now work in full words directly from input */

	SHA3_ASSERT(m_ctxKey.byteIndex == 0);

	words = cData / sizeof(ULONGLONG);
	tail = cData - words * sizeof(ULONGLONG);

	/* SHA3_TRACE("have %d full words to process", (unsigned)words); */

	for(i = 0; i < words; i++, pcbData += sizeof(ULONGLONG))
	{
		const ULONGLONG t = (ULONGLONG) (pcbData[0]) |
			((ULONGLONG) (pcbData[1]) << 8 * 1) |
			((ULONGLONG) (pcbData[2]) << 8 * 2) |
			((ULONGLONG) (pcbData[3]) << 8 * 3) |
			((ULONGLONG) (pcbData[4]) << 8 * 4) |
			((ULONGLONG) (pcbData[5]) << 8 * 5) |
			((ULONGLONG) (pcbData[6]) << 8 * 6) |
			((ULONGLONG) (pcbData[7]) << 8 * 7);
#if defined(__x86_64__ ) || defined(__i386__)
		SHA3_ASSERT(memcmp(&t, pcbData, 8) == 0);
#endif
		m_ctxKey.s[m_ctxKey.wordIndex] ^= t;
		if(++m_ctxKey.wordIndex == (SHA3_KECCAK_SPONGE_WORDS - SHA3_CW(m_ctxKey.capacityWords)))
		{
			keccakf(m_ctxKey.s);
			m_ctxKey.wordIndex = 0;
		}
	}

	/* SHA3_TRACE("have %d bytes left to process, save them", (unsigned)tail); */

	/* finally, save the partial word */
	SHA3_ASSERT(m_ctxKey.byteIndex == 0 && tail < 8);
	while(tail--)
	{
		/* SHA3_TRACE("Store byte %02x '%c'", *pcbData, *pcbData); */
		m_ctxKey.saved |= (ULONGLONG) (*(pcbData++)) << ((m_ctxKey.byteIndex++) * 8);
	}
	SHA3_ASSERT(m_ctxKey.byteIndex < 8);
	/* SHA3_TRACE("Have saved=0x%016" PRIx64 " at the end", m_ctxKey.saved); */
}

UINT CSHA3::GetDigestSize (VOID)
{
	switch(m_nBitSize)
	{
	case 224:
		return 28;
	case 256:
		return 32;
	case 384:
		return 48;
	case 512:
		return 64;
	}
	return 0;
}

VOID CSHA3::GetDigest (LPBYTE lpDigest)
{
	SHA3_CONTEXT ctxKey;
	CopyMemory(&ctxKey, &m_ctxKey, sizeof(m_ctxKey));
	Finalize(&ctxKey);
	CopyMemory(lpDigest, ctxKey.sb, GetDigestSize());
}

UINT CSHA3::GetHexKeySize (VOID)
{
	return GetDigestSize() * 2;
}

VOID CSHA3::GetHexKey (LPSTR lpszKey)
{
	BYTE Digest[64];
	GetDigest(Digest);
	KeyToHex(Digest,GetDigestSize(),lpszKey);
}

VOID CSHA3::Finalize (SHA3_CONTEXT* pctx)
{
	/* SHA3_TRACE("called with %d bytes in the buffer", ctxKey.byteIndex); */

	/* Append 2-bit suffix 01, per SHA-3 spec. Instead of 1 for padding we
	 * use 1<<2 below. The 0x02 below corresponds to the suffix 01.
	 * Overall, we feed 0, then 1, and finally 1 to start padding. Without
	 * M || 01, we would simply use 1 to start padding. */

	ULONGLONG t;

	if(pctx->capacityWords & SHA3_USE_KECCAK_FLAG)
	{
		/* Keccak version */
		t = (ULONGLONG)(((ULONGLONG) 1) << (pctx->byteIndex * 8));
	}
	else
	{
		/* SHA3 version */
		t = (ULONGLONG)(((ULONGLONG)(0x02 | (1 << 2))) << ((pctx->byteIndex) * 8));
	}

	pctx->s[pctx->wordIndex] ^= pctx->saved ^ t;

	pctx->s[SHA3_KECCAK_SPONGE_WORDS - SHA3_CW(pctx->capacityWords) - 1] ^=
		SHA3_CONST(0x8000000000000000UL);
	keccakf(pctx->s);

	/* Return first bytes of the pctx->s. This conversion is not needed for
	 * little-endian platforms e.g. wrap with #if !defined(__BYTE_ORDER__)
	 * || !defined(__ORDER_LITTLE_ENDIAN__) || __BYTE_ORDER__!=__ORDER_LITTLE_ENDIAN__ 
	 *    ... the conversion below ...
	 * #endif */
	{
		unsigned i;
		for(i = 0; i < SHA3_KECCAK_SPONGE_WORDS; i++)
		{
			const unsigned t1 = (ULONG) pctx->s[i];
			const unsigned t2 = (ULONG) ((pctx->s[i] >> 16) >> 16);
			pctx->sb[i * 8 + 0] = (BYTE) (t1);
			pctx->sb[i * 8 + 1] = (BYTE) (t1 >> 8);
			pctx->sb[i * 8 + 2] = (BYTE) (t1 >> 16);
			pctx->sb[i * 8 + 3] = (BYTE) (t1 >> 24);
			pctx->sb[i * 8 + 4] = (BYTE) (t2);
			pctx->sb[i * 8 + 5] = (BYTE) (t2 >> 8);
			pctx->sb[i * 8 + 6] = (BYTE) (t2 >> 16);
			pctx->sb[i * 8 + 7] = (BYTE) (t2 >> 24);
		}
	}

	/* SHA3_TRACE_BUF("Hash: (first 32 bytes)", pctx->sb, 256 / 8); */
}
