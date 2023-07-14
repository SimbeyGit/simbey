#pragma once

#pragma pack(push, 1)

// Format for ICO files

typedef struct
{
	WORD wReserved;			// Reserved, must be 0
	WORD wType;				// Resource type (1 for icons)
	WORD wCount;			// Number of ICONDIRENTRY items following
} ICONHEAD;

typedef struct
{
	BYTE bWidth;			// Width, in pixels, of the image
	BYTE bHeight;			// Height, in pixels, of the image
	BYTE bColorCount;		// Number of colors in image (0 if >=8bpp)
	BYTE bReserved;			// Reserved (must be 0)
	WORD wPlanes;			// Color Planes
	WORD wBitCount;			// Bits per pixel
	DWORD dwBytesInRes;		// How many bytes in this resource?
	DWORD dwImageOffset;	// Where in the file is this image?
} ICONDIRENTRY, *PICONDIRENTRY;

// Format for ICO data in DLL and EXE modules

struct GROUPICONHEADER
{
	WORD wReserved;			// reserved, must be 0
	WORD wResourceType;		// type is 1 for icons
	WORD wImageCount;		// number of icons in structure
};

struct GROUPICONENTRY
{
	BYTE bWidth;			// icon width (32)
	BYTE bHeight;			// icon height (32)
	BYTE bColors;			// colors (0 means more than 8 bits per pixel)
	BYTE bReserved;			// reserved, must be 0
	WORD wPlanes;			// color planes
	WORD wBitsPerPixel;		// bit depth
	DWORD dwImageSize;		// size of structure
	WORD wResourceID;		// resource ID
	WORD wReserved;
};

#pragma pack(pop)
