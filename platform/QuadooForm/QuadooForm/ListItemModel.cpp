#include "ListView.h"

namespace QF
{
	VOID CListView::ReleaseItems(TArray<CListItem*>& values)
	{
		for(sysint n = 0; n < values.Length(); n++)
		{
			values[n]->attached = FALSE;
			values[n]->data.Release();
			values[n]->Release();
		}
		values.Clear();
	}
	HRESULT CListView::ParseItems(IJSONValue* value, TArray<CListItem*>& result)
	{
		HRESULT hr;
		TStackRef<IJSONArray> array;
		TRStrMap<BOOL> identifiers;
		CheckIf(!value,E_INVALIDARG);
		Check(value->GetArray(&array));
		CheckIf(array->Count() > 1000000 || (Boolean(L"OwnerData") && array->Count()),E_INVALIDARG);
		for(sysint n = 0; n < array->Count(); n++)
		{
			TStackRef<IJSONValue> entry;
			TStackRef<IJSONValue> field;
			TStackRef<IJSONObject> object;
			TStackRef<IJSONArray> cells;
			TStackRef<CListItem> item;
			CRString key;
			Check(array->GetValue(n,&entry));
			CheckIf(!entry,E_INVALIDARG);
			Check(entry->GetObject(&object));
			for(sysint member = 0; member < object->Count(); member++)
			{
				CRString name;
				Check(object->GetValueName(member,&name));
				CheckIf(!(name == RSTRING_CAST(L"id")) && !(name == RSTRING_CAST(L"Caption")) &&
					!(name == RSTRING_CAST(L"SubItems")) && !(name == RSTRING_CAST(L"ImageIndex")) &&
					!(name == RSTRING_CAST(L"StateIndex")) && !(name == RSTRING_CAST(L"Checked")),E_INVALIDARG);
			}
			item.Attach(__new CListItem(m_link));
			CheckAlloc(item);
			if(object->HasField(RSTRING_CAST(L"id")))
			{
				Check(object->FindNonNullValueW(L"id",&field));
				Check(field->GetStringAsGuid(item->id));
			}
			else
			{
				Check(CoCreateGuid(&item->id));
			}
			CheckIf(IsEqualGUID(item->id,GUID_NULL),E_INVALIDARG);
			Check(GuidString(item->id,&key));
			CheckIf(identifiers.HasItem(key),QuadooForm::EDuplicateID);
			Check(identifiers.Add(key,TRUE));
			Check(ReadString(object,L"Caption",L"",&item->caption));
			Check(ValidateListText(item->caption));
			Check(ReadInt(object,L"ImageIndex",-1,&item->image));
			CheckIf(item->image < -1,E_INVALIDARG);
			if(Boolean(L"CheckBoxes"))
			{
				bool checked = false;
				CheckIf(object->HasField(RSTRING_CAST(L"StateIndex")),E_INVALIDARG);
				if(object->HasField(RSTRING_CAST(L"Checked")))
				{
					field.Release();
					Check(object->FindNonNullValueW(L"Checked",&field));
					Check(field->GetBoolean(&checked));
				}
				item->state = checked ? 1 : 0;
			}
			else
			{
				CheckIf(object->HasField(RSTRING_CAST(L"Checked")),E_INVALIDARG);
				Check(ReadInt(object,L"StateIndex",-1,&item->state));
				CheckIf(item->state < -1 || item->state > 14,E_INVALIDARG);
			}
			Check(ScCreateRStringArray(&item->strings));
			if(object->HasField(RSTRING_CAST(L"SubItems")))
			{
				field.Release();
				Check(object->FindNonNullValueW(L"SubItems",&field));
				Check(field->GetArray(&cells));
				CheckIf(cells->Count() > 4096,E_INVALIDARG);
				for(sysint cell = 0; cell < cells->Count(); cell++)
				{
					TStackRef<IJSONValue> string;
					CRString text;
					Check(cells->GetValue(cell,&string));
					CheckIf(!string,E_INVALIDARG);
					Check(string->GetString(&text));
					Check(ValidateListText(text));
					Check(item->strings->AddRString(text));
				}
			}
			item->index = static_cast<UINT>(n);
			Check(result.Append(item));
			item.Detach();
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
	HRESULT CListView::WriteItems(IJSONValue** value)
	{
		HRESULT hr;
		TStackRef<IJSONArray> array;
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(JSONCreateArray(&array));
		for(sysint n = 0; n < rows.Length(); n++)
		{
			TStackRef<IJSONObject> object;
			TStackRef<IJSONArray> cells;
			TStackRef<IJSONValue> entry;
			CRString id;
			CListItem* item = rows[n];
			Check(JSONCreateObject(&object));
			Check(GuidString(item->id,&id));
			Check(PutString(object,L"id",id));
			Check(PutString(object,L"Caption",item->caption));
			Check(PutInt(object,L"ImageIndex",item->image));
			if(Boolean(L"CheckBoxes"))
			{
				Check(JSONCreateBool(item->state == 1,&entry));
				Check(object->AddValueW(L"Checked",entry));
				entry.Release();
			}
			else
			{
				Check(PutInt(object,L"StateIndex",item->state));
			}
			Check(JSONCreateArray(&cells));
			for(sysint cell = 0; cell < item->strings->Length(); cell++)
			{
				CRString text;
				Check(item->strings->GetRString(cell,&text));
				Check(JSONCreateString(text,&entry));
				Check(cells->Add(entry));
				entry.Release();
			}
			Check(JSONWrapArray(cells,&entry));
			Check(object->AddValueW(L"SubItems",entry));
			entry.Release();
			Check(JSONWrapObject(object,&entry));
			Check(array->Add(entry));
		}
		Check(JSONWrapArray(array,value));
	Cleanup:
		return hr;
	}
	HRESULT CListView::LoadItems(IJSONValue* value)
	{
		HRESULT hr;
		TArray<CListItem*> candidate;
		Check(ParseItems(value,candidate));
		rows.Swap(candidate);
		hr = m_hwnd ? Rebuild() : S_OK;
		if(FAILED(hr))
		{
			rows.Swap(candidate);
		}
	Cleanup:
		ReleaseItems(candidate);
		return hr;
	}
	HRESULT CListView::RemapCells(UINT operation, UINT from, UINT to, TArray<IRStringArray*>& previous)
	{
		HRESULT hr = S_OK;
		TArray<IRStringArray*> candidates;
		for(sysint n = 0; n < rows.Length(); n++)
		{
			TStackRef<IRStringArray> old;
			TStackRef<IRStringArray> next;
			Check(ScCreateRStringArray(&old));
			Check(old->AddRString(rows[n]->caption));
			for(sysint cell = 0; cell < rows[n]->strings->Length(); cell++)
			{
				CRString text;
				Check(rows[n]->strings->GetRString(cell,&text));
				Check(old->AddRString(text));
			}
			Check(previous.Append(old));
			old.Detach();
			Check(previous[n]->Clone(&next));
			if(operation == 0)
			{
				while(next->Length() < static_cast<sysint>(from))
				{
					Check(next->AddRString(RSTRING_CAST(L"")));
				}
				Check(next->InsertAt(RSTRING_CAST(L""),from));
			}
			else if(operation == 1)
			{
				if(static_cast<sysint>(from) < next->Length())
				{
					Check(next->RemoveString(from,NULL));
				}
			}
			else if(operation == 2)
			{
				CRString text;
				while(next->Length() <= static_cast<sysint>(max(from,to)))
				{
					Check(next->AddRString(RSTRING_CAST(L"")));
				}
				Check(next->RemoveString(from,&text));
				Check(next->InsertAt(text,to));
			}
			else
			{
				for(UINT cell = 0; cell < from && next->Length(); cell++)
				{
					Check(next->RemoveString(0,NULL));
				}
			}
			if(!next->Length())
			{
				Check(next->AddRString(RSTRING_CAST(L"")));
			}
			CheckIf(next->Length() > 4097,E_INVALIDARG);
			Check(candidates.Append(next));
			next.Detach();
		}
		for(sysint n = 0; n < rows.Length(); n++)
		{
			CRString text;
			Check(candidates[n]->RemoveString(0,&text));
			rows[n]->caption = text;
			rows[n]->strings = candidates[n];
		}
	Cleanup:
		for(sysint n = 0; n < candidates.Length(); n++)
		{
			candidates[n]->Release();
		}
		if(FAILED(hr))
		{
			FinishCells(previous,FALSE);
		}
		return hr;
	}
	VOID CListView::FinishCells(TArray<IRStringArray*>& previous, BOOL restore)
	{
		for(sysint n = 0; n < previous.Length(); n++)
		{
			if(restore)
			{
				CRString text;
				previous[n]->RemoveString(0,&text);
				rows[n]->caption = text;
				rows[n]->strings = previous[n];
			}
			previous[n]->Release();
		}
		previous.Clear();
	}
}
