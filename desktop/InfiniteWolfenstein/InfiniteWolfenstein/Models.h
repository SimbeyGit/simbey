#pragma once

#include "Library\Core\RStrMap.h"
#include "library\Spatial\GeometryTypes.h"
#include "Library\RectPlacement.h"
#include "Model\ModelTemplates.h"
#include "Model\SIFModel.h"

interface IJSONObject;
interface IPersistedFile;
interface ISimbeyInterchangeFile;

class CSIFPackage;

class CModel
{
public:
	enum KeyType
	{
		None,
		Silver,
		Gold
	};

	UINT m_nDisplayList;
	RSTRING m_rstrEntityW;
	RSTRING m_rstrPickupW;
	bool m_fObstacle;
	bool m_fSpear;
	KeyType m_eKey;
	INT m_nPoints;

	CSIFModel* m_pModel;
	DOUBLE m_dblOffsetY;

public:
	CModel ();
	~CModel ();

	HRESULT Load (ISimbeyInterchangeFile* pSIF, CRectPlacement* pPacker, RSTRING rstrNameW, CSIFPackage* pDir, IJSONObject* pDef);
	HRESULT RebuildList (ISimbeyInterchangeFile* pSIF);

private:
	HRESULT LoadModelData (ISimbeyInterchangeFile* pSIF, CRectPlacement* pPacker, ISimbeyInterchangeFile* pModelSIF, IPersistedFile* pData, ULONGLONG cbData);
};

class CModelNamespace
{
private:
	TRStrMap<CModel*> m_mapModels;

public:
	CModelNamespace ();
	~CModelNamespace ();

	HRESULT Add (ISimbeyInterchangeFile* pSIF, CRectPlacement* pPacker, RSTRING rstrNameW, CSIFPackage* pDir, IJSONObject* pDef);
	HRESULT Find (RSTRING rstrNameW, __deref_out CModel** ppModel);
	HRESULT RebuildLists (ISimbeyInterchangeFile* pSIF);
};

class CModels
{
private:
	ISimbeyInterchangeFile* m_pSIF;
	CRectPlacement m_Packer;
	TRStrMap<CModelNamespace*> m_mapNamespaces;
	UINT m_nTextures;

public:
	CModels ();
	~CModels ();

	HRESULT Initialize (VOID);
	HRESULT Load (CSIFPackage* pPackage, BOOL fRequired);
	HRESULT RebuildModels (VOID);
	VOID BindTexture (VOID);
	HRESULT Resolve (RSTRING rstrModelW, __deref_out CModel** ppModel);
};
