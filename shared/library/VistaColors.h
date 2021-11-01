#ifndef	_H_VISTACOLORS
#define	_H_VISTACOLORS

typedef struct
{
	LPSTR lpszColor;
	LPSTR lpszFriendlyName;
    LPSTR lpszHexName;
	COLORREF crColor;
} VISTACOLOR, *LPVISTACOLOR;

class CVistaColors
{
public:
	static VISTACOLOR m_vColors[];

public:
	static INT Colors (VOID);

	template<typename T>
	static INT TIndex (const T* pctzFindName)
	{
		if(pctzFindName)
		{
			INT cMaxColors = Colors(), n;
			LPSTR lpszColor;
			CHAR a, b;

			for(INT c = 0; c < cMaxColors; c++)
			{
				lpszColor = m_vColors[c].lpszColor;
				if(lpszColor == NULL)
					break;

				n = 0;
				for(;;)
				{
					b = (CHAR)pctzFindName[n++];
					if(b >= 'A' && b <= 'Z')
						b += 32;
					if((b >= 'a' && b <= 'z') || b == 0)
					{
						a = *lpszColor;
						if(a != b)
							break;
						if(a == 0)
							return c;
						lpszColor++;
					}
				}
			}
		}
		return -1;
	}
};

#endif