#pragma once

#ifdef	_USE_GEOMETRY_SSE

#include <smmintrin.h>

union FPOINT
{
      struct
      {
            FLOAT x;
            FLOAT y;
            FLOAT z;
      };
      __m128 m;
};

#else

struct FPOINT
{
	FLOAT x;
	FLOAT y;
	FLOAT z;
};

#endif

typedef FPOINT* PFPOINT;

typedef struct
{
	DOUBLE x;
	DOUBLE y;
	DOUBLE z;
} DPOINT, *PDPOINT;

typedef struct
{
	FLOAT u;
	FLOAT v;
} TPOINT, *PTPOINT;

typedef struct
{
	FLOAT uLeft;
	FLOAT vTop;
	FLOAT uRight;
	FLOAT vBottom;
} TPOINT_RECT, *PTPOINT_RECT;

typedef struct
{
	FLOAT left;
	FLOAT top;
	FLOAT right;
	FLOAT bottom;
} FRECT, *PFRECT;

typedef struct
{
	FLOAT red;
	FLOAT green;
	FLOAT blue;
} FCOLOR, *PFCOLOR;

typedef struct
{
	FLOAT red;
	FLOAT green;
	FLOAT blue;
	FLOAT alpha;
} FCOLOR4, *PFCOLOR4;
