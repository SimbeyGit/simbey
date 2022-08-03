#pragma once

#include "Library\Core\Map.h"

class CModelMesh;
class CModelNode;

#define	MESH_REQUIRED				1

struct MIRRORLIST
{
	CModelNode* pNode;
	PCSTR pcszMirror;
};

HRESULT LoadString (ISequentialStream* pStream, PSTR* ppszString, INT* pcchString);
HRESULT WriteString (ISequentialStream* pStream, PCSTR pcszString);

class CModelMesh
{
protected:
	CModelNode* m_pParent;

	PSTR m_pszRelativeName;
	PSTR m_pszFullName;

	TArray<CModelNode*> m_aNodes;
	USHORT m_usFlags;

public:
	CModelMesh (CModelNode* pParent, USHORT usFlags);
	~CModelMesh ();

	CModelNode* GetParent (VOID);
	inline USHORT GetFlags (VOID) { return m_usFlags; }

	HRESULT SetRelativeName (PCSTR pcszName);
	PCSTR GetRelativeName (VOID);

	HRESULT SetFullName (PCSTR pcszName);
	PCSTR GetFullName (VOID);

	HRESULT AddNode (CModelNode* pNode);
	HRESULT FindNode (PCSTR pcszNode, __deref_out CModelNode** ppNode);
	VOID GetNodeArray (CModelNode*** pprgNodes, __out sysint* pcNodes);

	BOOL HasNodes (VOID);
	PCSTR GetJointName (LONG nJoint);
};

class CModelNode
{
protected:
	CModelMesh* m_pParent;

	PSTR m_pszRelativeName;
	TArray<CModelMesh*> m_aMeshes;

	CModelNode* m_pMirrorNode;

public:
	CModelNode (CModelMesh* pParent);
	~CModelNode ();

	CModelMesh* GetParent (VOID);

	HRESULT SetRelativeName (PCSTR pcszName);
	PCSTR GetRelativeName (VOID);

	HRESULT AddMesh (CModelMesh* pMesh);
	HRESULT FindMesh (PCSTR pcszMesh, __deref_out CModelMesh** ppMesh);
	VOID GetMeshArray (CModelMesh*** pprgMeshes, __out sysint* pcMeshes);

	CModelNode* GetMirrorNode (VOID);
	VOID SetMirrorNode (CModelNode* pMirrorNode);
};

class CModelTemplate
{
protected:
	CModelNode* m_pRoot;
	TNamedMapA<CModelMesh*> m_mapMeshes;

public:
	CModelTemplate ();
	~CModelTemplate ();

	HRESULT SaveToStream (ISequentialStream* pStream);

	HRESULT LoadFromStream (ISequentialStream* pStream);
	TNamedMapA<CModelMesh*>* GetModelMeshes (VOID);

	HRESULT GetMesh (INT nMesh, __deref_out CModelMesh** ppMesh);

protected:
	HRESULT SaveNode (ISequentialStream* pStream, CModelNode* pNode);
	HRESULT SaveMesh (ISequentialStream* pStream, CModelMesh* pMesh);

	HRESULT ResolveMirrors (TArray<MIRRORLIST>& aMirrors);

	HRESULT LoadNode (ISequentialStream* pStream, CModelMesh* pParent, TArray<MIRRORLIST>& aMirrors, CModelNode** ppNode);
	HRESULT LoadMesh (ISequentialStream* pStream, CModelNode* pParent, TArray<MIRRORLIST>& aMirrors, CModelMesh** ppMesh);

	HRESULT AddMeshWithPath (CModelMesh* pMesh, PCSTR pcszName);
};
