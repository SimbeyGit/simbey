#pragma once

namespace Dir
{
	enum Value
	{
		NORTH = 0,
		NORTH_EAST,
		EAST,
		SOUTH_EAST,
		SOUTH,
		SOUTH_WEST,
		WEST,
		NORTH_WEST,

		N = NORTH,
		NE = NORTH_EAST,
		E = EAST,
		SE = SOUTH_EAST,
		S = SOUTH,
		SW = SOUTH_WEST,
		W = WEST,
		NW = NORTH_WEST,

		INVALID = -1
	};
}

inline Dir::Value operator~(const Dir::Value& dir)
{
	switch (dir)
	{
	case Dir::NORTH: return Dir::SOUTH;
	case Dir::NORTH_EAST: return Dir::SOUTH_WEST;
	case Dir::EAST: return Dir::WEST;
	case Dir::SOUTH_EAST: return Dir::NORTH_WEST;
	case Dir::SOUTH: return Dir::NORTH;
	case Dir::SOUTH_WEST: return Dir::NORTH_EAST;
	case Dir::WEST: return Dir::EAST;
	case Dir::NORTH_WEST: return Dir::SOUTH_EAST;
	default: return Dir::INVALID;
	}
}

namespace DirJoin
{
	enum Value
	{
		NONE = 0x00,

		N = 0x01, NE = 0x02, E = 0x04, SE = 0x08,
		S = 0x10, SW = 0x20, W = 0x40, NW = 0x80,

		/* orthogonal for map */

		HORIZONTAL = W | E,
		VERTICAL = N | S,

		OCORNER_NE = N | E,
		OCORNER_NW = N | W,
		OCORNER_SE = S | E,
		OCORNER_SW = S | W,

		CORNER_NW = N | NW | W,
		CORNER_SW = S | SW | W,
		CORNER_NE = N | NE | E,
		CORNER_SE = S | SE | E,

		OCROSS = N | E | W | S,

		TCROSS_N = N | W | E,
		TCROSS_S = S | W | E,
		TCROSS_E = N | E | S,
		TCROSS_W = N | W | S,

		EDGE_N = NW | N | NE,
		EDGE_S = SW | S | SE,
		EDGE_W = NW | W | SW,
		EDGE_E = NE | E | SE,

		EDGES_NS = EDGE_N | EDGE_S,
		EDGES_WE = EDGE_W | EDGE_E,

		/* diagonal for combat isometric */

		HORIZONTAL_NW_SE = NW | SE,
		HORIZONTAL_NE_SW = NE | SW,

		DIAGONAL_NW_SE = NW | SE,
		DIAGONAL_NE_SW = NE | SW,

		CORNER_NW_NE = NW | NE,
		CORNER_NE_SE = NE | SE,
		CORNER_SE_SW = SE | SW,
		CORNER_SW_NW = SW | NW,

		HALF_NW = NW | NE | SW,
		HALF_NE = NW | NE | SE,
		HALF_SW = SW | SE | NW,
		HALF_SE = SE | SW | NE,

		CROSS = NW | SE | NE | SW,

		ALL = 0xFF,

		ALL_NO_N = ALL - N,
		ALL_NO_S = ALL - S,
		ALL_NO_W = ALL - W,
		ALL_NO_E = ALL - E,
	};
}

static const POINT c_rgDirections[] =
{
	{ 0, -1 },	// North
	{ 1, -1 },	// North East
	{ 1, 0 },	// East
	{ 1, 1 },	// South East
	{ 0, 1 },	// South
	{ -1, 1 },	// South West
	{ -1, 0 },	// West
	{ -1, -1 }	// North West
};
