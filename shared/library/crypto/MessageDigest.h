#ifndef	_H_MESSAGEDIGEST
#define	_H_MESSAGEDIGEST

class CMessageDigest
{
public:
	virtual VOID Reset (VOID) = 0;

	virtual VOID AddData (const BYTE* pcbData, UINT cData) = 0;

	virtual UINT GetDigestSize (VOID) = 0;
	virtual VOID GetDigest (LPBYTE lpDigest) = 0;

	virtual UINT GetHexKeySize (VOID) = 0;
	virtual VOID GetHexKey (LPSTR lpszKey) = 0;

protected:
	static VOID KeyToHex (LPBYTE lpDigest, UINT cDigest, LPSTR lpszHex);
};

#endif
