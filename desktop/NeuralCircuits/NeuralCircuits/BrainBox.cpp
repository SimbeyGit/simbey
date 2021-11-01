#include <windows.h>
#include "resource.h"
#include "Library\Core\CoreDefs.h"
#include "Library\Core\MemoryStream.h"
#include "NeuralAPI.h"
#include "Neurone.h"
#include "BiasNeurone.h"
#include "NeuralNet.h"
#include "BrainBox.h"

CBrainBox::CBrainBox (INeuralNet* lpNet)
{
	m_lpNet = lpNet;
	m_lpNet->AddRef();
}

CBrainBox::~CBrainBox ()
{
	m_lpNet->Release();
}

HRESULT CBrainBox::Load (LPBYTE lpData, DWORD cbData, BOOL fUseBiasNeuronesForBrainBox)
{
	HRESULT hr;
	CMemoryStream Line;

	hr = ReadLine(&lpData, &cbData, &Line);
	if(SUCCEEDED(hr))
	{
		LPCSTR lpcszLine = Line.TGetReadPtr<CHAR>();
		LPCSTR lpcszPtr = strchr(lpcszLine, ':');

		hr = E_FAIL;
		if(lpcszPtr)
		{
			INT nVersion = (INT)(atof(lpcszPtr + 1) * 100.0);
			if(180 == nVersion)
				hr = S_OK;
		}

		if(SUCCEEDED(hr))
		{
			Line.Reset();
			hr = ReadLine(&lpData, &cbData, &Line);
		}

		if(SUCCEEDED(hr))
		{
			lpcszLine = Line.TGetReadPtr<CHAR>();
			lpcszPtr = strchr(lpcszLine, ':');

			hr = E_FAIL;
			if(lpcszPtr)
			{
				INT cNeurones = atoi(lpcszPtr + 1);
				if(0 < cNeurones)
				{
					TMap<INT, BBDATA> mapData;
					BBDATA bbData = {0};
					RECT rcExtents = {0};
					BOOL fInitExtents = FALSE;
					INT x, y;

					for(INT i = 0; i < cNeurones; i++)
					{
						INT n = i + 1;
						BOOL fBiasNeurone = FALSE;

						do
						{
							Line.Reset();
							hr = ReadLine(&lpData, &cbData, &Line);
						} while(SUCCEEDED(hr) && 1 >= Line.DataRemaining());

						if(FAILED(hr))
							break;

						if(0 == lstrcmpi(Line.TGetReadPtr<CHAR>(), "\"Bias\""))
							fBiasNeurone = TRUE;

						Line.Reset();
						hr = ReadLine(&lpData, &cbData, &Line);
						if(FAILED(hr))
							break;
						hr = ReadInputs(Line.TGetReadPtr<CHAR>(), &bbData);
						if(FAILED(hr))
							break;

						Line.Reset();
						hr = ReadLine(&lpData, &cbData, &Line);
						if(FAILED(hr))
							break;
						hr = ReadWeights(Line.TGetReadPtr<CHAR>(), &bbData);
						if(FAILED(hr))
							break;

						if(bbData.lpaInputs->Length() != bbData.lpaWeights->Length())
						{
							hr = E_FAIL;
							break;
						}

						// Read the coordinates and create the neurone.
						Line.Reset();
						hr = ReadLine(&lpData, &cbData, &Line);
						if(FAILED(hr))
							break;
						lpcszLine = Line.TGetReadPtr<CHAR>();
						lpcszPtr = strchr(lpcszLine, ',');
						if(NULL == lpcszPtr)
						{
							hr = E_FAIL;
							break;
						}
						if(fUseBiasNeuronesForBrainBox)
						{
							if(!fBiasNeurone)
							{
								bbData.lpNeurone = __new CBiasNeurone;
								if(NULL == bbData.lpNeurone)
								{
									hr = E_OUTOFMEMORY;
									break;
								}
							}
						}
						else
						{
							bbData.lpNeurone = __new CNeurone;
							if(NULL == bbData.lpNeurone)
							{
								hr = E_OUTOFMEMORY;
								break;
							}
						}

						if(bbData.lpNeurone)
						{
							x = atoi(lpcszLine) * 10;
							y = atoi(lpcszPtr + 1) * 10;
							bbData.lpNeurone->MoveObject(x, y);

							// Update view extents.
							if(!fBiasNeurone)
							{
								if(!fInitExtents)
								{
									rcExtents.left = x;
									rcExtents.right = x;
									rcExtents.top = y;
									rcExtents.bottom = y;
									fInitExtents = TRUE;
								}
								else
								{
									if(x < rcExtents.left)
										rcExtents.left = x;
									else if(x > rcExtents.right)
										rcExtents.right = x;
									if(y > rcExtents.top)
										rcExtents.top = y;
									else if(y < rcExtents.bottom)
										rcExtents.bottom = y;
								}
							}
						}

						// Read the state information.
						Line.Reset();
						hr = ReadLine(&lpData, &cbData, &Line);
						if(FAILED(hr))
							break;

						if(bbData.lpNeurone)
						{
							lpcszLine = Line.TGetReadPtr<CHAR>();
							if(1 == atoi(lpcszLine))
							{
								hr = bbData.lpNeurone->Exec(NULL, ID_NEURONE_FIRE, 0, NULL, NULL);
								if(FAILED(hr))
									break;
							}

							bbData.lpNeurone->SetThreshold(0.5f);

							hr = m_lpNet->AttachObject(bbData.lpNeurone);
							if(FAILED(hr))
								break;
						}

						hr = mapData.Add(n, bbData);
						if(FAILED(hr))
							break;

						ZeroMemory(&bbData, sizeof(BBDATA));
					}

					SafeDelete(bbData.lpaInputs);
					SafeDelete(bbData.lpaWeights);
					SafeRelease(bbData.lpNeurone);

					if(SUCCEEDED(hr))
					{
						for(INT i = 0; i < mapData.Length(); i++)
						{
							BBDATA* pbbData = mapData.GetValuePtr(i);
							for(INT n = 0; n < pbbData->lpaInputs->Length(); n++)
							{
								INT nInput = (*pbbData->lpaInputs)[n];
								BBDATA bbSource;

								if(SUCCEEDED(mapData.Find(nInput, &bbSource)))
								{
									if(bbSource.lpNeurone)
										hr = bbSource.lpNeurone->ConnectTo(0, pbbData->lpNeurone, 0, (*pbbData->lpaWeights)[n]);
									else if(pbbData->lpNeurone)
										static_cast<CBiasNeurone*>(pbbData->lpNeurone)->SetBias((*pbbData->lpaWeights)[n]);
									if(FAILED(hr))
										break;
								}
							}

							if(FAILED(hr))
								break;
						}
					}

					x = rcExtents.left + ((rcExtents.right - rcExtents.left) >> 1);
					y = rcExtents.bottom + ((rcExtents.top - rcExtents.bottom) >> 1);

					for(INT i = 0; i < mapData.Length(); i++)
					{
						BBDATA* pbbData = mapData.GetValuePtr(i);
						INT xPos, yPos;

						if(pbbData->lpNeurone)
						{
							// Shift each neurone to be centered around the origin.
							pbbData->lpNeurone->MoveObject(-x, -y);

							// Flip each neurone around the x-axis.
							pbbData->lpNeurone->GetPosition(xPos, yPos);
							pbbData->lpNeurone->MoveObject(0, -yPos * 2);

							SafeRelease(pbbData->lpNeurone);
						}

						SafeDelete(pbbData->lpaInputs);
						SafeDelete(pbbData->lpaWeights);
					}
				}
			}
		}
	}

	return hr;
}

HRESULT CBrainBox::ReadInputs (LPCSTR lpcszInputs, BBDATA* lpbbData)
{
	HRESULT hr = E_OUTOFMEMORY;

	lpbbData->lpaInputs = __new TArray<INT>;
	if(lpbbData->lpaInputs)
	{
		hr = S_OK;

		if('"' == *lpcszInputs)
			lpcszInputs++;

		while('"' != *lpcszInputs)
		{
			INT n = atoi(lpcszInputs);
			hr = lpbbData->lpaInputs->Append(&n);
			if(FAILED(hr))
				break;
			lpcszInputs = strchr(lpcszInputs, 32);
			if(NULL == lpcszInputs)
				break;
			lpcszInputs++;
		}
	}

	return hr;
}

HRESULT CBrainBox::ReadWeights (LPCSTR lpcszWeights, BBDATA* lpbbData)
{
	HRESULT hr = E_OUTOFMEMORY;

	lpbbData->lpaWeights = __new TArray<FLOAT>;
	if(lpbbData->lpaWeights)
	{
		hr = S_OK;

		if('"' == *lpcszWeights)
			lpcszWeights++;

		while('"' != *lpcszWeights)
		{
			FLOAT r = (FLOAT)atof(lpcszWeights);
			hr = lpbbData->lpaWeights->Append(&r);
			if(FAILED(hr))
				break;
			lpcszWeights = strchr(lpcszWeights, 32);
			if(NULL == lpcszWeights)
				break;
			lpcszWeights++;
		}
	}

	return hr;
}

HRESULT CBrainBox::ReadLine (LPBYTE* lplpData, DWORD* lpcbData, ISequentialStream* lpLine)
{
	HRESULT hr;
	ULONG cbLine = 0, cb;
	LPBYTE lpData = *lplpData;
	DWORD cbData = *lpcbData;

	if(0 < cbData)
	{
		while('\n' != lpData[cbLine++] && cbLine < cbData);

		if(1 < cbLine && '\r' == lpData[cbLine - 2])
			hr = lpLine->Write(lpData, cbLine - 2, &cb);
		else
			hr = lpLine->Write(lpData, cbLine - 1, &cb);
		if(SUCCEEDED(hr))
		{
			CHAR chZero = '\0';
			hr = lpLine->Write(&chZero, sizeof(CHAR), &cb);

			*lplpData = lpData + cbLine;
			*lpcbData = cbData - cbLine;
		}
	}
	else
		hr = E_FAIL;

	return hr;
}
