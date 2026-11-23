#pragma once
#include "RuntimeObjects.h"

namespace AQVM
{
	HRESULT WrapListItem(IQuadooListItem* item, IQuadooListItems* items, QuadooVM::QVARIANT* result, CInstanceLink* cache);
	HRESULT WrapListColumn(IQuadooListColumn* column, IQuadooListColumns* columns, QuadooVM::QVARIANT* result, CInstanceLink* cache);
	HRESULT WrapListCollection(IQuadooListItems* items, IQuadooListColumns* columns, QuadooVM::QVARIANT* result, CInstanceLink* cache);
	HRESULT UnwrapListItem(const QuadooVM::QVARIANT* value, IQuadooListItem** item);
	VOID ClearListObjects(CInstanceLink* cache);
	HRESULT ListGet(IQuadooListView* view, CInstanceLink* link, RSTRING name, QuadooVM::QVARIANT* result);
	HRESULT ListSet(IQuadooListView* view, RSTRING name, QuadooVM::QVARIANT* value);
	HRESULT ListInvoke(IQuadooListView* view, CInstanceLink* link, RSTRING name, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result);
	HRESULT WrapValueObject(IUnknown* object, IQuadooComponent* sender, CInstanceLink* cache, QuadooVM::QVARIANT* result);
}
