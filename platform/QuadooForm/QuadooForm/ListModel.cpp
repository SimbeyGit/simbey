#include "ListView.h"

namespace QF
{
	static HRESULT ValidateCaption(RSTRING text)
	{
		HRESULT hr = S_OK;
		CheckIf(!text || !RStrIsWide(text) || RStrLen(text) > 32767,E_INVALIDARG);
		for(INT n = 0; n < RStrLen(text); n++)
		{
			CheckIf(!RStrToWide(text)[n],E_INVALIDARG);
		}
	Cleanup:
		return hr;
	}
	VOID CListView::ReleaseColumns(TArray<CListColumn*>& values)
	{
		for(sysint n = 0; n < values.Length(); n++)
		{
			values[n]->attached = FALSE;
			values[n]->Release();
		}
		values.Clear();
	}
	HRESULT CListView::ParseColumns(IJSONValue* value, TArray<CListColumn*>& result)
	{
		HRESULT hr;
		TStackRef<IJSONArray> array;
		CheckIf(!value,E_INVALIDARG);
		Check(value->GetArray(&array));
		// This bound applies to columns only, never to the eventual flat row set.
		CheckIf(array->Count() > 4096,E_INVALIDARG);
		for(sysint n = 0; n < array->Count(); n++)
		{
			TStackRef<IJSONValue> entry;
			TStackRef<IJSONValue> field;
			TStackRef<IJSONObject> object;
			TStackRef<CListColumn> column;
			INT number;
			Check(array->GetValue(n,&entry));
			CheckIf(!entry,E_INVALIDARG);
			Check(entry->GetObject(&object));
			for(sysint fieldIndex = 0; fieldIndex < object->Count(); fieldIndex++)
			{
				CRString name;
				Check(object->GetValueName(fieldIndex,&name));
				CheckIf(!(name == RSTRING_CAST(L"id")) && !(name == RSTRING_CAST(L"Caption")) &&
					!(name == RSTRING_CAST(L"Width")) && !(name == RSTRING_CAST(L"Alignment")) &&
					!(name == RSTRING_CAST(L"DisplayIndex")),E_INVALIDARG);
			}
			column.Attach(__new CListColumn(m_link));
			CheckAlloc(column);
			if(object->HasField(RSTRING_CAST(L"id")))
			{
				Check(object->FindNonNullValueW(L"id",&field));
				Check(field->GetStringAsGuid(column->id));
			}
			else
			{
				Check(CoCreateGuid(&column->id));
			}
			CheckIf(IsEqualGUID(column->id,GUID_NULL),E_INVALIDARG);
			Check(ReadString(object,L"Caption",L"",&column->caption));
			Check(ValidateCaption(column->caption));
			Check(ReadInt(object,L"Width",100,&column->width));
			CheckIf(column->width < 0 || column->width > 1000000,E_INVALIDARG);
			Check(ReadInt(object,L"DisplayIndex",static_cast<INT>(n),&number));
			CheckIf(number < 0 || number >= array->Count(),E_INVALIDARG);
			column->display = number;
			if(object->HasField(RSTRING_CAST(L"Alignment")))
			{
				field.Release();
				Check(object->FindNonNullValueW(L"Alignment",&field));
				if(field->GetType() == JSON::String)
				{
					CRString alignment;
					Check(field->GetString(&alignment));
					if(alignment == RSTRING_CAST(L"Left"))
					{
						number = QuadooForm::ListColumnLeft;
					}
					else if(alignment == RSTRING_CAST(L"Center"))
					{
						number = QuadooForm::ListColumnCenter;
					}
					else
					{
						CheckIf(!(alignment == RSTRING_CAST(L"Right")),E_INVALIDARG);
						number = QuadooForm::ListColumnRight;
					}
				}
				else
				{
					Check(field->GetInteger(&number));
				}
				CheckIf(number < QuadooForm::ListColumnLeft || number > QuadooForm::ListColumnRight,E_INVALIDARG);
				column->alignment = static_cast<QuadooForm::ListColumnAlignment>(number);
			}
			CheckIf(n == 0 && column->alignment != QuadooForm::ListColumnLeft,E_INVALIDARG);
			for(sysint previous = 0; previous < result.Length(); previous++)
			{
				CheckIf(IsEqualGUID(result[previous]->id,column->id),QuadooForm::EDuplicateID);
				CheckIf(result[previous]->display == column->display,E_INVALIDARG);
			}
			Check(result.Append(column));
			column.Detach();
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}
	HRESULT CListView::WriteColumns(IJSONValue** value)
	{
		HRESULT hr;
		TStackRef<IJSONArray> array;
		static PCWSTR const alignments[] = {L"Left",L"Center",L"Right"};
		CheckIf(!value,E_POINTER);
		*value = NULL;
		Check(JSONCreateArray(&array));
		for(sysint n = 0; n < columns.Length(); n++)
		{
			TStackRef<IJSONObject> object;
			TStackRef<IJSONValue> entry;
			CRString id;
			CListColumn* column = columns[n];
			Check(JSONCreateObject(&object));
			Check(GuidString(column->id,&id));
			Check(PutString(object,L"id",id));
			Check(PutString(object,L"Caption",column->caption));
			// ReadNativeColumns normalizes user-resized native widths to 96 DPI.
			Check(PutInt(object,L"Width",column->width));
			Check(PutString(object,L"Alignment",RSTRING_CAST(alignments[column->alignment])));
			Check(PutInt(object,L"DisplayIndex",column->display));
			Check(JSONWrapObject(object,&entry));
			Check(array->Add(entry));
		}
		Check(JSONWrapArray(array,value));
	Cleanup:
		return hr;
	}
	HRESULT CListView::LoadColumns(IJSONValue* value)
	{
		HRESULT hr;
		TArray<CListColumn*> candidate;
		Check(ParseColumns(value,candidate));
		columns.Swap(candidate);
		hr = m_hwnd ? Rebuild() : S_OK;
		if(FAILED(hr))
		{
			columns.Swap(candidate);
		}
	Cleanup:
		ReleaseColumns(candidate);
		return hr;
	}
	HRESULT CListView::ResolveColumn(IQuadooListColumn* column, UINT* index)
	{
		HRESULT hr = E_INVALIDARG;
		*index = 0;
		for(sysint n = 0; column && n < columns.Length(); n++)
		{
			if(SameObject(column,static_cast<IQuadooListColumn*>(columns[n])))
			{
				*index = static_cast<UINT>(n);
				hr = S_OK;
				break;
			}
		}
		return hr;
	}
	HRESULT CListView::NotifyColumns()
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		m_busy++;
		if(IsVirtual())
		{
			m_generation++;
		}
		Check(Dirty(L"Columns"));
		if(rows.Length())
		{
			Check(Dirty(L"Items"));
		}
		Check(FlushNotifications());
		Check(CheckAlive());
		Check(DrainVirtualStates());
	Cleanup:
		m_busy--;
		return hr;
	}
	HRESULT CListView::EditColumn(CListColumn* column, UINT field, RSTRING text, INT number)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListColumn> keepColumn(column);
		UINT index;
		CRString oldText;
		INT oldWidth;
		QuadooForm::ListColumnAlignment oldAlignment;
		UINT oldDisplay;
		Check(Mutable());
		Check(ResolveColumn(column,&index));
		oldText = column->caption;
		oldWidth = column->width;
		oldAlignment = column->alignment;
		oldDisplay = column->display;
		if(field == 0)
		{
			Check(ValidateCaption(text));
			CheckIfIgnore(SameString(text,column->caption),S_FALSE);
			column->caption = text;
		}
		else if(field == 1)
		{
			CheckIf(number < 0 || number > 1000000,E_INVALIDARG);
			CheckIfIgnore(number == oldWidth,S_FALSE);
			column->width = number;
		}
		else if(field == 2)
		{
			CheckIf(number < QuadooForm::ListColumnLeft || number > QuadooForm::ListColumnRight ||
				(!index && number != QuadooForm::ListColumnLeft),E_INVALIDARG);
			CheckIfIgnore(number == oldAlignment,S_FALSE);
			column->alignment = static_cast<QuadooForm::ListColumnAlignment>(number);
		}
		else
		{
			CheckIf(field != 3 || number < 0 || number >= columns.Length(),E_INVALIDARG);
			CheckIfIgnore(static_cast<UINT>(number) == oldDisplay,S_FALSE);
			for(sysint n = 0; n < columns.Length(); n++)
			{
				CListColumn* other = columns[n];
				if(other != column && other->display > oldDisplay && other->display <= static_cast<UINT>(number))
				{
					other->display--;
				}
				else if(other != column && other->display >= static_cast<UINT>(number) && other->display < oldDisplay)
				{
					other->display++;
				}
			}
			column->display = number;
		}
		hr = Rebuild();
		if(FAILED(hr))
		{
			if(field == 3)
			{
				for(sysint n = 0; n < columns.Length(); n++)
				{
					CListColumn* other = columns[n];
					if(other != column && other->display >= oldDisplay && other->display < static_cast<UINT>(number))
					{
						other->display++;
					}
					else if(other != column && other->display > static_cast<UINT>(number) && other->display <= oldDisplay)
					{
						other->display--;
					}
				}
			}
			column->caption = oldText;
			column->width = oldWidth;
			column->alignment = oldAlignment;
			column->display = oldDisplay;
			goto Cleanup;
		}
		Check(NotifyColumns());
	Cleanup:
		return hr;
	}
	HRESULT CListView::InsertColumn(UINT index, RSTRING caption, IQuadooListColumn** result)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListColumn> column;
		TArray<IRStringArray*> cells;
		BOOL committed = FALSE;
		CheckIf(!result,E_POINTER);
		*result = NULL;
		Check(Mutable());
		Check(ValidateCaption(caption));
		CheckIf(index > static_cast<UINT>(columns.Length()) || columns.Length() >= 4096,E_INVALIDARG);
		Check(columns.Reserve(1));
		column.Attach(__new CListColumn(m_link));
		CheckAlloc(column);
		Check(CoCreateGuid(&column->id));
		Check(RemapCells(0,index,0,cells));
		column->caption = caption;
		column->display = index;
		for(sysint n = 0; n < columns.Length(); n++)
		{
			if(columns[n]->display >= index)
			{
				columns[n]->display++;
			}
		}
		Check(columns.InsertAt(static_cast<CListColumn*>(column),index));
		hr = Rebuild();
		if(FAILED(hr))
		{
			columns.Remove(index,NULL);
			for(sysint n = 0; n < columns.Length(); n++)
			{
				if(columns[n]->display > index)
				{
					columns[n]->display--;
				}
			}
			goto Cleanup;
		}
		column->AddRef();
		committed = TRUE;
		Check(NotifyColumns());
		*result = column.Detach();
	Cleanup:
		FinishCells(cells,!committed);
		return hr;
	}
	HRESULT CListView::DeleteColumn(IQuadooListColumn* value)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TStackRef<CListColumn> column;
		TArray<IRStringArray*> cells;
		BOOL committed = FALSE;
		UINT index;
		Check(Mutable());
		Check(ResolveColumn(value,&index));
		// Native logical column zero must remain left aligned.
		CheckIf(index == 0 && columns.Length() > 1 && columns[1]->alignment != QuadooForm::ListColumnLeft,E_INVALIDARG);
		Check(RemapCells(1,index,0,cells));
		column.Attach(columns[index]);
		columns.Remove(index,NULL);
		for(sysint n = 0; n < columns.Length(); n++)
		{
			if(columns[n]->display > column->display)
			{
				columns[n]->display--;
			}
		}
		hr = Rebuild();
		if(FAILED(hr))
		{
			for(sysint n = 0; n < columns.Length(); n++)
			{
				if(columns[n]->display >= column->display)
				{
					columns[n]->display++;
				}
			}
			columns.InsertAt(static_cast<CListColumn*>(column),index);
			column.Detach();
			goto Cleanup;
		}
		column->attached = FALSE;
		committed = TRUE;
		Check(NotifyColumns());
	Cleanup:
		FinishCells(cells,!committed);
		return hr;
	}
	HRESULT CListView::MoveColumn(IQuadooListColumn* value, UINT index)
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		UINT previous;
		CListColumn* column;
		TArray<IRStringArray*> cells;
		BOOL committed = FALSE;
		Check(Mutable());
		Check(ResolveColumn(value,&previous));
		CheckIf(index >= static_cast<UINT>(columns.Length()),E_INVALIDARG);
		CheckIfIgnore(index == previous,S_FALSE);
		CheckIf((index == 0 && columns[previous]->alignment != QuadooForm::ListColumnLeft) ||
			(previous == 0 && columns[1]->alignment != QuadooForm::ListColumnLeft),E_INVALIDARG);
		column = columns[previous];
		Check(RemapCells(2,previous,index,cells));
		columns.Remove(previous,NULL);
		columns.InsertAt(column,index);
		hr = Rebuild();
		if(FAILED(hr))
		{
			columns.Remove(index,NULL);
			columns.InsertAt(column,previous);
			goto Cleanup;
		}
		committed = TRUE;
		Check(NotifyColumns());
	Cleanup:
		FinishCells(cells,!committed);
		return hr;
	}
	HRESULT CListView::ClearColumns()
	{
		HRESULT hr;
		TStackRef<CListView> keepAlive(this);
		TArray<CListColumn*> previous;
		TArray<IRStringArray*> cells;
		BOOL committed = FALSE;
		Check(Mutable());
		CheckIfIgnore(!columns.Length(),S_FALSE);
		Check(RemapCells(3,static_cast<UINT>(columns.Length()),0,cells));
		columns.Swap(previous);
		hr = Rebuild();
		if(FAILED(hr))
		{
			columns.Swap(previous);
			goto Cleanup;
		}
		ReleaseColumns(previous);
		committed = TRUE;
		Check(NotifyColumns());
	Cleanup:
		FinishCells(cells,!committed);
		return hr;
	}
}
