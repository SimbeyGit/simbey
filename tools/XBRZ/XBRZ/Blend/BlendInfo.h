#pragma once

#include "..\Common\RotationDegree.h"

namespace BlendInfo
{
	static short GetTopL (short b) { return (short)(b & 0x3); }
	static short GetTopR (short b) { return (short)((b >> 2) & 0x3); }
	static short GetBottomR (short b) { return (short)((b >> 4) & 0x3); }
	static short GetBottomL (short b) { return (short)((b >> 6) & 0x3); }

	static short SetTopL (short b, short bt) { return (short)(b | bt); }
	static short SetTopR (short b, short bt) { return (short)(b | (bt << 2)); }
	static short SetBottomR (short b, short bt) { return (short)(b | (bt << 4)); }
	static short SetBottomL (short b, short bt) { return (short)(b | (bt << 6)); }

	static short Rotate (short b, Rotation::Degree eRotDeg)
	{
		int l = (int)eRotDeg << 1;
		int r = 8 - l;

		return (short)(b << l | b >> r);
	}
};
