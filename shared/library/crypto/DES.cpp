#include <windows.h>
#include "..\Core\CoreDefs.h"
#include "Des.h"

#if defined(_M_I86) || defined(WIN32)
	#define LITTLE_ENDIAN
#endif

static BYTE odd_parity[256] =
{
	1,  1,  2,  2,  4,  4,  7,  7,  8,  8, 11, 11, 13, 13, 14, 14,
	16, 16, 19, 19, 21, 21, 22, 22, 25, 25, 26, 26, 28, 28, 31, 31,
	32, 32, 35, 35, 37, 37, 38, 38, 41, 41, 42, 42, 44, 44, 47, 47,
	49, 49, 50, 50, 52, 52, 55, 55, 56, 56, 59, 59, 61, 61, 62, 62,
	64, 64, 67, 67, 69, 69, 70, 70, 73, 73, 74, 74, 76, 76, 79, 79,
	81, 81, 82, 82, 84, 84, 87, 87, 88, 88, 91, 91, 93, 93, 94, 94,
	97, 97, 98, 98,100,100,103,103,104,104,107,107,109,109,110,110,
	112,112,115,115,117,117,118,118,121,121,122,122,124,124,127,127,
	128,128,131,131,133,133,134,134,137,137,138,138,140,140,143,143,
	145,145,146,146,148,148,151,151,152,152,155,155,157,157,158,158,
	161,161,162,162,164,164,167,167,168,168,171,171,173,173,174,174,
	176,176,179,179,181,181,182,182,185,185,186,186,188,188,191,191,
	193,193,194,194,196,196,199,199,200,200,203,203,205,205,206,206,
	208,208,211,211,213,213,214,214,217,217,218,218,220,220,223,223,
	224,224,227,227,229,229,230,230,233,233,234,234,236,236,239,239,
	241,241,242,242,244,244,247,247,248,248,251,251,253,253,254,254
};

// initial permutation IP
static CHAR ip[64] =
{
	58, 50, 42, 34, 26, 18, 10,  2,
	60, 52, 44, 36, 28, 20, 12,  4,
	62, 54, 46, 38, 30, 22, 14,  6,
	64, 56, 48, 40, 32, 24, 16,  8,
	57, 49, 41, 33, 25, 17,  9,  1,
	59, 51, 43, 35, 27, 19, 11,  3,
	61, 53, 45, 37, 29, 21, 13,  5,
	63, 55, 47, 39, 31, 23, 15,  7
};

// final permutation IP^-1
static CHAR fp[64] =
{
	40,  8, 48, 16, 56, 24, 64, 32,
	39,  7, 47, 15, 55, 23, 63, 31,
	38,  6, 46, 14, 54, 22, 62, 30,
	37,  5, 45, 13, 53, 21, 61, 29,
	36,  4, 44, 12, 52, 20, 60, 28,
	35,  3, 43, 11, 51, 19, 59, 27,
	34,  2, 42, 10, 50, 18, 58, 26,
	33,  1, 41,  9, 49, 17, 57, 25
};

// permuted choice table (key)
static CHAR pc1[] =
{
	57, 49, 41, 33, 25, 17,  9,
	 1, 58, 50, 42, 34, 26, 18,
	10,  2, 59, 51, 43, 35, 27,
	19, 11,  3, 60, 52, 44, 36,

	63, 55, 47, 39, 31, 23, 15,
	 7, 62, 54, 46, 38, 30, 22,
	14,  6, 61, 53, 45, 37, 29,
	21, 13,  5, 28, 20, 12,  4
};

// number left rotations of pc1
static CHAR totrot[] = {1,2,4,6,8,10,12,14,15,17,19,21,23,25,27,28};

// permuted choice key (table)
static CHAR pc2[] =
{
	14, 17, 11, 24,  1,  5,
	 3, 28, 15,  6, 21, 10,
	23, 19, 12,  4, 26,  8,
	16,  7, 27, 20, 13,  2,
	41, 52, 31, 37, 47, 55,
	30, 40, 51, 45, 33, 48,
	44, 49, 39, 56, 34, 53,
	46, 42, 50, 36, 29, 32
};

// The (in)famous S-boxes
static CHAR si[8][64] =
{
	// S1
	14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7,
	 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8,
	 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0,
	15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13,

	// S2
	15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10,
	 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5,
	 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15,
	13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9,

	// S3
	10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8,
	13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1,
	13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7,
	 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12,

	// S4
	 7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15,
	13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9,
	10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4,
	 3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14,

	// S5
	 2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9,
	14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6,
	 4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14,
	11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3,

	// S6
	12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11,
	10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8,
	 9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6,
	 4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13,

	// S7
	 4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1,
	13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6,
	 1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2,
	 6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12,

	// S8
	13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7,
	 1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2,
	 7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8,
	 2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11
};

// 32-bit permutation function P used on the output of the S-boxes
static char p32i[32] =
{
	16,  7, 20, 21,
	29, 12, 28, 17,
	 1, 15, 23, 26,
	 5, 18, 31, 10,
	 2,  8, 24, 14,
	32, 27,  3,  9,
	19, 13, 30,  6,
	22, 11,  4, 25
};

// bit 0 is left-most in byte
static INT bytebit[] = {0200,0100,040,020,010,04,02,01};

static INT nibblebit[] = {010,04,02,01};

//

CDes::CDes (INT iMode, BOOL bPermutations)
{
	m_iDesMode = iMode;
	m_bPermutations = bPermutations;

	m_sp = (LONG(*)[64])__new BYTE[sizeof(LONG) * 8 * 64];

	InitializeSandP();

	m_kn = (BYTE(*)[8])__new BYTE[sizeof(char) * 8 * 16];

	if(m_iDesMode == 0 && m_bPermutations)
	{
		m_iperm = (CHAR(*)[16][8])__new BYTE[sizeof(CHAR) * 16 * 16 * 8];

		PermInit(m_iperm,ip);

		m_fperm = (CHAR(*)[16][8])__new BYTE[sizeof(CHAR) * 16 * 16 * 8];

		PermInit(m_fperm,fp);
	}
	else
	{
		m_iperm = NULL;
		m_fperm = NULL;
	}
}

CDes::~CDes ()
{
	__delete_array m_sp;
	__delete_array m_kn;

	if(m_bPermutations)
	{
		__delete_array m_iperm;
		__delete_array m_fperm;

		m_iperm = NULL;
		m_fperm = NULL;
	}

	m_sp = NULL;
	m_kn = NULL;
}

VOID CDes::EncodeUsingKey (LPBYTE lpKey, const BYTE* pcbInput, LPBYTE lpOutput)
{
	BYTE Temp[8];

	CopyMemory(Temp,pcbInput,8);

	SetKey((LPSTR)lpKey);
	Encode((LPSTR)Temp);

	CopyMemory(lpOutput,Temp,8);
}

VOID CDes::SetKey (CHAR* key)
{
	// Set key (initialize key schedule array)
	register INT i, j, l;
	CHAR pc1m[56];			// place to modify pc1 into
	CHAR pcr[56];			// place to rotate pc1 into
	INT m;

	// Notes from:  Phil Karn, 12 Dec 1986
	// In mode 2, the 128 bytes of subkey are set directly from the
	// user's key, allowing him to use completely independent
	// subkeys for each round. Note that the user MUST specify a
	// full 128 bytes.
	//
	// I would like to think that this technique gives the NSA a real
	// headache, but I'm not THAT naive.
	if(m_iDesMode == 2)
	{
		for(i = 0; i < 16; i++)
		{
			for(j = 0; j < 8; j++)
				m_kn[i][j] = *key++;
		}
		return;
	}
	// Clear key schedule
	for(i = 0; i < 16; i++)
	{
		for(j = 0; j < 8; j++)
			m_kn[i][j] = 0;
	}

	for(j = 0; j < 56; j++)			// convert pc1 to bits of key
	{
		l = pc1[j] - 1;				// integer bit location
		m = l & 07;					// find bit
		// Find which key byte l is in
		// and which bit of that byte
		// and store 1-bit result
		pc1m[j] = (key[l >> 3] & bytebit[m]) ? 1 : 0;
	}
	for(i = 0; i < 16; i++)			// key chunk for each iteration
	{
		for(j = 0; j < 56; j++)		// rotate pc1 the right amount
			pcr[j] = pc1m[(l = j + totrot[i]) < (j < 28 ? 28 : 56) ? l : l - 28];

		// rotate left and right halves independently
		for(j = 0; j < 48; j++)		// select bits individually
		{
			// check bit that goes to kn[j]
			if(pcr[pc2[j] - 1])
			{
				// mask it in if it's there
				l = j % 6;
				m_kn[i][j / 6] |= bytebit[l] >> 2;
			}
		}
	}
}

VOID CDes::Encode (CHAR* block)
{
	// In-place encryption of 64-bit block
	register INT i;
	ULONG work[2];							// Working data storage
	LONG tmp;

	Permute(block,m_iperm,(CHAR*)work);		// Initial Permutation

#ifdef LITTLE_ENDIAN
	work[0] = ByteSwap(work[0]);
	work[1] = ByteSwap(work[1]);
#endif

	// Do the 16 rounds
	for(i = 0; i < 16; i++)
		CypherRound(i,work);

	// Left/right half swap
	tmp = work[0];
	work[0] = work[1];	
	work[1] = tmp;

#ifdef LITTLE_ENDIAN
	work[0] = ByteSwap(work[0]);
	work[1] = ByteSwap(work[1]);
#endif

	Permute((CHAR*)work,m_fperm,block);		// Inverse initial permutation
}

VOID CDes::Decode (CHAR* block)
{
	// In-place decryption of 64-bit block
	register INT i;
	ULONG work[2];							// Working data storage
	LONG tmp;

	Permute(block,m_iperm,(CHAR*)work);		// Initial permutation

#ifdef LITTLE_ENDIAN
	work[0] = ByteSwap(work[0]);
	work[1] = ByteSwap(work[1]);
#endif

	// Left/right half swap
	tmp = work[0];
	work[0] = work[1];	
	work[1] = tmp;

	// Do the 16 rounds in reverse order
	for(i = 15; i >= 0; i--)
		CypherRound(i,work);

#ifdef LITTLE_ENDIAN
	work[0] = ByteSwap(work[0]);
	work[1] = ByteSwap(work[1]);
#endif

	Permute((CHAR*)work,m_fperm,block);		// Inverse initial permutation
}

VOID CDes::CreateKey (LPBYTE lpInput7, LPBYTE lpOutput8)
{
	BYTE Key[8];
	INT i;

	// Extract 8 7-bit bytes
	Key[0] = lpInput7[0];
	Key[1] = (BYTE) (lpInput7[0] << 7 | (lpInput7[1] & 0xff) >> 1);
	Key[2] = (BYTE) (lpInput7[1] << 6 | (lpInput7[2] & 0xff) >> 2);
	Key[3] = (BYTE) (lpInput7[2] << 5 | (lpInput7[3] & 0xff) >> 3);
	Key[4] = (BYTE) (lpInput7[3] << 4 | (lpInput7[4] & 0xff) >> 4);
	Key[5] = (BYTE) (lpInput7[4] << 3 | (lpInput7[5] & 0xff) >> 5);
	Key[6] = (BYTE) (lpInput7[5] << 2 | (lpInput7[6] & 0xff) >> 6);
	Key[7] = (BYTE) (lpInput7[6] << 1);
	
	// Compute (and copy) Parity
	for(i = 0; i < 8; i++)
		lpOutput8[i] = odd_parity[Key[i]];
}

//

VOID CDes::InitializeSandP (VOID)
{
	// Initialize the lookup table for the combined S and P boxes
	CHAR pbox[32];
	INT p, i, s, j, rowcol;
	LONG val;

	// Compute pbox, the inverse of p32i.  This is easier to work with
	for(p = 0; p < 32; p++)
	{
		for(i = 0; i < 32; i++)
		{
			if(p32i[i] - 1 == p)
			{
				pbox[p] = i;
				break;
			}
		}
	}

	for(s = 0; s < 8; s++)			// For each S-box
	{
		for(i = 0; i < 64; i++)		// For each possible input
		{
			val = 0;
			// The row number is formed from the first and last
			// bits; the column number is from the middle 4
			rowcol = (i & 32) | ((i & 1) ? 16 : 0) | ((i >> 1) & 0xf);
			for(j = 0; j < 4; j++)	// For each output bit
			{
				if(si[s][rowcol] & (8 >> j))
					val |= 1L << (31 - pbox[4*s + j]);
			}
			m_sp[s][i] = val;
		}
	}
}

VOID CDes::PermInit (CHAR perm[16][16][8], CHAR p[64])
{
	// initialize a perm array
	// 64-bit, either init or final
	register INT l, j, k;
	INT i, m;

	// Clear the permutation array
	for(i = 0; i < 16; i++)
	{
		for(j = 0; j < 16; j++)
		{
			for(k = 0; k < 8; k++)
				perm[i][j][k] = 0;
		}
	}

	for(i = 0; i < 16; i++)			// each input nibble position
	{
		for(j = 0; j < 16; j++)		// each possible input nibble
		{
			for(k = 0; k < 64; k++)	// each output bit position
			{
				l = p[k] - 1;		// where does this bit come from
				if((l >> 2) != i)	// does it come from input posn?
					continue;		// if not, bit k is 0
				if(!(j & nibblebit[l & 3]))
					continue;		// any such bit in input?
				m = k & 07;			// which bit is this in the byte
				perm[i][j][k>>3] |= bytebit[m];
			}
		}
	}
}

VOID CDes::Permute (CHAR* inblock, CHAR perm[16][16][8], CHAR* outblock)
{
	// Permute inblock with perm result into outblock,64 bits 2K bytes defining perm.
	register INT i,j;
	register CHAR *ib, *ob; 			// ptr to input or output block
	register CHAR *p, *q;

	if(perm == NULL)
	{
		// No permutation, just copy
		for(i = 8; i != 0; i--)
			*outblock++ = *inblock++;
		return;
	}

	// Clear output block
	for(i = 8, ob = outblock; i != 0; i--)
		*ob++ = 0;

	ib = inblock;
	for(j = 0; j < 16; j += 2, ib++)	// for each input nibble
	{
		ob = outblock;
		p = perm[j][(*ib >> 4) & 017];
		q = perm[j + 1][*ib & 017];
		for(i = 8; i != 0; i--)			// and each output byte
			*ob++ |= *p++ | *q++;		/* OR the masks together*/
	}
}

LONG CDes::NonLinearF (ULONG r, BYTE subkey[8])
{
	// The nonlinear function f(r,k), the heart of DES
	// 32 bits
	// 48-bit key for this round
	register ULONG rval, rt;

	// Run E(R) ^ K through the combined S & P boxes
	// This code takes advantage of a convenient regularity in
	// E, namely that each group of 6 bits in E(R) feeding
	// a single S-box is a contiguous segment of R.
	rt = (r >> 1) | ((r & 1) ? 0x80000000 : 0);

	rval = 0;
	rval |= m_sp[0][((rt >> 26) ^ *subkey++) & 0x3f];
	rval |= m_sp[1][((rt >> 22) ^ *subkey++) & 0x3f];
	rval |= m_sp[2][((rt >> 18) ^ *subkey++) & 0x3f];
	rval |= m_sp[3][((rt >> 14) ^ *subkey++) & 0x3f];
	rval |= m_sp[4][((rt >> 10) ^ *subkey++) & 0x3f];
	rval |= m_sp[5][((rt >> 6) ^ *subkey++) & 0x3f];
	rval |= m_sp[6][((rt >> 2) ^ *subkey++) & 0x3f];

	rt = (r << 1) | ((r & 0x80000000) ? 1 : 0);

	rval |= m_sp[7][(rt ^ *subkey) & 0x3f];

	return rval;
}

VOID CDes::CypherRound (INT num, ULONG* block)
{
	// Do one DES cypher round i.e. the num-th one

	// The rounds are numbered from 0 to 15. On even rounds
	// the right half is fed to f() and the result exclusive-ORs
	// the left half; on odd rounds the reverse is done.

	if(num & 1)
		block[1] ^= NonLinearF(block[0],m_kn[num]);
	else
		block[0] ^= NonLinearF(block[1],m_kn[num]);
}          

ULONG CDes::ByteSwap (ULONG x)
{
	register CHAR *cp, tmp;

	cp = (CHAR*)&x;
	tmp = cp[3];
	cp[3] = cp[0];
	cp[0] = tmp;

	tmp = cp[2];
	cp[2] = cp[1];
	cp[1] = tmp;

	return x;
}