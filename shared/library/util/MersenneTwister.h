#pragma once

// Based on code from:
// https://github.com/ESultanik/mtwister

#define	STATE_VECTOR_LENGTH	624
#define	STATE_VECTOR_M		397 /* changes to STATE_VECTOR_LENGTH also require changes to this */

#define	UPPER_MASK			0x80000000
#define	LOWER_MASK			0x7fffffff
#define	TEMPERING_MASK_B	0x9d2c5680
#define	TEMPERING_MASK_C	0xefc60000

class CMersenneTwister
{
private:
	ULONG m_mt[STATE_VECTOR_LENGTH];
	INT m_index;

public:
	CMersenneTwister ()
	{
		ZeroMemory(m_mt, sizeof(m_mt));
		m_index = STATE_VECTOR_LENGTH;
	}

	CMersenneTwister (ULONG nSeed)
	{
		m_mt[0] = nSeed;
		for(m_index = 1; m_index < STATE_VECTOR_LENGTH; m_index++)
			m_mt[m_index] = 6069 * m_mt[m_index - 1];
	}

	VOID Randomize (const BYTE* pcb, INT cb)
	{
		if(cb >= sizeof(m_mt))
			cb = sizeof(m_mt);
		CopyMemory(m_mt, pcb, cb);
		for(m_index = cb / sizeof(ULONG); m_index < STATE_VECTOR_LENGTH; m_index++)
			m_mt[m_index] = 6069 * m_mt[m_index - 1];
	}

	ULONG Random (VOID)
	{
		ULONG y;
		static ULONG mag[2] = {0x0, 0x9908b0df}; /* mag[x] = x * 0x9908b0df for x = 0,1 */

		if(m_index >= STATE_VECTOR_LENGTH || m_index < 0)
		{
			/* generate STATE_VECTOR_LENGTH words at a time */
			int kk;

			Assert(m_index == STATE_VECTOR_LENGTH);

			for(kk=0; kk<STATE_VECTOR_LENGTH-STATE_VECTOR_M; kk++)
			{
				y = (m_mt[kk] & UPPER_MASK) | (m_mt[kk+1] & LOWER_MASK);
				m_mt[kk] = m_mt[kk+STATE_VECTOR_M] ^ (y >> 1) ^ mag[y & 0x1];
			}
			for(; kk<STATE_VECTOR_LENGTH-1; kk++)
			{
				y = (m_mt[kk] & UPPER_MASK) | (m_mt[kk+1] & LOWER_MASK);
				m_mt[kk] = m_mt[kk+(STATE_VECTOR_M-STATE_VECTOR_LENGTH)] ^ (y >> 1) ^ mag[y & 0x1];
			}
			y = (m_mt[STATE_VECTOR_LENGTH-1] & UPPER_MASK) | (m_mt[0] & LOWER_MASK);
			m_mt[STATE_VECTOR_LENGTH-1] = m_mt[STATE_VECTOR_M-1] ^ (y >> 1) ^ mag[y & 0x1];
			m_index = 0;
		}
		y = m_mt[m_index++];
		y ^= (y >> 11);
		y ^= (y << 7) & TEMPERING_MASK_B;
		y ^= (y << 15) & TEMPERING_MASK_C;
		y ^= (y >> 18);
		return y;
	}

	inline ULONG Random (DWORD dwRange)
	{
		return Random() % dwRange;
	}

	inline ULONG Random (DWORD nMin, DWORD nMax)
	{
		return Random((nMax - nMin) + 1) + nMin;
	}
};
