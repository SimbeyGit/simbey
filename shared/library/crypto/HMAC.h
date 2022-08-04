#ifndef	_H_HMAC
#define	_H_HMAC

#include "Md5.h"
#include "SHA1.h"
#include "SHA256.h"
#include "SHA3.h"
#include "Whirlpool.h"

#define	HMAC_PAD_SIZE		64

class CHmac : public CMessageDigest
{
protected:
	CMessageDigest* m_pHash;
	BYTE m_kopad[HMAC_PAD_SIZE];

public:
	CHmac ();
	~CHmac ();

	virtual VOID HmacInitRfc2104 (const BYTE* pcbKey, INT cKey) = 0;
	VOID HmacInitMicrosoft (const BYTE* pcbKey, INT cKey);

	// CMessageDigest
	virtual VOID Reset (VOID);
	virtual VOID AddData (const BYTE* pcbData, UINT cData);
	virtual UINT GetDigestSize (VOID);
	virtual UINT GetHexKeySize (VOID);
	virtual VOID GetHexKey (LPSTR lpszKey);

protected:
	VOID HmacInitialize (const BYTE* pcbKey, INT cKey);
};

class CHmacMd5 : public CHmac
{
private:
	CMd5 m_md5;

public:
	CHmacMd5 ();

	// CHmac
	virtual VOID HmacInitRfc2104 (const BYTE* pcbKey, INT cKey);

	// CMessageDigest
	virtual VOID GetDigest (LPBYTE lpDigest);
};

class CHmacSHA1 : public CHmac
{
private:
	CSHA1 m_sha1;

public:
	CHmacSHA1 ();

	// CHmac
	virtual VOID HmacInitRfc2104 (const BYTE* pcbKey, INT cKey);

	// CMessageDigest
	virtual VOID GetDigest (LPBYTE lpDigest);
};

class CHmacSHA256 : public CHmac
{
private:
	CSHA256 m_sha256;

public:
	CHmacSHA256 ();

	// CHmac
	virtual VOID HmacInitRfc2104 (const BYTE* pcbKey, INT cKey);

	// CMessageDigest
	virtual VOID GetDigest (LPBYTE lpDigest);
};

class CHmacSHA3 : public CHmac
{
private:
	CSHA3 m_sha3;

public:
	CHmacSHA3 (UINT nBitSize);

	// CHmac
	virtual VOID HmacInitRfc2104 (const BYTE* pcbKey, INT cKey);

	// CMessageDigest
	virtual VOID GetDigest (LPBYTE lpDigest);
};

class CHmacWhirlpool : public CHmac
{
private:
	CWhirlpool m_whirlpool;

public:
	CHmacWhirlpool ();

	// CHmac
	virtual VOID HmacInitRfc2104 (const BYTE* pcbKey, INT cKey);

	// CMessageDigest
	virtual VOID GetDigest (LPBYTE lpDigest);
};

#endif
