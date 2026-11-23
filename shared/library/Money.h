#ifndef _H_MONEY
#define	_H_MONEY

class CMoney
{
protected:
	__int64 m_iMoney;

public:
	CMoney ();
	CMoney (LONG iMoney);
	CMoney (DOUBLE fMoney);
	CMoney (LPSTR lpszMoney);
	CMoney (__int64 iDecimal);
	~CMoney ();

	VOID ParseString (PCSTR pcszAmount);
	VOID RoundNearestPenny (VOID);

	HRESULT GetDollarAmountW (PWSTR pwzAmount, INT cchMaxAmount, __out INT* pcchWritten);
	HRESULT GetDollarAmount (PSTR pszAmount, INT cchMaxAmount, __out INT* pcchWritten);

	INT GetDecimalAmount (LPSTR lpszAmount, INT cPlaces);
	__int64 GetDecimalValue (VOID);

	BOOL operator== (CMoney& Amount);
	BOOL operator== (LONG iAmount);
	BOOL operator== (DOUBLE fAmount);

	BOOL operator> (CMoney& Amount);
	BOOL operator> (LONG iAmount);
	BOOL operator> (DOUBLE fAmount);

	BOOL operator< (CMoney& Amount);
	BOOL operator< (LONG iAmount);
	BOOL operator< (DOUBLE fAmount);

	BOOL operator>= (CMoney& Amount);
	BOOL operator>= (LONG iAmount);
	BOOL operator>= (DOUBLE fAmount);

	BOOL operator<= (CMoney& Amount);
	BOOL operator<= (LONG iAmount);
	BOOL operator<= (DOUBLE fAmount);

	VOID operator= (CMoney& Amount);
	VOID operator= (LONG iAmount);
	VOID operator= (DOUBLE fAmount);
	VOID operator= (__int64 iAmount);

	CMoney operator+ (CMoney& Amount);
	CMoney operator+ (LONG iAmount);
	CMoney operator+ (DOUBLE fAmount);

	CMoney operator- (CMoney& Amount);
	CMoney operator- (LONG iAmount);
	CMoney operator- (DOUBLE fAmount);

	CMoney operator* (CMoney& Amount);
	CMoney operator* (LONG iAmount);
	CMoney operator* (DOUBLE fAmount);

	CMoney operator/ (CMoney& Amount);
	CMoney operator/ (LONG iAmount);
	CMoney operator/ (DOUBLE fAmount);

	VOID operator+= (CMoney& Amount);
	VOID operator+= (LONG iAmount);
	VOID operator+= (DOUBLE fAmount);

	VOID operator-= (CMoney& Amount);
	VOID operator-= (LONG iAmount);
	VOID operator-= (DOUBLE fAmount);

	VOID operator*= (CMoney& Amount);
	VOID operator*= (LONG iAmount);
	VOID operator*= (DOUBLE fAmount);

	VOID operator/= (CMoney& Amount);
	VOID operator/= (LONG iAmount);
	VOID operator/= (DOUBLE fAmount);

protected:
	static __int64 ConvertLong (LONG iMoney);
	static __int64 ConvertDouble (DOUBLE fMoney);
	static __int64 ConvertString (LPCSTR lpcszMoney);
};

typedef CMoney MONEY;
typedef MONEY* LPMONEY;

#endif