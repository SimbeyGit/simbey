#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Published\JSON.h"
#include "3rdParty\glew\include\GL\glew.h"
#include "Published\SIFGL.h"
#include "Package\SIFPackage.h"
#include "TemplateData.h"
#include "Models.h"

inline VOID SendVertex (CSIFMeshData* pMesh, ULONG nVertex, DOUBLE dblScale)
{
	FPOINT* pPoint = pMesh->m_mapVertex[nVertex];
	glVertex3d(static_cast<DOUBLE>(pPoint->x) * dblScale,
		static_cast<DOUBLE>(pPoint->y) * dblScale,
		static_cast<DOUBLE>(pPoint->z) * dblScale);
}

///////////////////////////////////////////////////////////////////////////////
// CModel
///////////////////////////////////////////////////////////////////////////////

CModel::CModel () :
	m_nDisplayList(0),
	m_rstrEntityW(NULL),
	m_rstrPickupW(NULL),
	m_fObstacle(false),
	m_fSpear(false),
	m_eKey(None),
	m_nPoints(0),
	m_pModel(NULL)
{
}

CModel::~CModel ()
{
	__delete m_pModel;

	if(0 != m_nDisplayList)
		glDeleteLists(m_nDisplayList, 1);

	RStrRelease(m_rstrPickupW);
	RStrRelease(m_rstrEntityW);
}

HRESULT CModel::Load (ISimbeyInterchangeFile* pSIF, CRectPlacement* pPacker, RSTRING rstrNameW, CSIFPackage* pDir, IJSONObject* pDef)
{
	HRESULT hr;
	TStackRef<ISimbeyInterchangeFile> srModelSIF;
	TStackRef<IJSONValue> srv;
	TStackRef<IPersistedFile> srData;
	ULARGE_INTEGER uliData;
	INT nResult;
	RSTRING rstrTypeW = NULL, rstrSIFW = NULL, rstrModelW = NULL, rstrKeyW = NULL;

	Check(RStrCompareIW(rstrNameW, L"Spear", &nResult));
	m_fSpear = 0 == nResult;

	Check(pDef->FindNonNullValueW(L"type", &srv));
	Check(srv->GetString(&rstrTypeW));
	Check(RStrCompareIW(rstrTypeW, L"obstacle", &nResult));
	m_fObstacle = 0 == nResult;
	srv.Release();

	Check(pDef->FindNonNullValueW(L"sif", &srv));
	Check(srv->GetString(&rstrSIFW));
	srv.Release();

	Check(pDir->OpenSIF(RStrToWide(rstrSIFW), &srModelSIF));

	Check(pDef->FindNonNullValueW(L"model", &srv));
	Check(srv->GetString(&rstrModelW));
	srv.Release();

	Check(pDir->OpenFile(RStrToWide(rstrModelW), RStrLen(rstrModelW), &srData, &uliData));
	Check(LoadModelData(pSIF, pPacker, srModelSIF, srData, uliData.QuadPart));

	Check(pDef->FindNonNullValueW(L"entity", &srv));
	Check(srv->GetString(&m_rstrEntityW));
	srv.Release();

	if(SUCCEEDED(pDef->FindNonNullValueW(L"pickup", &srv)))
	{
		Check(srv->GetString(&m_rstrPickupW));
		srv.Release();
	}

	if(SUCCEEDED(pDef->FindNonNullValueW(L"key", &srv)))
	{
		Check(srv->GetString(&rstrKeyW));

		Check(RStrCompareIW(rstrKeyW, L"Silver", &nResult));
		if(0 == nResult)
			m_eKey = Silver;
		else
		{
			Check(RStrCompareIW(rstrKeyW, L"Gold", &nResult));
			if(0 == nResult)
				m_eKey = Gold;
		}

		srv.Release();
	}

	if(SUCCEEDED(pDef->FindNonNullValueW(L"points", &srv)))
	{
		Check(srv->GetInteger(&m_nPoints));
		srv.Release();
	}

Cleanup:
	if(srModelSIF)
		srModelSIF->Close();

	RStrRelease(rstrKeyW);
	RStrRelease(rstrModelW);
	RStrRelease(rstrSIFW);
	RStrRelease(rstrTypeW);
	return hr;
}

HRESULT CModel::RebuildList (ISimbeyInterchangeFile* pSIF)
{
	HRESULT hr;
	CSIFMeshData* pMesh;
	FRECT frc = {0};
	FLOAT rWidth = 0.0f, rHeight = 0.0f;
	DOUBLE dblScale = static_cast<DOUBLE>(m_pModel->m_wScaleNumerator) / static_cast<DOUBLE>(m_pModel->m_wScaleDenominator);

	CheckIf(NULL == m_pModel, E_UNEXPECTED);

	if(0 != m_nDisplayList)
		glDeleteLists(m_nDisplayList, 1);
	m_nDisplayList = glGenLists(1);
	CheckIf(0 == m_nDisplayList, E_FAIL);

	Check(m_pModel->GetMeshData("Model.Normal", &pMesh));

	glNewList(m_nDisplayList, GL_COMPILE);

	glMaterialfv(GL_FRONT, GL_AMBIENT, reinterpret_cast<GLfloat*>(&pMesh->m_Material.fAmbient));
	glMaterialfv(GL_FRONT, GL_DIFFUSE, reinterpret_cast<GLfloat*>(&pMesh->m_Material.fDiffuse));
	glMaterialfv(GL_FRONT, GL_SPECULAR, reinterpret_cast<GLfloat*>(&pMesh->m_Material.fSpecular));
	glMaterialfv(GL_FRONT, GL_EMISSION, reinterpret_cast<GLfloat*>(&pMesh->m_Material.fEmission));
	glMaterialf(GL_FRONT, GL_SHININESS, pMesh->m_Material.fShininess);

	glBegin(GL_TRIANGLES);

	for(sysint i = 0; i < pMesh->m_mapFace.Length(); i++)
	{
		FACE* pFace = pMesh->m_mapFace.GetValuePtr(i);

		if(0 != pFace->idLayer)
		{
			TStackRef<ISimbeyInterchangeFileLayer> srLayer;

			Check(pSIF->GetLayer(pFace->idLayer, &srLayer));
			Check(sifGetGLTexturePositionF(srLayer, &frc.left, &frc.top, &frc.right, &frc.bottom));
			rWidth = frc.right - frc.left;
			rHeight = frc.bottom - frc.top;
		}

		glNormal3f(pFace->fNormal.x, pFace->fNormal.y, pFace->fNormal.z);

		glTexCoord2f(frc.left + pFace->Align[0].u * rWidth, frc.top + rHeight - (pFace->Align[0].v * rHeight));
		SendVertex(pMesh, pFace->nVertex[0], dblScale);
		glTexCoord2f(frc.left + pFace->Align[1].u * rWidth, frc.top + rHeight - (pFace->Align[1].v * rHeight));
		SendVertex(pMesh, pFace->nVertex[1], dblScale);
		glTexCoord2f(frc.left + pFace->Align[2].u * rWidth, frc.top + rHeight - (pFace->Align[2].v * rHeight));
		SendVertex(pMesh, pFace->nVertex[2], dblScale);
	}

	glEnd();
	glEndList();

Cleanup:
	return hr;
}

HRESULT CModel::LoadModelData (ISimbeyInterchangeFile* pSIF, CRectPlacement* pPacker, ISimbeyInterchangeFile* pModelSIF, IPersistedFile* pData, ULONGLONG cbData)
{
	HRESULT hr;
	CModelTemplate* pTemplate = __new CModelTemplate;
	TMap<DWORD, DWORD> mapImports;
	CSIFMeshData* pMesh;

	CheckAlloc(pTemplate);
	Check(pTemplate->LoadFromStream(pData));

	m_pModel = __new CSIFModel(pTemplate, true);
	CheckAlloc(m_pModel);
	pTemplate = NULL;

	Check(m_pModel->LoadFromStream(pData));

	DWORD cLayers = pModelSIF->GetLayerCount();
	for(DWORD i = 0; i < cLayers; i++)
	{
		TStackRef<ISimbeyInterchangeFileLayer> srLayer, srNew;
		CRectPlacement::POINTSIZE psRect;
		RECT rcPos;

		Check(pModelSIF->GetLayerByIndex(i, &srLayer));
		Check(pSIF->ImportLayer(srLayer, &srNew));
		Check(srNew->GetPosition(&rcPos));

		psRect.x = 0;
		psRect.y = 0;
		psRect.cx = rcPos.right - rcPos.left;
		psRect.cy = rcPos.bottom - rcPos.top;
		CheckIf(!pPacker->AddAtEmptySpotAutoGrow(&psRect, 0, 0), HRESULT_FROM_WIN32(ERROR_OUT_OF_STRUCTURES));
		srNew->SetPosition(psRect.x, psRect.y);

		Check(mapImports.Add(srLayer->GetLayerID(), srNew->GetLayerID()));
	}

	Check(m_pModel->GetMeshData("Model.Normal", &pMesh));
	for(sysint i = 0; i < pMesh->m_mapFace.Length(); i++)
	{
		FACE* pFace = pMesh->m_mapFace.GetValuePtr(i);
		if(0 != pFace->idLayer)
			Check(mapImports.Find(pFace->idLayer, &pFace->idLayer));
	}

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CModelNamespace
///////////////////////////////////////////////////////////////////////////////

CModelNamespace::CModelNamespace ()
{
}

CModelNamespace::~CModelNamespace ()
{
	m_mapModels.DeleteAll();
}

HRESULT CModelNamespace::Add (ISimbeyInterchangeFile* pSIF, CRectPlacement* pPacker, RSTRING rstrNameW, CSIFPackage* pDir, IJSONObject* pDef)
{
	HRESULT hr;
	CModel* pModel = __new CModel;

	CheckAlloc(pModel);
	Check(pModel->Load(pSIF, pPacker, rstrNameW, pDir, pDef));
	Check(m_mapModels.Add(rstrNameW, pModel));
	pModel = NULL;

Cleanup:
	__delete pModel;
	return hr;
}

HRESULT CModelNamespace::Find (RSTRING rstrNameW, __deref_out CModel** ppModel)
{
	return m_mapModels.Find(rstrNameW, ppModel);
}

HRESULT CModelNamespace::RebuildLists (ISimbeyInterchangeFile* pSIF)
{
	HRESULT hr = S_FALSE;

	for(sysint i = 0; i < m_mapModels.Length(); i++)
	{
		CModel* pModel = *(m_mapModels.GetValuePtr(i));
		Check(pModel->RebuildList(pSIF));
	}

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
// CModels
///////////////////////////////////////////////////////////////////////////////

CModels::CModels () :
	m_pSIF(NULL),
	m_nTextures(0)
{
}

CModels::~CModels ()
{
	m_mapNamespaces.DeleteAll();

	if(m_nTextures)
		glDeleteTextures(1, &m_nTextures);

	if(m_pSIF)
	{
		m_pSIF->Close();
		SafeRelease(m_pSIF);
	}
}

HRESULT CModels::Initialize (VOID)
{
	HRESULT hr;

	Check(m_Packer.Init(512, 512));
	Check(sifCreateNew(&m_pSIF));
	Check(m_pSIF->SetCanvasPixelSize(512));

Cleanup:
	return hr;
}

HRESULT CModels::Load (CSIFPackage* pPackage, BOOL fRequired)
{
	HRESULT hr;
	TStackRef<CSIFPackage> srDir;
	TStackRef<IJSONValue> srv;
	TStackRef<IJSONObject> srData;
	TStackRef<IJSONArray> srModels;
	CModelNamespace* pNamespace = NULL;
	RSTRING rstrNamespaceW = NULL, rstrNameW = NULL;

	hr = pPackage->OpenDirectory(SLP(L"models"), &srDir);
	if(FAILED(hr))
	{
		CheckIf(!fRequired, S_FALSE);
		Check(hr);
	}

	hr = srDir->GetJSONData(SLP(L"models.json"), &srv);
	if(FAILED(hr))
	{
		CheckIf(!fRequired, S_FALSE);
		Check(hr);
	}

	Check(srv->GetObject(&srData));
	srv.Release();

	Check(srData->FindNonNullValueW(L"namespace", &srv));
	Check(srv->GetString(&rstrNamespaceW));
	srv.Release();

	pNamespace = __new CModelNamespace;
	CheckAlloc(pNamespace);

	Check(srData->FindNonNullValueW(L"models", &srv));
	Check(srv->GetArray(&srModels));

	sysint cModels = srModels->Count();
	for(sysint i = 0; i < cModels; i++)
	{
		TStackRef<IJSONObject> srDef;

		Check(srModels->GetObject(i, &srDef));
		if(srDef->HasField(RSTRING_CAST(L"model")))
		{
			srv.Release();
			Check(srDef->FindNonNullValueW(L"name", &srv));
			Check(srv->GetString(&rstrNameW));
			Check(pNamespace->Add(m_pSIF, &m_Packer, rstrNameW, srDir, srDef));

			RStrRelease(rstrNameW); rstrNameW = NULL;
		}
	}

	Check(m_mapNamespaces.Add(rstrNamespaceW, pNamespace));
	pNamespace = NULL;

Cleanup:
	__delete pNamespace;
	RStrRelease(rstrNameW);
	RStrRelease(rstrNamespaceW);
	return hr;
}

HRESULT CModels::RebuildModels (VOID)
{
	HRESULT hr;

	if(0 != m_nTextures)
	{
		glDeleteTextures(1, &m_nTextures);
		m_nTextures = 0;
	}

	Check(sifMergeCanvasToOpenGLTexture32(m_pSIF, 0, 0, 0, 0, &m_nTextures));

	for(sysint i = 0; i < m_mapNamespaces.Length(); i++)
	{
		CModelNamespace* pNamespace = *(m_mapNamespaces.GetValuePtr(i));
		Check(pNamespace->RebuildLists(m_pSIF));
	}

Cleanup:
	return hr;
}

VOID CModels::BindTexture (VOID)
{
	glBindTexture(GL_TEXTURE_2D, m_nTextures);
}

HRESULT CModels::Resolve (RSTRING rstrModelW, __deref_out CModel** ppModel)
{
	HRESULT hr;
	RSTRING rstrNamespaceW = NULL, rstrNameW = NULL;
	CModelNamespace* pModelNamespace;

	Check(SplitKey(rstrModelW, &rstrNamespaceW, &rstrNameW));
	Check(m_mapNamespaces.Find(rstrNamespaceW, &pModelNamespace));
	Check(pModelNamespace->Find(rstrNameW, ppModel));

Cleanup:
	RStrRelease(rstrNameW);
	RStrRelease(rstrNamespaceW);
	return hr;
}
