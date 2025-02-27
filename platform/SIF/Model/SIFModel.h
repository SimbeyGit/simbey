#pragma once

#include "Library\Core\Map.h"
#include "Library\Spatial\Geometry.h"
#include "Published\SIFGL.h"

#define	VERTEX_TOLERANCE		0.11
#define	MAX_VIRTUAL_VERTEX		64
#define	FIRST_VIRTUAL_VERTEX	((0xFFFFFFFF - MAX_VIRTUAL_VERTEX) + 1)

class CModelTemplate;

struct FACE
{
	ULONG nVertex[3];
	TPOINT Align[3];
	FPOINT fNormal;
	ULONG idLayer;
};

struct MATERIAL
{
	FCOLOR4 fAmbient;
	FCOLOR4 fDiffuse;
	FCOLOR4 fSpecular;
	FCOLOR4 fEmission;
	FLOAT fShininess;
};

struct JOINT
{
	FPOINT fPos;
	FPOINT fRot;
};

class CSIFMeshData
{
public:
	TMap<ULONG, FPOINT> m_mapVertex;
	TMap<ULONG, FACE> m_mapFace;
	TNamedMapA<JOINT> m_mapJoint;

	MATERIAL m_Material;

public:
	CSIFMeshData ();
	~CSIFMeshData ();

	HRESULT AddFace (FPOINT* ppt, INT cPoints, __out_opt ULONG* pidFace, FLOAT rTolerance = (FLOAT)VERTEX_TOLERANCE);
	HRESULT AddFacesFromQuad (FPOINT* ppt, INT cPoints, FLOAT rTolerance = (FLOAT)VERTEX_TOLERANCE);
	ULONG MatchVertex (const FPOINT* ppt, FLOAT rTolerance = (FLOAT)VERTEX_TOLERANCE);
	ULONG FindClosestPoint (FPOINT* ppt, FLOAT rTolerance);
	VOID RecalculateNormals (ULONG idVertex);
	HRESULT GetJoint (PCSTR pcszJoint, __deref_out JOINT** ppJoint);
	BOOL HasJoint (PCSTR pcszJoint);
	HRESULT BuildMirrorFrom (CSIFMeshData* pMesh, TNamedMapA<PCSTR>& mapNodeMirrors);
	HRESULT FindFaceFromPoint (const FPOINT* ppt, ULONG* pidFace);
	HRESULT FindFaceFromVertices (__in_ecount(cVertics) const FPOINT* pcrgVertices, INT cVertices, ULONG* pidFace);

	HRESULT SaveToStream (ISequentialStream* pStream);
	HRESULT LoadFromStream (ISequentialStream* pStream);

	HRESULT AddVirtualPoint (const FPOINT& fpt, ULONG& idVertex);
	VOID RemoveVirtualVertices (VOID);

	static BOOL IsEquivalentFace (ULONG* prgFaceA, ULONG* prgFaceB);

public:
	template <typename T>
	static ULONG TGetFreeItemID (TMap<ULONG, T>& map)
	{
		ULONG idStart = static_cast<ULONG>(map.Length() + 1);
		if(idStart > MAX_VIRTUAL_VERTEX)
			idStart -= MAX_VIRTUAL_VERTEX;
		while(map.HasItem(idStart))
			idStart++;
		return idStart;
	}
};

class CSIFFrame
{
public:
	WORD m_yOffset;
	WORD m_cTicks;
	FPOINT m_fRootRotation;
	TNamedMapA<FPOINT> m_mapJoints;

public:
	CSIFFrame ();
	~CSIFFrame ();

	HRESULT SaveToStream (ISequentialStream* pStream);
	HRESULT LoadFromStream (ISequentialStream* pStream);
};

class CSIFAnimation
{
public:
	TArray<CSIFFrame*> m_aFrames;

public:
	CSIFAnimation ();
	~CSIFAnimation ();

	HRESULT SaveToStream (ISequentialStream* pStream);
	HRESULT LoadFromStream (ISequentialStream* pStream);
};

class CSIFModel
{
protected:
	CModelTemplate* m_pTemplate;
	bool m_fOwnTemplate;

	TNamedMapA<CSIFMeshData*> m_mapMeshData;
	TNamedMapA<CSIFAnimation*> m_mapAnimations;

	ISimbeyInterchangeFile* m_pSIF;
	UINT m_nTexture;

public:
	WORD m_wScaleNumerator;
	WORD m_wScaleDenominator;
	FPOINT m_fRootRotation;

public:
	CSIFModel (CModelTemplate* pTemplate, bool fOwnTemplate);
	~CSIFModel ();

	HRESULT GetMeshData (PCSTR pcszMesh, __deref_out CSIFMeshData** ppMeshData);
	BOOL HasMeshData (PCSTR pcszMesh);
	HRESULT GetMeshByIndex (sysint idxMesh, __out PCSTR* ppcszMesh, __deref_out CSIFMeshData** ppMeshData);
	CModelTemplate* GetTemplate (VOID) { return m_pTemplate; }

	LONG Animations (VOID) { return static_cast<LONG>(m_mapAnimations.Length()); }
	HRESULT GetAnimation (PCSTR pcszAnimation, __deref_out CSIFAnimation** ppAnimation);
	BOOL HasAnimation (PCSTR pcszAnimation);
	HRESULT GetAnimationByIndex (sysint idxAnimation, __out PCSTR* ppcszAnimation, __deref_out CSIFAnimation** ppAnimation);
	HRESULT AddAnimation (PCSTR pcszAnimation, __out sysint* pidxAnimation);
	HRESULT AddFrameToAnimation (CSIFAnimation* pAnimation, __deref_out CSIFFrame** ppFrame);

	ISimbeyInterchangeFile* GetSIF (VOID);
	VOID SetSIF (ISimbeyInterchangeFile* pSIF);
	VOID InvalidateTexture (VOID);
	HRESULT GetTexture (__out UINT* pnTexture);

	HRESULT SaveToStream (ISequentialStream* pStream);
	HRESULT LoadFromStream (ISequentialStream* pStream);
};
