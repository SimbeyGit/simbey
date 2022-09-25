#include <math.h>
#include <windows.h>
#include <gl\gl.h>
#include "Library\Core\CoreDefs.h"
#include "ModelTemplates.h"
#include "SIFModel.h"

#ifndef	USHORT_MAX
	#define	USHORT_MAX	65535
#endif

const MATERIAL c_matDefault =
{
	{ 0.2f, 0.2f, 0.2f, 1.0f },	// Ambient
	{ 0.8f, 0.8f, 0.8f, 1.0f },	// Diffuse
	{ 0.0f, 0.0f, 0.0f, 1.0f },	// Specular
	{ 0.0f, 0.0f, 0.0f, 1.0f },	// Emission
	0.0f						// Shininess
};

// CSIFMeshData

CSIFMeshData::CSIFMeshData () :
	m_Material(c_matDefault)
{
}

CSIFMeshData::~CSIFMeshData ()
{
}

HRESULT CSIFMeshData::AddFace (FPOINT* ppt, INT cPoints, __out_opt ULONG* pidFace, FLOAT rTolerance)
{
	HRESULT hr;
	FACE Face = {0};
	sysint cFaces = m_mapFace.Length();
	ULONG idFace;

	for(INT i = 0; i < cPoints; i++)
	{
		Face.nVertex[i] = MatchVertex(ppt + i, rTolerance);
		if(0 == Face.nVertex[i])
		{
			Face.nVertex[i] = TGetFreeItemID(m_mapVertex);
			Check(m_mapVertex.Add(Face.nVertex[i], ppt[i]));
		}
	}

	for(sysint i = 0; i < cFaces; i++)
	{
		FACE* pFace = m_mapFace.GetValuePtr(i);

		if(IsEquivalentFace(pFace->nVertex, Face.nVertex))
		{
			if(pidFace)
				*pidFace = m_mapFace.GetKey(i);
			hr = S_FALSE;
			goto Cleanup;
		}
	}

	Geometry::GetPlaneNormal(ppt[0], ppt[1], ppt[2], &Face.fNormal);
	idFace = TGetFreeItemID(m_mapFace);
	Check(m_mapFace.Add(idFace, Face));

	if(pidFace)
		*pidFace = idFace;

Cleanup:
	return hr;
}

HRESULT CSIFMeshData::AddFacesFromQuad (FPOINT* ppt, INT cPoints, FLOAT rTolerance)
{
	HRESULT hr;

	if(3 == cPoints)
		Check(AddFace(ppt, 3, NULL, rTolerance));
	else
	{
		FPOINT fpt[3];

		CheckIf(4 != cPoints, E_INVALIDARG);
		fpt[0] = ppt[0];
		fpt[1] = ppt[1];
		fpt[2] = ppt[2];
		Check(AddFace(fpt, ARRAYSIZE(fpt), NULL, rTolerance));
		fpt[0] = ppt[0];
		fpt[1] = ppt[2];
		fpt[2] = ppt[3];
		Check(AddFace(fpt, ARRAYSIZE(fpt), NULL, rTolerance));
	}

Cleanup:
	return hr;
}

ULONG CSIFMeshData::MatchVertex (const FPOINT* ppt, FLOAT rTolerance)
{
	ULONG idVertex = 0;

	for(INT i = 0; i < m_mapVertex.Length(); i++)
	{
		FPOINT* pVertex = m_mapVertex.GetValuePtr(i);
		if(fabs(pVertex->x - ppt->x) < rTolerance &&
			fabs(pVertex->y - ppt->y) < rTolerance &&
			fabs(pVertex->z - ppt->z) < rTolerance)
		{
			idVertex = m_mapVertex.GetKey(i);
			break;
		}
	}

	return idVertex;
}

ULONG CSIFMeshData::FindClosestPoint (FPOINT* ppt, FLOAT rTolerance)
{
	ULONG idVertex = 0;
	FLOAT rClosest = rTolerance;

	for(INT i = 0; i < m_mapVertex.Length(); i++)
	{
		FPOINT* pVertex = m_mapVertex.GetValuePtr(i);
		FLOAT rDist = Geometry::PointDistance(pVertex, ppt);
		if(rDist < rClosest)
		{
			rClosest = rDist;
			idVertex = m_mapVertex.GetKey(i);
		}
	}

	return idVertex;
}

VOID CSIFMeshData::RecalculateNormals (ULONG idVertex)
{
	for(sysint i = 0; i < m_mapFace.Length(); i++)
	{
		FACE* pFace = m_mapFace.GetValuePtr(i);
		for(INT n = 0; n < ARRAYSIZE(pFace->nVertex); n++)
		{
			if(idVertex == pFace->nVertex[n])
			{
				FPOINT* pfptA, *pfptB, *pfptC;

				SideAssertHr(m_mapVertex.FindPtr(pFace->nVertex[0], &pfptA));
				SideAssertHr(m_mapVertex.FindPtr(pFace->nVertex[1], &pfptB));
				SideAssertHr(m_mapVertex.FindPtr(pFace->nVertex[2], &pfptC));

				Geometry::GetPlaneNormal(*pfptC, *pfptB, *pfptA, &pFace->fNormal);
				break;
			}
		}
	}
}

HRESULT CSIFMeshData::GetJoint (PCSTR pcszJoint, __deref_out JOINT** ppJoint)
{
	HRESULT hr = m_mapJoint.FindPtr(pcszJoint, ppJoint);
	if(FAILED(hr))
	{
		JOINT joint = {0};
		hr = m_mapJoint.Add(pcszJoint, joint);
		if(SUCCEEDED(hr))
			hr = m_mapJoint.FindPtr(pcszJoint, ppJoint);
	}
	return hr;
}

BOOL CSIFMeshData::HasJoint (PCSTR pcszJoint)
{
	return m_mapJoint.HasItem(pcszJoint);
}

HRESULT CSIFMeshData::BuildMirrorFrom (CSIFMeshData* pMesh, TNamedMapA<PCSTR>& mapNodeMirrors)
{
	HRESULT hr = S_FALSE;
	TMap<ULONG, FPOINT>& mapVertex = pMesh->m_mapVertex;
	TMap<ULONG, FACE>& mapFace = pMesh->m_mapFace;
	TNamedMapA<JOINT>& mapJoint = pMesh->m_mapJoint;

	m_mapVertex.Clear();
	m_mapFace.Clear();
	m_mapJoint.Clear();

	for(sysint i = 0; i < mapFace.Length(); i++)
	{
		ULONG idFace;
		FACE face, *pFace;
		FPOINT fpPoints[3];

		// Get the face from source mesh.
		Check(mapFace.GetValueChecked(i, &face));

		// Swap the first two vertices.
		Check(mapVertex.Find(face.nVertex[0], fpPoints + 2));
		Check(mapVertex.Find(face.nVertex[2], fpPoints + 0));

		// The third vertex is still the third vertex.
		Check(mapVertex.Find(face.nVertex[1], fpPoints + 1));

		// Mirror the x coordinates.
		fpPoints[0].x = -fpPoints[0].x;
		fpPoints[1].x = -fpPoints[1].x;
		fpPoints[2].x = -fpPoints[2].x;

		// Add the face and get a pointer to it.
		Check(AddFace(fpPoints, ARRAYSIZE(fpPoints), &idFace));
		Check(m_mapFace.FindPtr(idFace, &pFace));

		// Setup the texture coordinates (swap first two).
		pFace->Align[0] = face.Align[2];
		pFace->Align[1] = face.Align[1];
		pFace->Align[2] = face.Align[0];

		pFace->idLayer = face.idLayer;
	}

	for(sysint i = 0; i < m_mapVertex.Length(); i++)
	{
		ULONG idVertex = m_mapVertex.GetKey(i);
		RecalculateNormals(idVertex);
	}

	for(sysint i = 0; i < mapJoint.Length(); i++)
	{
		PCSTR pcszName;
		JOINT joint;

		// Get the original joint's name and position/rotation data.
		Check(mapJoint.GetKeyAndValue(i, &pcszName, &joint));

		// Translate the original joint's name to the mirrored name.
		if(SUCCEEDED(mapNodeMirrors.Find(pcszName, &pcszName)))
		{
			joint.fPos.x = -joint.fPos.x;
			if(0.0f < joint.yRot)
				joint.yRot = 360.0f - joint.yRot;
			if(0.0f < joint.zRot)
				joint.zRot = 360.0f - joint.zRot;

			// Add the mirrored joint.
			Check(m_mapJoint.Add(pcszName, joint));
		}
	}

	CopyMemory(&m_Material, &pMesh->m_Material, sizeof(MATERIAL));

Cleanup:
	return hr;
}

HRESULT CSIFMeshData::FindFace (FPOINT* ppt, ULONG* pidFace)
{
	HRESULT hr;

	for(sysint i = 0; i < m_mapFace.Length(); i++)
	{
		FACE face;
		FPOINT fpA, fpB, fpC;

		Check(m_mapFace.GetValueChecked(i, &face));
		Check(m_mapVertex.Find(face.nVertex[0], &fpA));
		Check(m_mapVertex.Find(face.nVertex[1], &fpB));
		Check(m_mapVertex.Find(face.nVertex[2], &fpC));
		if(Geometry::PointInTriangleNew(ppt, &fpA, &fpB, &fpC))
		{
			*pidFace = m_mapFace.GetKey(i);
			return S_OK;
		}
	}

	hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

Cleanup:
	return hr;
}

HRESULT CSIFMeshData::SaveToStream (ISequentialStream* pStream)
{
	HRESULT hr;
	ULONG cb, cItems;

	Check(pStream->Write(&m_Material, sizeof(m_Material), &cb));

	RemoveVirtualVertices();

	cItems = static_cast<ULONG>(m_mapVertex.Length());
	Check(pStream->Write(&cItems, sizeof(cItems), &cb));
	for(ULONG i = 0; i < cItems; i++)
	{
		ULONG idVertex;
		FPOINT fp;

		Check(m_mapVertex.GetKeyAndValue(i, &idVertex, &fp));
		Check(pStream->Write(&idVertex, sizeof(idVertex), &cb));
		Check(pStream->Write(&fp, sizeof(fp), &cb));
	}

	cItems = static_cast<ULONG>(m_mapFace.Length());
	Check(pStream->Write(&cItems, sizeof(cItems), &cb));
	for(ULONG i = 0; i < cItems; i++)
	{
		ULONG idFace;
		FACE face;

		Check(m_mapFace.GetKeyAndValue(i, &idFace, &face));
		Check(pStream->Write(&idFace, sizeof(idFace), &cb));
		Check(pStream->Write(&face, sizeof(face), &cb));
	}

	cItems = static_cast<ULONG>(m_mapJoint.Length());
	Check(pStream->Write(&cItems, sizeof(cItems), &cb));
	for(ULONG i = 0; i < cItems; i++)
	{
		PCSTR pcszJoint;
		JOINT joint;

		Check(m_mapJoint.GetKeyAndValue(i, &pcszJoint, &joint));
		Check(WriteString(pStream, pcszJoint));
		Check(pStream->Write(&joint, sizeof(joint), &cb));
	}

Cleanup:
	return hr;
}

HRESULT CSIFMeshData::LoadFromStream (ISequentialStream* pStream)
{
	HRESULT hr;
	ULONG cb, cItems;
	PSTR pszJoint = NULL;

	Check(pStream->Read(&m_Material, sizeof(m_Material), &cb));

	Check(pStream->Read(&cItems, sizeof(cItems), &cb));
	for(ULONG i = 0; i < cItems; i++)
	{
		ULONG idVertex;
		FPOINT fp;

		Check(pStream->Read(&idVertex, sizeof(idVertex), &cb));
		Check(pStream->Read(&fp, sizeof(fp), &cb));
		Check(m_mapVertex.Add(idVertex, fp));
	}

	Check(pStream->Read(&cItems, sizeof(cItems), &cb));
	for(ULONG i = 0; i < cItems; i++)
	{
		ULONG idFace;
		FACE face;

		Check(pStream->Read(&idFace, sizeof(idFace), &cb));
		Check(pStream->Read(&face, sizeof(face), &cb));
		Check(m_mapFace.Add(idFace, face));
	}

	Check(pStream->Read(&cItems, sizeof(cItems), &cb));
	for(ULONG i = 0; i < cItems; i++)
	{
		INT cch;
		JOINT joint;

		Check(LoadString(pStream, &pszJoint, &cch));
		Check(pStream->Read(&joint, sizeof(joint), &cb));
		Check(m_mapJoint.Add(pszJoint, joint));
		__delete_array pszJoint; pszJoint = NULL;
	}

Cleanup:
	__delete_array pszJoint;
	return hr;
}

HRESULT CSIFMeshData::AddVirtualPoint (const FPOINT& fpt, ULONG& idVertex)
{
	HRESULT hr;
	ULONG v = FIRST_VIRTUAL_VERTEX;

	for(;;)
	{
		if(!m_mapVertex.HasItem(v))
		{
			Check(m_mapVertex.Add(v, fpt));
			idVertex = v;
			break;
		}
		CheckIf(0xFFFFFFFF == v++, E_FAIL);
	}

Cleanup:
	return hr;
}

VOID CSIFMeshData::RemoveVirtualVertices (VOID)
{
	ULONG v = FIRST_VIRTUAL_VERTEX;
	for(;;)
	{
		m_mapVertex.Remove(v, NULL);
		if(0xFFFFFFFF == v++)
			break;
	}
}

BOOL CSIFMeshData::IsEquivalentFace (ULONG* prgFaceA, ULONG* prgFaceB)
{
	ULONG rgSet[5];

	if(0 == memcmp(prgFaceA, prgFaceB, sizeof(ULONG) * 3))
		return TRUE;

	CopyMemory(rgSet, prgFaceA, sizeof(ULONG) * 3);

	rgSet[3] = prgFaceA[0];
	if(0 == memcmp(rgSet + 1, prgFaceB, sizeof(ULONG) * 3))
		return TRUE;

	rgSet[4] = prgFaceA[1];
	if(0 == memcmp(rgSet + 2, prgFaceB, sizeof(ULONG) * 3))
		return TRUE;

	return FALSE;
}

// CSIFModel

CSIFModel::CSIFModel (CModelTemplate* pTemplate, bool fOwnTemplate) :
	m_pTemplate(pTemplate),
	m_fOwnTemplate(fOwnTemplate),
	m_pSIF(NULL),
	m_nTexture(0),
	m_wScaleNumerator(1),
	m_wScaleDenominator(64)
{
}

CSIFModel::~CSIFModel ()
{
	if(m_pSIF)
	{
		m_pSIF->Close();
		m_pSIF->Release();
	}
	for(INT i = 0; i < m_mapMeshData.Length(); i++)
		__delete *m_mapMeshData.GetValuePtr(i);
	if(m_fOwnTemplate)
		__delete m_pTemplate;
}

HRESULT CSIFModel::GetMeshData (PCSTR pcszMesh, __deref_out CSIFMeshData** ppMeshData)
{
	HRESULT hr;
	CSIFMeshData* pMeshData = NULL;

	if(SUCCEEDED(m_mapMeshData.Find(pcszMesh, ppMeshData)))
		hr = S_OK;
	else
	{
		pMeshData = __new CSIFMeshData;
		CheckAlloc(pMeshData);
		Check(m_mapMeshData.Add(pcszMesh, pMeshData));
		*ppMeshData = pMeshData;
		pMeshData = NULL;
	}

Cleanup:
	SafeDelete(pMeshData);
	return hr;
}

BOOL CSIFModel::HasMeshData (PCSTR pcszMesh)
{
	return m_mapMeshData.HasItem(pcszMesh);
}

HRESULT CSIFModel::GetMeshByIndex (sysint idxMesh, __out PCSTR* pcszMesh, __deref_out CSIFMeshData** ppMeshData)
{
	return m_mapMeshData.GetKeyAndValue(idxMesh, pcszMesh, ppMeshData);
}

ISimbeyInterchangeFile* CSIFModel::GetSIF (VOID)
{
	return m_pSIF;
}

VOID CSIFModel::SetSIF (ISimbeyInterchangeFile* pSIF)
{
	if(m_pSIF)
		m_pSIF->Close();

	ReplaceInterface(m_pSIF, pSIF);
}

VOID CSIFModel::InvalidateTexture (VOID)
{
	glDeleteTextures(1, &m_nTexture);
	m_nTexture = 0;
}

HRESULT CSIFModel::GetTexture (__out UINT* pnTexture)
{
	HRESULT hr;

	if(0 == m_nTexture)
	{
		CheckIfIgnore(NULL == m_pSIF, E_FAIL);
		Check(sifMergeCanvasToOpenGLTexture32(m_pSIF, 0, 0, 0, 0, &m_nTexture));
	}

	*pnTexture = m_nTexture;
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CSIFModel::SaveToStream (ISequentialStream* pStream)
{
	HRESULT hr;
	sysint cMeshes = m_mapMeshData.Length();
	USHORT usMeshes;
	ULONG cb;

	Check(pStream->Write(&m_wScaleNumerator, sizeof(m_wScaleNumerator), &cb));
	Check(pStream->Write(&m_wScaleDenominator, sizeof(m_wScaleDenominator), &cb));

	CheckIf(cMeshes > USHORT_MAX, E_FAIL);
	usMeshes = static_cast<USHORT>(cMeshes);

	Check(pStream->Write(&usMeshes, sizeof(usMeshes), &cb));
	for(sysint i = 0; i < cMeshes; i++)
	{
		PCSTR pcszName;
		CSIFMeshData* pMesh;

		Check(m_mapMeshData.GetKeyAndValue(i, &pcszName, &pMesh));
		Check(WriteString(pStream, pcszName));
		Check(pMesh->SaveToStream(pStream));
	}

Cleanup:
	return hr;
}

HRESULT CSIFModel::LoadFromStream (ISequentialStream* pStream)
{
	HRESULT hr;
	USHORT usMeshes;
	ULONG cb;
	PSTR pszMesh = NULL;
	CSIFMeshData* pData = NULL;

	Check(pStream->Read(&m_wScaleNumerator, sizeof(m_wScaleNumerator), &cb));
	Check(pStream->Read(&m_wScaleDenominator, sizeof(m_wScaleDenominator), &cb));

	Check(pStream->Read(&usMeshes, sizeof(usMeshes), &cb));

	for(USHORT i = 0; i < usMeshes; i++)
	{
		INT cch;

		Check(LoadString(pStream, &pszMesh, &cch));

		pData = __new CSIFMeshData;
		CheckAlloc(pData);

		Check(pData->LoadFromStream(pStream));
		Check(m_mapMeshData.Add(pszMesh, pData));
		pData = NULL;

		__delete_array pszMesh; pszMesh = NULL;
	}

Cleanup:
	__delete pData;
	__delete_array pszMesh;
	return hr;
}
