#pragma once

class CDes
{
protected:
	INT m_iDesMode;
	BOOL m_bPermutations;

	LONG (*m_sp)[64];			// Combined S and P boxes
	BYTE (*m_kn)[8];			// 8 6-bit subkeys for each of 16 rounds, initialized by setkey()

	CHAR (*m_iperm)[16][8];		// Initial and final permutations
	CHAR (*m_fperm)[16][8];

public:
	CDes (INT iMode, BOOL bPermutations);
	~CDes ();

	VOID EncodeUsingKey (LPBYTE lpKey, const BYTE* pcbInput, LPBYTE lpOutput);

	VOID SetKey (CHAR* key);

	VOID Encode (CHAR* block);
	VOID Decode (CHAR* block);

	static VOID CreateKey (LPBYTE lpInput7, LPBYTE lpOutput8);

protected:
	VOID InitializeSandP (VOID);
	VOID PermInit (CHAR perm[16][16][8], CHAR p[64]);

	VOID Permute (CHAR* inblock, CHAR perm[16][16][8], CHAR* outblock);
	LONG NonLinearF (ULONG r, BYTE subkey[8]);
	VOID CypherRound (INT num, ULONG* block);

	static ULONG ByteSwap (ULONG x);
};