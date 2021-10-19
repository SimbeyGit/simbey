#include "Endian.h"

bool IsBigEndian (void)
{
	static unsigned long v(1);
	static bool fBigEndian(reinterpret_cast<unsigned char*>(&v)[0] == 0);
	return fBigEndian;
}
