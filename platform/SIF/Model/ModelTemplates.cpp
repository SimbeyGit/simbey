#include <windows.h>
#include "Library\Core\CoreDefs.h"
#include "Library\Core\StringCore.h"
#include "Library\Core\MemoryStream.h"
#include "ModelTemplates.h"

#ifndef	USHORT_MAX
	#define	USHORT_MAX	65535
#endif

HRESULT LoadString (ISequentialStream* pStream, PSTR* ppszString, INT* pcchString)
{
	HRESULT hr;
	USHORT cch;
	ULONG cb;
	PSTR pszString = NULL;

	Check(pStream->Read(&cch, sizeof(cch), &cb));
	pszString = __new CHAR[cch + 1];
	CheckAlloc(pszString);
	Check(pStream->Read(pszString, cch, &cb));
	pszString[cch] = '\0';

	*ppszString = pszString; pszString = NULL;
	*pcchString = cch;

Cleanup:
	__delete_array pszString;
	return hr;
}

HRESULT WriteString (ISequentialStream* pStream, PCSTR pcszString)
{
	HRESULT hr;
	USHORT cch = TStrLenChecked(pcszString);
	ULONG cb;

	Check(pStream->Write(&cch, sizeof(cch), &cb));
	if(0 < cch)
		Check(pStream->Write(pcszString, cch, &cb));

Cleanup:
	return hr;
}

// CModelMesh

CModelMesh::CModelMesh (CModelNode* pParent, USHORT usFlags) :
	m_pParent(pParent),
	m_usFlags(usFlags),
	m_pszRelativeName(NULL),
	m_pszFullName(NULL)
{
}

CModelMesh::~CModelMesh ()
{
	if(0 < m_aNodes.Length())
	{
		for(INT i = 0; i < m_aNodes.Length(); i++)
			__delete m_aNodes[i];
		m_aNodes.Clear();
	}
	SafeDeleteArray(m_pszFullName);
	SafeDeleteArray(m_pszRelativeName);
}

CModelNode* CModelMesh::GetParent (VOID)
{
	return m_pParent;
}

HRESULT CModelMesh::SetRelativeName (PCSTR pcszName)
{
	return TReplaceStringAssert(pcszName, &m_pszRelativeName);
}

PCSTR CModelMesh::GetRelativeName (VOID)
{
	return m_pszRelativeName;
}

HRESULT CModelMesh::SetFullName (PCSTR pcszName)
{
	return TReplaceStringAssert(pcszName, &m_pszFullName);
}

PCSTR CModelMesh::GetFullName (VOID)
{
	return m_pszFullName;
}

HRESULT CModelMesh::AddNode (CModelNode* pNode)
{
	return m_aNodes.Append(pNode);
}

HRESULT CModelMesh::FindNode (PCSTR pcszNode, __deref_out CModelNode** ppNode)
{
	HRESULT hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	for(sysint i = 0; i < m_aNodes.Length(); i++)
	{
		if(0 == TStrCmpAssert(pcszNode, m_aNodes[i]->GetRelativeName()))
		{
			*ppNode = m_aNodes[i];
			hr = S_OK;
			break;
		}
	}
	return hr;
}

VOID CModelMesh::GetNodeArray (CModelNode*** pprgNodes, __out sysint* pcNodes)
{
	m_aNodes.GetData(pprgNodes, pcNodes);
}

BOOL CModelMesh::HasNodes (VOID)
{
	return 0 < m_aNodes.Length();
}

PCSTR CModelMesh::GetJointName (LONG nJoint)
{
	return m_aNodes[nJoint]->GetRelativeName();
}

// CModelNode

CModelNode::CModelNode (CModelMesh* pParent) :
	m_pParent(pParent),
	m_pszRelativeName(NULL),
	m_pMirrorNode(NULL)
{
}

CModelNode::~CModelNode ()
{
	if(0 < m_aMeshes.Length())
	{
		for(INT i = 0; i < m_aMeshes.Length(); i++)
			__delete m_aMeshes[i];
		m_aMeshes.Clear();
	}
	SafeDeleteArray(m_pszRelativeName);
}

CModelMesh* CModelNode::GetParent (VOID)
{
	return m_pParent;
}

HRESULT CModelNode::SetRelativeName (PCSTR pcszName)
{
	return TReplaceStringAssert(pcszName, &m_pszRelativeName);
}

PCSTR CModelNode::GetRelativeName (VOID)
{
	return m_pszRelativeName;
}

HRESULT CModelNode::AddMesh (CModelMesh* pMesh)
{
	return m_aMeshes.Append(pMesh);
}

HRESULT CModelNode::FindMesh (PCSTR pcszMesh, __deref_out CModelMesh** ppMesh)
{
	HRESULT hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	for(sysint i = 0; i < m_aMeshes.Length(); i++)
	{
		if(0 == TStrCmpAssert(pcszMesh, m_aMeshes[i]->GetRelativeName()))
		{
			*ppMesh = m_aMeshes[i];
			hr = S_OK;
			break;
		}
	}
	return hr;
}

VOID CModelNode::GetMeshArray (CModelMesh*** pprgMeshes, __out sysint* pcMeshes)
{
	m_aMeshes.GetData(pprgMeshes, pcMeshes);
}

CModelNode* CModelNode::GetMirrorNode (VOID)
{
	return m_pMirrorNode;
}

VOID CModelNode::SetMirrorNode (CModelNode* pMirrorNode)
{
	m_pMirrorNode = pMirrorNode;
}

// CModelTemplate

CModelTemplate::CModelTemplate ()
{
	m_pRoot = NULL;
}

CModelTemplate::~CModelTemplate ()
{
	// All nodes and meshes will be deleted by deleting the root.
	m_mapMeshes.Clear();

	__delete m_pRoot;
}

HRESULT CModelTemplate::SaveToStream (ISequentialStream* pStream)
{
	return SaveNode(pStream, m_pRoot);
}

HRESULT CModelTemplate::LoadFromStream (ISequentialStream* pStream)
{
	HRESULT hr;
	TArray<MIRRORLIST> aMirrors;

	Assert(NULL == m_pRoot);

	Check(LoadNode(pStream, NULL, aMirrors, &m_pRoot));
	Check(ResolveMirrors(aMirrors));

Cleanup:
	// When loading from a stream, the mirror names must be deleted.
	for(sysint i = 0; i < aMirrors.Length(); i++)
		__delete_array const_cast<PSTR>(aMirrors[i].pcszMirror);
	return hr;
}

TNamedMapA<CModelMesh*>* CModelTemplate::GetModelMeshes (VOID)
{
	return &m_mapMeshes;
}

HRESULT CModelTemplate::GetMesh (INT nMesh, __deref_out CModelMesh** ppMesh)
{
	HRESULT hr;

	CheckIf(nMesh >= m_mapMeshes.Length(), E_INVALIDARG);
	*ppMesh = *m_mapMeshes.GetValuePtr(nMesh);
	hr = S_OK;

Cleanup:
	return hr;
}

HRESULT CModelTemplate::SaveNode (ISequentialStream* pStream, CModelNode* pNode)
{
	HRESULT hr;
	CModelNode* pMirror = pNode->GetMirrorNode();
	CModelMesh** prgMeshes;
	sysint cMeshes;
	USHORT usMeshes;
	ULONG cb;

	Check(WriteString(pStream, pNode->GetRelativeName()));

	if(pMirror)
	{
		TArray<PCSTR> aNames;
		CMemoryStream stmMirror;
		static CHAR chNil = '\0';

		while(pMirror)
		{
			CModelMesh* pMesh = pMirror->GetParent();
			Check(aNames.Append(pMirror->GetRelativeName()));
			Check(aNames.Append(pMesh->GetRelativeName()));
			pMirror = pMesh->GetParent();
		}

		for(sysint i = aNames.Length() - 1; i >= 0; i--)
		{
			if(stmMirror.DataRemaining())
				Check(stmMirror.TWrite(SLP("."), &cb));
			Check(stmMirror.TWrite(aNames[i], TStrLenAssert(aNames[i]), &cb));
		}

		Check(stmMirror.TWrite(&chNil, 1, &cb));
		Check(WriteString(pStream, stmMirror.TGetReadPtr<CHAR>()));
	}
	else
		Check(WriteString(pStream, NULL));

	pNode->GetMeshArray(&prgMeshes, &cMeshes);
	CheckIf(cMeshes > USHORT_MAX, E_FAIL);

	usMeshes = static_cast<USHORT>(cMeshes);
	Check(pStream->Write(&usMeshes, sizeof(usMeshes), &cb));

	for(sysint i = 0; i < cMeshes; i++)
		Check(SaveMesh(pStream, prgMeshes[i]));

Cleanup:
	return hr;
}

HRESULT CModelTemplate::SaveMesh (ISequentialStream* pStream, CModelMesh* pMesh)
{
	HRESULT hr;
	USHORT usFlags = pMesh->GetFlags(), usNodes;
	ULONG cb;
	CModelNode** prgNodes;
	sysint cNodes;

	Check(WriteString(pStream, pMesh->GetRelativeName()));
	Check(pStream->Write(&usFlags, sizeof(usFlags), &cb));

	pMesh->GetNodeArray(&prgNodes, &cNodes);
	CheckIf(cNodes > USHORT_MAX, E_FAIL);

	usNodes = static_cast<USHORT>(cNodes);
	Check(pStream->Write(&usNodes, sizeof(usNodes), &cb));

	for(sysint i = 0; i < cNodes; i++)
		Check(SaveNode(pStream, prgNodes[i]));

Cleanup:
	return hr;
}

HRESULT CModelTemplate::ResolveMirrors (TArray<MIRRORLIST>& aMirrors)
{
	HRESULT hr = S_OK;
	TArray<PSTR> aNames;
	PSTR pszCopy = NULL;

	for(sysint i = 0; i < aMirrors.Length(); i++)
	{
		MIRRORLIST& ml = aMirrors[i];
		PCSTR pcszPtr = ml.pcszMirror, pcszStart = pcszPtr;
		CModelNode* pNode = m_pRoot;

		for(;;)
		{
			if('.' == *pcszPtr || '\0' == *pcszPtr)
			{
				INT cch = static_cast<INT>(pcszPtr - pcszStart);
				pszCopy = __new CHAR[cch + 1];
				CheckAlloc(pszCopy);
				CopyMemory(pszCopy, pcszStart, cch);
				pszCopy[cch] = '\0';
				Check(aNames.Append(pszCopy));
				pszCopy = NULL;

				if('\0' == *pcszPtr)
					break;

				pcszStart = pcszPtr + 1;
			}
			pcszPtr++;
		}

		for(sysint n = 1; n < aNames.Length(); n++)
		{
			CModelMesh* pMesh;
			Check(pNode->FindMesh(aNames[n++], &pMesh));
			CheckIf(n == aNames.Length(), E_FAIL);
			Check(pMesh->FindNode(aNames[n], &pNode));
		}

		ml.pNode->SetMirrorNode(pNode);

		for(sysint n = 0; n < aNames.Length(); n++)
			__delete_array aNames[n];

		aNames.Clear();
	}

Cleanup:
	for(sysint n = 0; n < aNames.Length(); n++)
		__delete_array aNames[n];
	SafeDeleteArray(pszCopy);
	return hr;
}

HRESULT CModelTemplate::LoadNode (ISequentialStream* pStream, CModelMesh* pParent, TArray<MIRRORLIST>& aMirrors, CModelNode** ppNode)
{
	HRESULT hr;
	PSTR pszName = NULL;
	PSTR pszMirror = NULL;
	CModelMesh* pMesh = NULL;
	CModelNode* pNode = NULL;
	INT cch;
	USHORT cMeshes;
	ULONG cb;

	Check(LoadString(pStream, &pszName, &cch));

	pNode = __new CModelNode(pParent);
	CheckAlloc(pNode);

	Check(LoadString(pStream, &pszMirror, &cch));
	if(0 < cch)
	{
		MIRRORLIST ml = { pNode, pszMirror };
		Check(aMirrors.Append(ml));
		pszMirror = NULL;
	}

	Check(pNode->SetRelativeName(pszName));

	Check(pStream->Read(&cMeshes, sizeof(cMeshes), &cb));
	for(USHORT i = 0; i < cMeshes; i++)
	{
		Check(LoadMesh(pStream, pNode, aMirrors, &pMesh));
		Check(pNode->AddMesh(pMesh));
		pMesh = NULL;
	}

	*ppNode = pNode;
	pNode = NULL;

Cleanup:
	SafeDelete(pNode);
	SafeDelete(pMesh);
	__delete_array pszMirror;
	__delete_array pszName;
	return hr;
}

HRESULT CModelTemplate::LoadMesh (ISequentialStream* pStream, CModelNode* pParent, TArray<MIRRORLIST>& aMirrors, CModelMesh** ppMesh)
{
	HRESULT hr;
	PSTR pszName = NULL;
	CModelMesh* pMesh = NULL;
	CModelNode* pNode = NULL;
	INT cch;
	USHORT usFlags, cNodes;
	ULONG cb;

	Check(LoadString(pStream, &pszName, &cch));
	Check(pStream->Read(&usFlags, sizeof(usFlags), &cb));

	pMesh = __new CModelMesh(pParent, usFlags);
	CheckAlloc(pMesh);

	Check(pMesh->SetRelativeName(pszName));

	Check(pStream->Read(&cNodes, sizeof(cNodes), &cb));
	for(USHORT i = 0; i < cNodes; i++)
	{
		Check(LoadNode(pStream, pMesh, aMirrors, &pNode));
		Check(pMesh->AddNode(pNode));
		pNode = NULL;
	}

	// Build a list of meshes from the entire template for easy access.
	Check(AddMeshWithPath(pMesh, pszName));

	*ppMesh = pMesh;
	pMesh = NULL;

Cleanup:
	SafeDelete(pNode);
	SafeDelete(pMesh);
	__delete_array pszName;
	return hr;
}

HRESULT CModelTemplate::AddMeshWithPath (CModelMesh* pMesh, PCSTR pcszName)
{
	const CHAR chZero = '\0';

	HRESULT hr;
	CModelMesh* pRootMesh = pMesh;
	TArray<PCSTR> aNames;
	CMemoryStream stmName;
	ULONG cb;

	Check(aNames.Append(pcszName));

	for(;;)
	{
		CModelNode* pNode = pMesh->GetParent();
		CheckIf(NULL == pNode, E_UNEXPECTED);

		Check(aNames.Append(pNode->GetRelativeName()));

		pMesh = pNode->GetParent();
		if(NULL == pMesh)
			break;
	}

	for(sysint i = aNames.Length() - 1; i >= 0; i--)
	{
		Check(stmName.Write(aNames[i], TStrLenAssert(aNames[i]), &cb));
		if(i > 0)
			Check(stmName.Write(".", 1, &cb));
	}

	Check(stmName.Write(&chZero, sizeof(chZero), &cb));
	Check(m_mapMeshes.Add(stmName.TGetReadPtr<CHAR>(), pRootMesh));
	Check(pRootMesh->SetFullName(stmName.TGetReadPtr<CHAR>()));

Cleanup:
	return hr;
}
