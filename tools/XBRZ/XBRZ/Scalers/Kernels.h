#pragma once

class Kernel3x3
{
public:
	int m_data[3 * 3];
};

/*
        input kernel area naming convention:
        -----------------
        | A | B | C | D |
        ----|---|---|---|
        | E | F | G | H | //evalute the four corners between F, G, J, K
        ----|---|---|---| //input pixel is at position F
        | I | J | K | L |
        ----|---|---|---|
        | M | N | O | P |
        -----------------
*/
class Kernel4x4
{
public:
	int A, B, C, D;
	int E, F, G, H;
	int I, J, K, L;
	int M, N, O, P;
};
