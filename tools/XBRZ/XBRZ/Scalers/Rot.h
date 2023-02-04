#pragma once

namespace Rot
{
	const int MaxRotations = 4;	// Number of 90 degree rotations
	const int MaxPositions = 9;
	const int DefaultSide = 3;

	int data[MaxRotations * MaxPositions];

	void RotateClockwise (int* pnSquareMatrix, int* pnResult, int nSide)
	{
		for(int i = 0; i < nSide; i++)
		{
			for(int j = 0; j < nSide; j++)
				pnResult[i * nSide + j] = pnSquareMatrix[(nSide - j - 1) * nSide + i];
		}
	}

	void Initialize ()
	{
		int nRotation[MaxPositions];

		for(int i = 0; i < MaxPositions; i++)
			nRotation[i] = i;

		for(int nRot = 0; nRot < MaxRotations; nRot++)
		{
			for(int nPos = 0; nPos < MaxPositions; nPos++)
				data[nPos * MaxRotations + nRot] = nRotation[nPos];

			int nNext[MaxPositions];
			RotateClockwise(nRotation, nNext, DefaultSide);
			CopyMemory(nRotation, nNext, sizeof(nNext));
		}
	}
};
