#include "ListViewObjects.h"
#include "TreeObjects.h"
#include "Published\SimbeyCore.h"

namespace AQVM
{
	static HRESULT PruneListObjects(CInstanceLink* cache, BOOL all);
	interface __declspec(uuid("E417DF92-00ED-4318-9C8F-DF7309AA1141")) IListObject : IUnknown
	{
		STDMETHOD(GetValue)(IUnknown** value) = 0;
		STDMETHOD_(VOID, DetachCache)(VOID) = 0;
	};
	interface __declspec(uuid("A164EE17-BEE4-43EA-841B-D4F7EA829F83")) IListPayload : IUnknown
	{
		STDMETHOD(Copy)(QuadooVM::QVARIANT* value) = 0;
	};
	interface __declspec(uuid("A164EE17-BEE4-43EA-841B-D4F7EA829F84")) INativePayload : IUnknown
	{
		STDMETHOD(GetValue)(IUnknown** value) = 0;
	};
	static QuadooVM::QVARIANT* Arg(QuadooVM::QVPARAMS* args, UINT index)
	{
		return args->pqvArgs + args->cArgs - index - 1;
	}
	static HRESULT Number(const QuadooVM::QVARIANT* value, INT minimum, INT* number)
	{
		HRESULT hr;
		LONGLONG integer;
		Check(VariantInteger(value,&integer));
		CheckIf(integer < minimum || integer > INT_MAX,DISP_E_OVERFLOW);
		*number = static_cast<INT>(integer);
	Cleanup:
		return hr;
	}
	class CListPayload : public CBaseUnknown, public IListPayload
	{
		QuadooVM::QVARIANT m_value;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IListPayload)
		END_UNK_MAP
		CListPayload(const QuadooVM::QVARIANT* value)
		{
			QVMSetVariant(&m_value,value);
		}
		~CListPayload()
		{
			QVMClearVariant(&m_value);
		}
		STDMETHOD(Copy)(QuadooVM::QVARIANT* value)
		{
			QVMSetVariant(value,&m_value);
			return S_OK;
		}
	};
	class CNativePayload : public CBaseUnknown, public CQuadooObjectImpl, public INativePayload
	{
		TStackRef<IUnknown> m_value;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
			UNK_INTERFACE(INativePayload)
		END_UNK_MAP
		CNativePayload(IUnknown* value) : m_value(value)
		{
		}
		STDMETHOD(GetValue)(IUnknown** value)
		{
			HRESULT hr = S_OK;
			CheckIf(!value,E_POINTER);
			*value = m_value;
			(*value)->AddRef();
		Cleanup:
			return hr;
		}
	};
	static HRESULT CopyStrings(QuadooVM::QVARIANT* value, IRStringArray** result)
	{
		HRESULT hr;
		TStackRef<IQuadooArray> array;
		TStackRef<IRStringArray> strings;
		QuadooVM::QVARIANT cell = {QuadooVM::Null};
		*result = NULL;
		if(value->eType == QuadooVM::Array)
		{
			array = value->pArray;
		}
		else
		{
			CheckIf(value->eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
			Check(value->pObject->QueryInterface(&array));
		}
		CheckIf(array->Length() > 4096,E_INVALIDARG);
		Check(ScCreateRStringArray(&strings));
		for(sysint n = 0; n < array->Length(); n++)
		{
			Check(array->GetItem(n,&cell));
			CheckIf(cell.eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
			Check(strings->AddRString(cell.rstrVal));
			QVMClearVariant(&cell);
		}
		*result = strings.Detach();
	Cleanup:
		QVMClearVariant(&cell);
		return hr;
	}
	class CListStrings : public CBaseUnknown, public CQuadooObjectImpl
	{
		TStackRef<IQuadooListItem> m_item;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
		END_UNK_MAP
		CListStrings(IQuadooListItem* item) : m_item(item)
		{
		}
		STDMETHOD(GetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			TStackRef<IRStringArray> strings;
			CheckIf(!name || !result,E_POINTER);
			CheckIf(lstrcmpW(RStrToWide(name),L"Count"),DISP_E_MEMBERNOTFOUND);
			Check(m_item->GetSubItems(&strings));
			result->eType = QuadooVM::I4;
			result->lVal = static_cast<INT>(strings->Length());
		Cleanup:
			return hr;
		}
		STDMETHOD(GetIndexedProperty)(IQuadooVM*, RSTRING, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			INT number;
			Check(Number(index,0,&number));
			Check(m_item->GetSubItem(number,&result->rstrVal));
			result->eType = QuadooVM::String;
		Cleanup:
			return hr;
		}
		STDMETHOD(SetIndexedProperty)(IQuadooVM*, RSTRING, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* value)
		{
			HRESULT hr;
			INT number;
			Check(Number(index,0,&number));
			CheckIf(value->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
			Check(m_item->SetSubItem(number,value->rstrVal));
		Cleanup:
			return hr;
		}
		STDMETHOD(Invoke)(IQuadooVM*, RSTRING method, QuadooVM::QVPARAMS* args, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			TStackRef<IRStringArray> strings;
			INT index = 0;
			PCWSTR name = RStrToWide(method);
			CheckIf(!name || !args || !result,E_POINTER);
			result->eType = QuadooVM::Null;
			Check(m_item->GetSubItems(&strings));
			if(!lstrcmpW(name,L"GetAt") || !lstrcmpW(name,L"GetItem"))
			{
				CheckIf(args->cArgs != 1,DISP_E_BADPARAMCOUNT);
				Check(Number(Arg(args,0),0,&index));
				Check(m_item->GetSubItem(index,&result->rstrVal));
				result->eType = QuadooVM::String;
				goto Cleanup;
			}
			if(!lstrcmpW(name,L"Clear"))
			{
				CheckIf(args->cArgs,DISP_E_BADPARAMCOUNT);
				strings->RemoveAll();
			}
			else if(!lstrcmpW(name,L"Delete"))
			{
				CheckIf(args->cArgs != 1,DISP_E_BADPARAMCOUNT);
				Check(Number(Arg(args,0),0,&index));
				Check(strings->RemoveString(index,NULL));
			}
			else if(!lstrcmpW(name,L"Add") || !lstrcmpW(name,L"Insert") || !lstrcmpW(name,L"Replace"))
			{
				BOOL add = !lstrcmpW(name,L"Add");
				CheckIf(args->cArgs != (add ? 1 : 2),DISP_E_BADPARAMCOUNT);
				QuadooVM::QVARIANT* text = Arg(args,add ? 0 : 1);
				CheckIf(text->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				if(add)
				{
					index = static_cast<INT>(strings->Length());
					Check(strings->AddRString(text->rstrVal));
					result->eType = QuadooVM::I4;
					result->lVal = index;
				}
				else
				{
					Check(Number(Arg(args,0),0,&index));
					if(!lstrcmpW(name,L"Insert"))
					{
						Check(strings->InsertAt(text->rstrVal,index));
					}
					else
					{
						Check(strings->Replace(index,text->rstrVal));
					}
				}
			}
			else
			{
				Check(DISP_E_MEMBERNOTFOUND);
			}
			Check(m_item->SetSubItems(strings));
		Cleanup:
			return hr;
		}
	};
	class CListObject : public CBaseUnknown, public CQuadooObjectImpl, public IListObject
	{
	public:
		TStackRef<IQuadooListItem> item;
		TStackRef<IQuadooListColumn> column;
		TStackRef<IQuadooListItems> items;
		TStackRef<IQuadooListColumns> columns;
		CInstanceLink* cache;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
			UNK_INTERFACE(IListObject)
		END_UNK_MAP
		CListObject(CInstanceLink* link) : cache(link)
		{
		}
		STDMETHOD_(VOID, DetachCache)(VOID)
		{
			cache = NULL;
		}
		STDMETHOD(GetValue)(IUnknown** value)
		{
			HRESULT hr = S_OK;
			CheckIf(!value,E_POINTER);
			*value = item ? static_cast<IUnknown*>(item) : static_cast<IUnknown*>(column);
			(*value)->AddRef();
		Cleanup:
			return hr;
		}
		STDMETHOD(GetProperty)(IQuadooVM*, RSTRING property, QuadooVM::QVARIANT* result)
		{
			HRESULT hr = S_OK;
			PCWSTR name = RStrToWide(property);
			CheckIf(!name || !result,E_POINTER);
			result->eType = QuadooVM::Null;
			if(!lstrcmpW(name,L"Caption"))
			{
				Check(item ? item->GetCaption(&result->rstrVal) : column->GetCaption(&result->rstrVal));
				result->eType = QuadooVM::String;
			}
			else if(!lstrcmpW(name,L"ID"))
			{
				GUID id;
				WCHAR text[40];
				Check(item ? item->GetID(&id) : column->GetID(&id));
				INT length = StringFromGUID2(id,text,ARRAYSIZE(text));
				CheckIf(!length,E_FAIL);
				Check(RStrCreateW(length - 1,text,&result->rstrVal));
				result->eType = QuadooVM::String;
			}
			else if(!lstrcmpW(name,L"SubItems") && item)
			{
				result->pObject = __new CListStrings(item);
				CheckAlloc(result->pObject);
				result->eType = QuadooVM::Object;
			}
			else if(!lstrcmpW(name,L"Data") && item)
			{
				TStackRef<IUnknown> data;
				TStackRef<IListPayload> payload;
				Check(item->GetData(&data));
				if(data)
				{
					hr = data->QueryInterface(&payload);
					CheckIf(FAILED(hr) && hr != E_NOINTERFACE,hr);
					if(payload)
					{
						Check(payload->Copy(result));
					}
					else
					{
						result->pObject = __new CNativePayload(data);
						CheckAlloc(result->pObject);
						result->eType = QuadooVM::Object;
						hr = S_OK;
					}
				}
			}
			else if(!lstrcmpW(name,L"Attached") || (item && (!lstrcmpW(name,L"Checked") ||
				!lstrcmpW(name,L"Selected") || !lstrcmpW(name,L"Focused") || !lstrcmpW(name,L"IsVirtual"))))
			{
				BOOL value;
				if(!lstrcmpW(name,L"Attached"))
				{
					Check(item ? item->GetAttached(&value) : column->GetAttached(&value));
				}
				else if(!lstrcmpW(name,L"Checked"))
				{
					Check(item->GetChecked(&value));
				}
				else if(!lstrcmpW(name,L"Selected"))
				{
					Check(item->GetSelected(&value));
				}
				else if(!lstrcmpW(name,L"Focused"))
				{
					Check(item->GetFocused(&value));
				}
				else
				{
					Check(item->GetIsVirtual(&value));
				}
				result->eType = QuadooVM::Bool;
				result->fVal = value != FALSE;
			}
			else if(item && !lstrcmpW(name,L"DataGeneration"))
			{
				ULONGLONG value;
				Check(item->GetDataGeneration(&value));
				QVMSetQWord(result,value);
			}
			else
			{
				INT value = 0;
				UINT index;
				if(item)
				{
					if(!lstrcmpW(name,L"ItemIndex") || !lstrcmpW(name,L"Index"))
					{
						Check(item->GetItemIndex(&index));
						value = index;
					}
					else if(!lstrcmpW(name,L"ImageIndex"))
					{
						Check(item->GetImageIndex(&value));
					}
					else if(!lstrcmpW(name,L"StateIndex"))
					{
						Check(item->GetStateIndex(&value));
					}
					else
					{
						Check(DISP_E_MEMBERNOTFOUND);
					}
				}
				else if(!lstrcmpW(name,L"Index") || !lstrcmpW(name,L"DisplayIndex"))
				{
					Check(!lstrcmpW(name,L"Index") ? column->GetIndex(&index) : column->GetDisplayIndex(&index));
					value = index;
				}
				else if(!lstrcmpW(name,L"Width"))
				{
					Check(column->GetWidth(&value));
				}
				else if(!lstrcmpW(name,L"Alignment"))
				{
					QuadooForm::ListColumnAlignment alignment;
					Check(column->GetAlignment(&alignment));
					value = alignment;
				}
				else
				{
					Check(DISP_E_MEMBERNOTFOUND);
				}
				result->eType = QuadooVM::I4;
				result->lVal = value;
			}
		Cleanup:
			return hr;
		}
		STDMETHOD(SetProperty)(IQuadooVM*, RSTRING property, QuadooVM::QVARIANT* value)
		{
			HRESULT hr;
			PCWSTR name = RStrToWide(property);
			INT number;
			CheckIf(!name || !value,E_POINTER);
			if(!lstrcmpW(name,L"Caption"))
			{
				CheckIf(value->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				Check(item ? item->SetCaption(value->rstrVal) : column->SetCaption(value->rstrVal));
			}
			else if(item && !lstrcmpW(name,L"SubItems"))
			{
				TStackRef<IRStringArray> strings;
				Check(CopyStrings(value,&strings));
				Check(item->SetSubItems(strings));
			}
			else if(item && !lstrcmpW(name,L"Data"))
			{
				TStackRef<IUnknown> data;
				TStackRef<INativePayload> native;
				if(value->eType == QuadooVM::Object)
				{
					hr = value->pObject->QueryInterface(&native);
					CheckIf(FAILED(hr) && hr != E_NOINTERFACE,hr);
				}
				if(native)
				{
					Check(native->GetValue(&data));
				}
				else if(value->eType != QuadooVM::Null)
				{
					data.Attach(static_cast<IListPayload*>(__new CListPayload(value)));
					CheckAlloc(data);
				}
				Check(item->SetData(data));
			}
			else if(item && (!lstrcmpW(name,L"Checked") || !lstrcmpW(name,L"Selected") || !lstrcmpW(name,L"Focused")))
			{
				CheckIf(value->eType != QuadooVM::Bool,DISP_E_TYPEMISMATCH);
				if(!lstrcmpW(name,L"Checked"))
				{
					Check(item->SetChecked(value->fVal));
				}
				else if(!lstrcmpW(name,L"Selected"))
				{
					Check(item->SetSelected(value->fVal));
				}
				else
				{
					Check(item->SetFocused(value->fVal));
				}
			}
			else
			{
				Check(Number(value,item ? -1 : 0,&number));
				if(item && !lstrcmpW(name,L"ImageIndex"))
				{
					Check(item->SetImageIndex(number));
				}
				else if(item && !lstrcmpW(name,L"StateIndex"))
				{
					Check(item->SetStateIndex(number));
				}
				else if(column && !lstrcmpW(name,L"Width"))
				{
					Check(column->SetWidth(number));
				}
				else if(column && !lstrcmpW(name,L"DisplayIndex"))
				{
					Check(column->SetDisplayIndex(number));
				}
				else if(column && !lstrcmpW(name,L"Alignment"))
				{
					Check(column->SetAlignment(static_cast<QuadooForm::ListColumnAlignment>(number)));
				}
				else
				{
					Check(DISP_E_MEMBERNOTFOUND);
				}
			}
		Cleanup:
			return hr;
		}
		STDMETHOD(GetIndexedProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			INT number;
			QuadooVM::QVARIANT data = {QuadooVM::Null};
			if(item && name && !lstrcmpW(RStrToWide(name),L"Data"))
			{
				Check(GetProperty(vm,name,&data));
				if(data.eType == QuadooVM::Array)
				{
					Check(Number(index,0,&number));
					Check(data.pArray->GetItem(number,result));
				}
				else
				{
					CheckIf(data.eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
					Check(data.pObject->GetIndexedProperty(vm,NULL,index,result));
				}
				goto Cleanup;
			}
			CheckIf(!item || !name || lstrcmpW(RStrToWide(name),L"SubItems"),DISP_E_MEMBERNOTFOUND);
			Check(Number(index,0,&number));
			Check(item->GetSubItem(number,&result->rstrVal));
			result->eType = QuadooVM::String;
		Cleanup:
			QVMClearVariant(&data);
			return hr;
		}
		STDMETHOD(SetIndexedProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* value)
		{
			HRESULT hr;
			INT number;
			QuadooVM::QVARIANT data = {QuadooVM::Null};
			if(item && name && !lstrcmpW(RStrToWide(name),L"Data"))
			{
				Check(GetProperty(vm,name,&data));
				if(data.eType == QuadooVM::Array)
				{
					Check(Number(index,0,&number));
					Check(data.pArray->SetItem(number,value));
				}
				else
				{
					CheckIf(data.eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
					Check(data.pObject->SetIndexedProperty(vm,NULL,index,value));
				}
				goto Cleanup;
			}
			CheckIf(!item || !name || lstrcmpW(RStrToWide(name),L"SubItems"),DISP_E_MEMBERNOTFOUND);
			Check(Number(index,0,&number));
			CheckIf(value->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
			Check(item->SetSubItem(number,value->rstrVal));
		Cleanup:
			QVMClearVariant(&data);
			return hr;
		}
		STDMETHOD(Invoke)(IQuadooVM*, RSTRING method, QuadooVM::QVPARAMS* args, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			PCWSTR name = RStrToWide(method);
			INT index;
			CheckIf(!name || !args || !result,E_POINTER);
			result->eType = QuadooVM::Null;
			if(!lstrcmpW(name,L"Delete"))
			{
				CheckIf(args->cArgs,DISP_E_BADPARAMCOUNT);
				CheckIf(item ? !items : !columns,QuadooForm::EInvalidState);
				Check(item ? items->Delete(item) : columns->Delete(column));
				Check(PruneListObjects(cache,TRUE));
			}
			else if(!lstrcmpW(name,L"MoveTo"))
			{
				CheckIf(args->cArgs != 1,DISP_E_BADPARAMCOUNT);
				Check(Number(Arg(args,0),0,&index));
				CheckIf(item ? !items : !columns,QuadooForm::EInvalidState);
				Check(item ? items->Move(item,index) : columns->Move(column,index));
			}
			else if(column && !lstrcmpW(name,L"AutoSize"))
			{
				CheckIf(args->cArgs != 1,DISP_E_BADPARAMCOUNT);
				Check(Number(Arg(args,0),0,&index));
				Check(column->AutoSize(static_cast<QuadooForm::ListColumnAutoSize>(index)));
			}
			else
			{
				Check(DISP_E_MEMBERNOTFOUND);
			}
		Cleanup:
			return hr;
		}
	};
	class CListCollection : public CBaseUnknown, public CQuadooObjectImpl
	{
		TStackRef<IQuadooListItems> m_items;
		TStackRef<IQuadooListColumns> m_columns;
		TStackRef<CInstanceLink> m_cache;
		HRESULT Count(UINT* count)
		{
			return m_items ? m_items->GetCount(count) : m_columns->GetCount(count);
		}
		HRESULT At(UINT index, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			if(m_items)
			{
				TStackRef<IQuadooListItem> item;
				Check(m_items->GetAt(index,&item));
				Check(WrapListItem(item,m_items,result,m_cache));
			}
			else
			{
				TStackRef<IQuadooListColumn> column;
				Check(m_columns->GetAt(index,&column));
				Check(WrapListColumn(column,m_columns,result,m_cache));
			}
		Cleanup:
			return hr;
		}
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
		END_UNK_MAP
		CListCollection(IQuadooListItems* items, IQuadooListColumns* columns, CInstanceLink* cache) :
			m_items(items), m_columns(columns), m_cache(cache)
		{
		}
		STDMETHOD(GetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			UINT count;
			CheckIf(!name || !result,E_POINTER);
			CheckIf(lstrcmpW(RStrToWide(name),L"Count"),DISP_E_MEMBERNOTFOUND);
			Check(Count(&count));
			result->eType = QuadooVM::I4;
			result->lVal = count;
		Cleanup:
			return hr;
		}
		STDMETHOD(SetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* value)
		{
			HRESULT hr;
			INT count;
			CheckIf(!m_items || !name || lstrcmpW(RStrToWide(name),L"Count"),DISP_E_MEMBERNOTFOUND);
			Check(Number(value,0,&count));
			Check(m_items->SetCount(count));
		Cleanup:
			return hr;
		}
		STDMETHOD(GetIndexedProperty)(IQuadooVM*, RSTRING, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			INT number;
			Check(Number(index,0,&number));
			Check(At(number,result));
		Cleanup:
			return hr;
		}
		STDMETHOD(Invoke)(IQuadooVM*, RSTRING method, QuadooVM::QVPARAMS* args, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			PCWSTR name = RStrToWide(method);
			TStackRef<IQuadooListItem> item;
			TStackRef<IQuadooListColumn> column;
			INT index = 0;
			CheckIf(!name || !args || !result,E_POINTER);
			result->eType = QuadooVM::Null;
			if(!lstrcmpW(name,L"GetAt") || !lstrcmpW(name,L"GetItem"))
			{
				CheckIf(args->cArgs != 1,DISP_E_BADPARAMCOUNT);
				Check(Number(Arg(args,0),0,&index));
				Check(At(index,result));
				goto Cleanup;
			}
			if(!lstrcmpW(name,L"Add") || !lstrcmpW(name,L"Insert"))
			{
				BOOL add = !lstrcmpW(name,L"Add");
				CheckIf(args->cArgs != (add ? 1 : 2),DISP_E_BADPARAMCOUNT);
				QuadooVM::QVARIANT* text = Arg(args,add ? 0 : 1);
				CheckIf(text->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				if(add)
				{
					UINT count;
					Check(Count(&count));
					index = count;
				}
				else
				{
					Check(Number(Arg(args,0),0,&index));
				}
				if(m_items)
				{
					Check(m_items->Insert(index,text->rstrVal,&item));
				}
				else
				{
					Check(m_columns->Insert(index,text->rstrVal,&column));
				}
			}
			else if(!lstrcmpW(name,L"Find"))
			{
				GUID id;
				TStackRef<IJSONValue> value;
				CheckIf(args->cArgs != 1,DISP_E_BADPARAMCOUNT);
				CheckIf(Arg(args,0)->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				Check(JSONCreateString(Arg(args,0)->rstrVal,&value));
				Check(value->GetStringAsGuid(id));
				Check(m_items ? m_items->Find(id,&item) : m_columns->Find(id,&column));
			}
			else if(!lstrcmpW(name,L"Clear"))
			{
				CheckIf(args->cArgs,DISP_E_BADPARAMCOUNT);
				Check(m_items ? m_items->Clear() : m_columns->Clear());
				Check(PruneListObjects(m_cache,TRUE));
				goto Cleanup;
			}
			else if(!lstrcmpW(name,L"Delete") || !lstrcmpW(name,L"Move"))
			{
				BOOL move = !lstrcmpW(name,L"Move");
				CheckIf(args->cArgs != (move ? 2 : 1),DISP_E_BADPARAMCOUNT);
				QuadooVM::QVARIANT* value = Arg(args,0);
				if(value->eType == QuadooVM::Object)
				{
					TStackRef<IListObject> wrapper;
					TStackRef<IUnknown> object;
					Check(value->pObject->QueryInterface(&wrapper));
					Check(wrapper->GetValue(&object));
					Check(m_items ? object->QueryInterface(&item) : object->QueryInterface(&column));
				}
				else
				{
					Check(Number(value,0,&index));
					Check(m_items ? m_items->GetAt(index,&item) : m_columns->GetAt(index,&column));
				}
				if(move)
				{
					Check(Number(Arg(args,1),0,&index));
					Check(m_items ? m_items->Move(item,index) : m_columns->Move(column,index));
				}
				else
				{
					Check(m_items ? m_items->Delete(item) : m_columns->Delete(column));
					Check(PruneListObjects(m_cache,TRUE));
				}
				goto Cleanup;
			}
			else
			{
				Check(DISP_E_MEMBERNOTFOUND);
			}
			Check(m_items ? WrapListItem(item,m_items,result,m_cache) : WrapListColumn(column,m_columns,result,m_cache));
		Cleanup:
			return hr;
		}
	};
	VOID ClearListObjects(CInstanceLink* cache)
	{
		if(cache)
		{
			for(sysint n = 0; n < cache->listObjects.Length(); n++)
			{
				TStackRef<IListObject> object;
				IQuadooObject* value = *cache->listObjects.GetValuePtr(n);
				if(SUCCEEDED(value->QueryInterface(&object)))
				{
					object->DetachCache();
				}
				value->Release();
			}
			cache->listObjects.Clear();
			cache->listCursor = 0;
		}
	}
	static HRESULT Attached(IUnknown* value, BOOL* attached)
	{
		HRESULT hr;
		TStackRef<IQuadooListItem> item;
		TStackRef<IQuadooListColumn> column;
		hr = value->QueryInterface(&item);
		CheckIf(FAILED(hr) && hr != E_NOINTERFACE,hr);
		if(item)
		{
			Check(item->GetAttached(attached));
		}
		else
		{
			Check(value->QueryInterface(&column));
			Check(column->GetAttached(attached));
		}
	Cleanup:
		return hr;
	}
	static HRESULT PruneListObjects(CInstanceLink* cache, BOOL all)
	{
		HRESULT hr = S_OK;
		sysint remaining;
		CheckIfIgnore(!cache || !cache->listObjects.Length(),S_OK);
		remaining = all ? cache->listObjects.Length() : 1;
		while(remaining-- && cache->listObjects.Length())
		{
			TStackRef<IListObject> wrapper;
			TStackRef<IUnknown> value;
			BOOL attached;
			sysint index = cache->listCursor % cache->listObjects.Length();
			IQuadooObject* object = *cache->listObjects.GetValuePtr(index);
			Check(object->QueryInterface(&wrapper));
			Check(wrapper->GetValue(&value));
			Check(Attached(value,&attached));
			if(!attached)
			{
				wrapper->DetachCache();
				Check(cache->listObjects.RemoveByIndex(index,NULL));
				object->Release();
			}
			else
			{
				cache->listCursor++;
			}
		}
	Cleanup:
		return hr;
	}
	static HRESULT WrapListValue(IQuadooListItem* item, IQuadooListColumn* column,
		IQuadooListItems* items, IQuadooListColumns* columns, QuadooVM::QVARIANT* result, CInstanceLink* cache)
	{
		HRESULT hr = S_OK;
		TStackRef<CListObject> created;
		IUnknown* value = item ? static_cast<IUnknown*>(item) : static_cast<IUnknown*>(column);
		BOOL attached;
		result->eType = QuadooVM::Null;
		CheckIfIgnore(!value,S_OK);
		Check(Attached(value,&attached));
		if(!attached)
		{
			cache = NULL;
		}
		else if(item && cache)
		{
			BOOL isVirtual;
			Check(item->GetIsVirtual(&isVirtual));
			if(isVirtual)
			{
				cache = NULL;
			}
		}
		if(cache)
		{
			IQuadooObject* existing = NULL;
			TStackRef<IUnknown> identity;
			Check(value->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identity)));
			Check(PruneListObjects(cache,FALSE));
			if(SUCCEEDED(cache->listObjects.Find(identity,&existing)))
			{
				result->pObject = existing;
				result->pObject->AddRef();
				result->eType = QuadooVM::Object;
				goto Cleanup;
			}
		}
		created.Attach(__new CListObject(cache));
		CheckAlloc(created);
		created->item = item;
		created->column = column;
		created->items = items;
		created->columns = columns;
		if(cache)
		{
			TStackRef<IUnknown> identity;
			Check(value->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identity)));
			Check(cache->listObjects.Add(identity,static_cast<IQuadooObject*>(created)));
			created->AddRef();
		}
		result->pObject = created.Detach();
		result->eType = QuadooVM::Object;
	Cleanup:
		return hr;
	}
	HRESULT WrapListItem(IQuadooListItem* item, IQuadooListItems* items, QuadooVM::QVARIANT* result, CInstanceLink* cache)
	{
		return WrapListValue(item,NULL,items,NULL,result,cache);
	}
	HRESULT WrapListColumn(IQuadooListColumn* column, IQuadooListColumns* columns, QuadooVM::QVARIANT* result, CInstanceLink* cache)
	{
		return WrapListValue(NULL,column,NULL,columns,result,cache);
	}
	HRESULT WrapListCollection(IQuadooListItems* items, IQuadooListColumns* columns, QuadooVM::QVARIANT* result, CInstanceLink* cache)
	{
		HRESULT hr = S_OK;
		result->eType = QuadooVM::Null;
		result->pObject = __new CListCollection(items,columns,cache);
		CheckAlloc(result->pObject);
		result->eType = QuadooVM::Object;
	Cleanup:
		return hr;
	}
	HRESULT UnwrapListItem(const QuadooVM::QVARIANT* value, IQuadooListItem** item)
	{
		HRESULT hr = S_OK;
		TStackRef<IListObject> wrapper;
		TStackRef<IUnknown> object;
		*item = NULL;
		CheckIfIgnore(value->eType == QuadooVM::Null,S_OK);
		CheckIf(value->eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
		Check(value->pObject->QueryInterface(&wrapper));
		Check(wrapper->GetValue(&object));
		Check(object->QueryInterface(item));
	Cleanup:
		return hr;
	}
	HRESULT ListGet(IQuadooListView* view, CInstanceLink* link, RSTRING property, QuadooVM::QVARIANT* result)
	{
		HRESULT hr;
		PCWSTR name = RStrToWide(property);
		TStackRef<IQuadooListItems> items;
		TStackRef<IQuadooListColumns> columns;
		if(!lstrcmpW(name,L"Items"))
		{
			Check(view->GetItems(&items));
			Check(WrapListCollection(items,NULL,result,link));
		}
		else if(!lstrcmpW(name,L"Columns"))
		{
			Check(view->GetColumns(&columns));
			Check(WrapListCollection(NULL,columns,result,link));
		}
		else if(!lstrcmpW(name,L"Selected") || !lstrcmpW(name,L"Focused") || !lstrcmpW(name,L"ItemIndex"))
		{
			TStackRef<IQuadooListItem> item;
			Check(!lstrcmpW(name,L"Focused") ? view->GetFocused(&item) : view->GetSelected(&item));
			if(!lstrcmpW(name,L"ItemIndex"))
			{
				UINT index;
				result->eType = QuadooVM::I4;
				result->lVal = -1;
				if(item)
				{
					Check(item->GetItemIndex(&index));
					result->lVal = index;
				}
			}
			else
			{
				Check(view->GetItems(&items));
				Check(WrapListItem(item,items,result,link));
			}
		}
		else if(!lstrcmpW(name,L"SelectedCount"))
		{
			UINT count;
			Check(view->GetSelectedCount(&count));
			result->eType = QuadooVM::I4;
			result->lVal = count;
		}
		else if(!lstrcmpW(name,L"DataGeneration"))
		{
			ULONGLONG generation;
			Check(view->GetDataGeneration(&generation));
			QVMSetQWord(result,generation);
		}
		else if(!lstrcmpW(name,L"LargeImages") || !lstrcmpW(name,L"SmallImages") || !lstrcmpW(name,L"StateImages"))
		{
			TStackRef<IQuadooComponent> component;
			Check(view->GetImages(!lstrcmpW(name,L"LargeImages") ? QuadooForm::ListLargeImages :
				!lstrcmpW(name,L"SmallImages") ? QuadooForm::ListSmallImages : QuadooForm::ListStateImages,&component));
			if(component)
			{
				CComponentObject* wrapper = NULL;
				if(link && link->instance)
				{
					Check(link->instance->Wrap(component,&result->pObject));
				}
				else
				{
					Check(NewComponentObject(NULL,component,&wrapper));
					result->pObject = wrapper;
				}
				result->eType = QuadooVM::Object;
			}
		}
		else
		{
			hr = DISP_E_MEMBERNOTFOUND;
		}
	Cleanup:
		return hr;
	}
	HRESULT ListSet(IQuadooListView* view, RSTRING property, QuadooVM::QVARIANT* value)
	{
		HRESULT hr;
		TStackRef<IQuadooListItem> item;
		PCWSTR name = RStrToWide(property);
		if(!lstrcmpW(name,L"Selected") || !lstrcmpW(name,L"Focused"))
		{
			Check(UnwrapListItem(value,&item));
			Check(!lstrcmpW(name,L"Selected") ? view->SetSelected(item) : view->SetFocused(item));
		}
		else if(!lstrcmpW(name,L"ItemIndex"))
		{
			INT index;
			TStackRef<IQuadooListItems> items;
			Check(Number(value,-1,&index));
			if(index >= 0)
			{
				Check(view->GetItems(&items));
				Check(items->GetAt(index,&item));
			}
			Check(view->SetSelected(item));
		}
		else if(!lstrcmpW(name,L"LargeImages") || !lstrcmpW(name,L"SmallImages") || !lstrcmpW(name,L"StateImages"))
		{
			TStackRef<IComponentObject> wrapper;
			TStackRef<IQuadooComponent> component;
			if(value->eType != QuadooVM::Null)
			{
				CheckIf(value->eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
				Check(value->pObject->QueryInterface(&wrapper));
				Check(wrapper->GetComponent(&component));
			}
			Check(view->SetImages(!lstrcmpW(name,L"LargeImages") ? QuadooForm::ListLargeImages :
				!lstrcmpW(name,L"SmallImages") ? QuadooForm::ListSmallImages : QuadooForm::ListStateImages,component));
		}
		else
		{
			hr = DISP_E_MEMBERNOTFOUND;
		}
	Cleanup:
		return hr;
	}
	HRESULT ListInvoke(IQuadooListView* view, CInstanceLink* link, RSTRING method, QuadooVM::QVPARAMS* args, QuadooVM::QVARIANT* result)
	{
		HRESULT hr;
		PCWSTR name = RStrToWide(method);
		TStackRef<IQuadooListItem> item;
		INT index;
		if(!lstrcmpW(name,L"BeginUpdate") || !lstrcmpW(name,L"EndUpdate") || !lstrcmpW(name,L"Refresh"))
		{
			CheckIf(args->cArgs,DISP_E_BADPARAMCOUNT);
			if(!lstrcmpW(name,L"BeginUpdate"))
			{
				Check(view->BeginUpdate());
			}
			else if(!lstrcmpW(name,L"EndUpdate"))
			{
				Check(view->EndUpdate());
			}
			else
			{
				Check(view->Refresh());
			}
		}
		else if(!lstrcmpW(name,L"BeginLabelEdit") || !lstrcmpW(name,L"EnsureVisible"))
		{
			CheckIf(args->cArgs != 1,DISP_E_BADPARAMCOUNT);
			Check(UnwrapListItem(Arg(args,0),&item));
			Check(!lstrcmpW(name,L"BeginLabelEdit") ? view->BeginLabelEdit(item) : view->EnsureVisible(item,FALSE));
		}
		else if(!lstrcmpW(name,L"Sort"))
		{
			CheckIf(args->cArgs != 2,DISP_E_BADPARAMCOUNT);
			Check(Number(Arg(args,0),0,&index));
			CheckIf(Arg(args,1)->eType != QuadooVM::Bool,DISP_E_TYPEMISMATCH);
			Check(view->Sort(index,Arg(args,1)->fVal));
		}
		else if(!lstrcmpW(name,L"NextSelected"))
		{
			TStackRef<IQuadooListItems> items;
			CheckIf(args->cArgs != 1,DISP_E_BADPARAMCOUNT);
			Check(Number(Arg(args,0),-1,&index));
			Check(view->GetNextSelected(index,&item));
			Check(view->GetItems(&items));
			Check(WrapListItem(item,items,result,link));
		}
		else if(!lstrcmpW(name,L"SetItemSelected"))
		{
			CheckIf(args->cArgs != 2,DISP_E_BADPARAMCOUNT);
			Check(Number(Arg(args,0),0,&index));
			CheckIf(Arg(args,1)->eType != QuadooVM::Bool,DISP_E_TYPEMISMATCH);
			Check(view->SetItemSelected(index,Arg(args,1)->fVal));
		}
		else if(!lstrcmpW(name,L"ResetVirtualData"))
		{
			CheckIf(args->cArgs != 1,DISP_E_BADPARAMCOUNT);
			Check(Number(Arg(args,0),0,&index));
			Check(view->ResetVirtualData(index));
		}
		else if(!lstrcmpW(name,L"RefreshRange"))
		{
			INT last;
			CheckIf(args->cArgs != 2,DISP_E_BADPARAMCOUNT);
			Check(Number(Arg(args,0),0,&index));
			Check(Number(Arg(args,1),0,&last));
			Check(view->RefreshRange(index,last));
		}
		else if(!lstrcmpW(name,L"UpdateVirtualText"))
		{
			INT column;
			CheckIf(args->cArgs != 3,DISP_E_BADPARAMCOUNT);
			Check(Number(Arg(args,0),0,&index));
			Check(Number(Arg(args,1),0,&column));
			CheckIf(Arg(args,2)->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
			Check(view->UpdateVirtualText(index,column,Arg(args,2)->rstrVal));
		}
		else if(!lstrcmpW(name,L"UpdateVirtualChecked"))
		{
			CheckIf(args->cArgs != 2,DISP_E_BADPARAMCOUNT);
			Check(Number(Arg(args,0),0,&index));
			CheckIf(Arg(args,1)->eType != QuadooVM::Bool,DISP_E_TYPEMISMATCH);
			Check(view->UpdateVirtualChecked(index,Arg(args,1)->fVal));
		}
		else
		{
			hr = DISP_E_MEMBERNOTFOUND;
		}
	Cleanup:
		return hr;
	}
	HRESULT WrapValueObject(IUnknown* object, IQuadooComponent* sender, CInstanceLink* cache, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = S_OK;
		TStackRef<IQuadooListItem> item;
		TStackRef<IQuadooListColumn> column;
		TStackRef<IQuadooTreeNode> node;
		TStackRef<IQuadooListView> list;
		TStackRef<IQuadooTreeView> tree;
		TStackRef<IQuadooListItems> items;
		TStackRef<IQuadooListColumns> columns;
		TStackRef<IQuadooTreeNodes> nodes;
		result->eType = QuadooVM::Null;
		CheckIfIgnore(!object,S_OK);
		hr = object->QueryInterface(&item);
		CheckIf(FAILED(hr) && hr != E_NOINTERFACE,hr);
		if(item)
		{
			if(sender && SUCCEEDED(sender->QueryInterface(&list)))
			{
				hr = list->GetItems(&items);
				CheckIf(FAILED(hr) && hr != QuadooForm::EDestroyed,hr);
			}
			Check(WrapListItem(item,items,result,cache));
			goto Cleanup;
		}
		hr = object->QueryInterface(&column);
		CheckIf(FAILED(hr) && hr != E_NOINTERFACE,hr);
		if(column)
		{
			if(sender && SUCCEEDED(sender->QueryInterface(&list)))
			{
				hr = list->GetColumns(&columns);
				CheckIf(FAILED(hr) && hr != QuadooForm::EDestroyed,hr);
			}
			Check(WrapListColumn(column,columns,result,cache));
			goto Cleanup;
		}
		Check(object->QueryInterface(&node));
		if(sender && SUCCEEDED(sender->QueryInterface(&tree)))
		{
			hr = tree->GetNodes(&nodes);
			CheckIf(FAILED(hr) && hr != QuadooForm::EDestroyed,hr);
		}
		Check(WrapTreeNode(node,nodes,result,cache));
	Cleanup:
		return hr;
	}
}
