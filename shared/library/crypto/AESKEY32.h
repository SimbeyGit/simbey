#pragma once

struct AES_KEY32
{
	BLOBHEADER Header;
	DWORD dwKeyLength;
	BYTE bKey[32];

	AES_KEY32 ()
	{
		ZeroMemory(this, sizeof(AES_KEY32));
		Header.bType = PLAINTEXTKEYBLOB;
		Header.bVersion = CUR_BLOB_VERSION;
		Header.reserved = 0;
		Header.aiKeyAlg = CALG_AES_256;
		dwKeyLength = sizeof(bKey);
	}

	~AES_KEY32 ()
	{
		SecureZeroMemory(this, sizeof(AES_KEY32));
	}
};
