#include <windows.h>
#include "MessageDigest.h"

VOID CMessageDigest::KeyToHex (LPBYTE lpDigest, UINT cDigest, LPSTR lpszHex)
{
	UINT i, v, n;
	for(i = 0; i < cDigest; i++)
	{
		v = lpDigest[i];
		n = v / 16;
		if(n < 10)
			*lpszHex = '0' + n;
		else
			*lpszHex = 'a' + (n - 10);
		lpszHex++;
		n = v % 16;
		if(n < 10)
			*lpszHex = '0' + n;
		else
			*lpszHex = 'a' + (n - 10);
		lpszHex++;
	}
	*lpszHex = 0;
}
