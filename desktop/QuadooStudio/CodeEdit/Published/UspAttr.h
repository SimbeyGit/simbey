#pragma once

//
//	ATTR - use to define the attributes of a single run of text
//
typedef struct _ATTR
{
	COLORREF	fg;
	COLORREF	bg;
	int			len			: 16;		// length of this run (in code-units)
	int			font		: 7;		// font-index
	int			sel			: 1;		// selection flag (yes/no)
	int			ctrl		: 1;		// show as an isolated control-character
	int			eol			: 1;		// when set, prevents cursor from selecting character. only valid for last char in line
	int			reserved	: 6;		// possible underline/other styles (must be NULL)
} ATTR, *PATTR;
