#include "ListView.h"

namespace QF
{
	static const DWORD Stored = QuadooForm::PropertySerializable;
	static const EnumSpec views[] =
	{
		{L"Icon",QuadooForm::ListViewIcon},
		{L"SmallIcon",QuadooForm::ListViewSmallIcon},
		{L"List",QuadooForm::ListViewList},
		{L"Report",QuadooForm::ListViewReport}
	};
	static const PropertySpec listProperties[] =
	{
		{L"Width",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,240,3},
		{L"Height",QuadooProperty::Int,Stored | QuadooForm::PropertyAffectsLayout,160,4},
		{L"BackColor",QuadooProperty::Color,Stored,static_cast<INT>(0x80000000 | COLOR_WINDOW),21},
		{L"ViewStyle",QuadooProperty::Enum,Stored,QuadooForm::ListViewReport,24,views,ARRAYSIZE(views)},
		{L"ReadOnly",QuadooProperty::Boolean,Stored,0,25},
		{L"FullRowSelect",QuadooProperty::Boolean,Stored,1,26},
		{L"OwnerData",QuadooProperty::Boolean,Stored,0,27},
		{L"CheckBoxes",QuadooProperty::Boolean,Stored,0,28},
		{L"MultiSelect",QuadooProperty::Boolean,Stored,0,29},
		{L"HideSelection",QuadooProperty::Boolean,Stored,0,30},
		{L"GridLines",QuadooProperty::Boolean,Stored,0,31},
		{L"ShowColumnHeaders",QuadooProperty::Boolean,Stored,1,32},
		{L"ColumnClick",QuadooProperty::Boolean,Stored,1,33},
		{L"ColumnReorder",QuadooProperty::Boolean,Stored,0,34},
		{L"LargeImages",QuadooProperty::ObjectReference,Stored,0,35},
		{L"SmallImages",QuadooProperty::ObjectReference,Stored,0,36},
		{L"StateImages",QuadooProperty::ObjectReference,Stored,0,37},
		{L"Columns",QuadooProperty::Collection,Stored,0,38},
		{L"Items",QuadooProperty::Collection,Stored | QuadooForm::PropertyHidden,0,39}
	};
	static const ArgumentSpec columnArgs[] =
	{
		{L"Column",QuadooForm::ValueObject,FALSE},
		{L"ColumnIndex",QuadooForm::ValueInt,FALSE}
	};
	static const ArgumentSpec changingArgs[] =
	{
		{L"Cancel",QuadooForm::ValueBool,TRUE},
		{L"Item",QuadooForm::ValueObject,FALSE},
		{L"ChangeMask",QuadooForm::ValueInt,FALSE},
		{L"OldState",QuadooForm::ValueInt,FALSE},
		{L"NewState",QuadooForm::ValueInt,FALSE}
	};
	static const ArgumentSpec changeArgs[] =
	{
		{L"Item",QuadooForm::ValueObject,FALSE},
		{L"ChangeMask",QuadooForm::ValueInt,FALSE}
	};
	static const ArgumentSpec selectedArgs[] =
	{
		{L"Item",QuadooForm::ValueObject,FALSE},
		{L"Selected",QuadooForm::ValueBool,FALSE}
	};
	static const ArgumentSpec itemColumnArgs[] =
	{
		{L"Item",QuadooForm::ValueObject,FALSE},
		{L"ColumnIndex",QuadooForm::ValueInt,FALSE}
	};
	static const ArgumentSpec editArgs[] =
	{
		{L"Cancel",QuadooForm::ValueBool,TRUE},
		{L"Item",QuadooForm::ValueObject,FALSE},
		{L"ColumnIndex",QuadooForm::ValueInt,FALSE},
		{L"Text",QuadooForm::ValueString,TRUE}
	};
	static const ArgumentSpec checkingArgs[] =
	{
		{L"Cancel",QuadooForm::ValueBool,TRUE},
		{L"Item",QuadooForm::ValueObject,FALSE},
		{L"Checked",QuadooForm::ValueBool,TRUE}
	};
	static const ArgumentSpec itemArgs[] =
	{
		{L"Item",QuadooForm::ValueObject,FALSE}
	};
	static const ArgumentSpec compareArgs[] =
	{
		{L"Item1",QuadooForm::ValueObject,FALSE},
		{L"Item2",QuadooForm::ValueObject,FALSE},
		{L"ColumnIndex",QuadooForm::ValueInt,FALSE},
		{L"Result",QuadooForm::ValueInt,TRUE}
	};
	static const ArgumentSpec contextArgs[] =
	{
		{L"Item",QuadooForm::ValueObject,FALSE},
		{L"ColumnIndex",QuadooForm::ValueInt,FALSE},
		{L"X",QuadooForm::ValueInt,FALSE},
		{L"Y",QuadooForm::ValueInt,FALSE},
		{L"Handled",QuadooForm::ValueBool,TRUE}
	};
	static const ArgumentSpec dataArgs[] =
	{
		{L"Item",QuadooForm::ValueObject,FALSE},
		{L"RequestedFields",QuadooForm::ValueInt,FALSE},
		{L"ColumnIndex",QuadooForm::ValueInt,FALSE}
	};
	static const ArgumentSpec updateArgs[] =
	{
		{L"Cancel",QuadooForm::ValueBool,TRUE},
		{L"Item",QuadooForm::ValueObject,FALSE},
		{L"ChangeMask",QuadooForm::ValueInt,FALSE},
		{L"ColumnIndex",QuadooForm::ValueInt,FALSE},
		{L"Handled",QuadooForm::ValueBool,TRUE}
	};
	static const ArgumentSpec rangeArgs[] =
	{
		{L"FirstIndex",QuadooForm::ValueInt,FALSE},
		{L"LastIndex",QuadooForm::ValueInt,FALSE},
		{L"OldState",QuadooForm::ValueInt,FALSE},
		{L"NewState",QuadooForm::ValueInt,FALSE}
	};
	static const ArgumentSpec findArgs[] =
	{
		{L"Text",QuadooForm::ValueString,FALSE},
		{L"StartIndex",QuadooForm::ValueInt,FALSE},
		{L"Flags",QuadooForm::ValueInt,FALSE},
		{L"ResultIndex",QuadooForm::ValueInt,TRUE}
	};
	static const EventSpec listEvents[] =
	{
		{L"OnColumnClick",0,2,columnArgs,20},
		{L"OnChanging",QuadooForm::EventCancellable,5,changingArgs,21},
		{L"OnChange",0,2,changeArgs,22},
		{L"OnSelectItem",0,2,selectedArgs,23},
		{L"OnItemActivate",0,2,itemColumnArgs,24},
		{L"OnEditing",QuadooForm::EventCancellable,3,editArgs,25},
		{L"OnEdited",QuadooForm::EventCancellable,4,editArgs,26},
		{L"OnItemChecking",QuadooForm::EventCancellable,3,checkingArgs,27},
		{L"OnItemChecked",0,1,itemArgs,28},
		{L"OnInsert",0,1,itemArgs,29},
		{L"OnDeletion",0,1,itemArgs,30},
		{L"OnCompare",0,4,compareArgs,31},
		{L"OnContextPopup",0,5,contextArgs,32},
		{L"OnData",0,3,dataArgs,33},
		{L"OnDataUpdate",QuadooForm::EventCancellable,5,updateArgs,34},
		{L"OnDataHint",0,2,rangeArgs,35},
		{L"OnDataFind",0,4,findArgs,36},
		{L"OnDataStateChange",0,4,rangeArgs,37}
	};
	const TypeSpec* CListView::Type()
	{
		static const TypeSpec type = {L"ListView",NativeWindowType(),listProperties,
			ARRAYSIZE(listProperties),listEvents,ARRAYSIZE(listEvents),IDI_PALETTE_LISTVIEW};
		return &type;
	}
	HRESULT RegisterListViewFactory(IQuadooControlRegistry* registry)
	{
		HRESULT hr;
		TStackRef<CFactory> factory;
		factory.Attach(__new TFactory<CListView>);
		CheckAlloc(factory);
		Check(factory->Initialize());
		Check(registry->RegisterFactory(factory));
	Cleanup:
		return hr;
	}
}
