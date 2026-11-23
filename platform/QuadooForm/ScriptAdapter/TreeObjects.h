#pragma once
#include "RuntimeObjects.h"

namespace AQVM
{
	HRESULT WrapTreeNode(IQuadooTreeNode* node, IQuadooTreeNodes* nodes, QuadooVM::QVARIANT* result, CInstanceLink* cache = NULL);
	HRESULT WrapTreeNodes(IQuadooTreeNodes* nodes, IQuadooTreeNode* parent, QuadooVM::QVARIANT* result, CInstanceLink* cache = NULL);
	VOID ClearTreeObjects(CInstanceLink* cache);
	HRESULT UnwrapTreeNode(const QuadooVM::QVARIANT* value, IQuadooTreeNode** node);
}
