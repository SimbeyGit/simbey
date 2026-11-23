#include "Runtime.h"

namespace QF
{
static const DWORD Stored = QuadooForm::PropertySerializable;
static const DWORD Layout = Stored | QuadooForm::PropertyAffectsLayout;
struct PropertyRule { PCWSTR name; QuadooProperty::Type type; DWORD flags; BOOL mandatory; };
// These are contracts for framework names, not a table of control capabilities.
// Appearance/layout extensions remain optional for independent implementations.
static const PropertyRule rules[] =
{
	{ L"Name", QuadooProperty::String, Stored, TRUE },
	{ L"Left", QuadooProperty::Int, Layout, TRUE }, { L"Top", QuadooProperty::Int, Layout, TRUE },
	{ L"Width", QuadooProperty::Int, Layout, TRUE }, { L"Height", QuadooProperty::Int, Layout, TRUE },
	{ L"Visible", QuadooProperty::Boolean, Layout, TRUE }, { L"Enabled", QuadooProperty::Boolean, Stored, TRUE },
	{ L"Align", QuadooProperty::Enum, Layout, FALSE }, { L"Anchors", QuadooProperty::Flags, Layout, FALSE },
	{ L"Padding", QuadooProperty::Rect, Layout, FALSE }, { L"Margins", QuadooProperty::Rect, Layout, FALSE },
	{ L"AutoSize", QuadooProperty::Boolean, Layout, FALSE },
	{ L"MinWidth", QuadooProperty::Int, Layout, FALSE }, { L"MinHeight", QuadooProperty::Int, Layout, FALSE },
	{ L"MaxWidth", QuadooProperty::Int, Layout, FALSE }, { L"MaxHeight", QuadooProperty::Int, Layout, FALSE },
	{ L"Caption", QuadooProperty::String, Layout, FALSE }, { L"Text", QuadooProperty::String, Stored, FALSE },
	{ L"TabOrder", QuadooProperty::Int, Layout, FALSE }, { L"Font", QuadooProperty::Font, Layout, FALSE },
	{ L"ForeColor", QuadooProperty::Color, Stored, FALSE }, { L"BackColor", QuadooProperty::Color, Stored, FALSE },
	{ L"AutoScroll", QuadooProperty::Boolean, Layout, FALSE },
	{ L"PopupMenu", QuadooProperty::ObjectReference, Stored, FALSE },
	{ L"MainMenu", QuadooProperty::ObjectReference, Stored, FALSE },
	{ L"Checked", QuadooProperty::Boolean, Stored, FALSE },
	{ L"CheckState", QuadooProperty::Enum, Stored, FALSE },
	{ L"ThreeState", QuadooProperty::Boolean, Stored, FALSE },
	{ L"Items", QuadooProperty::List, Stored, FALSE },
	{ L"ItemIndex", QuadooProperty::Int, Stored, FALSE },
	{ L"ActivePageIndex", QuadooProperty::Int, Stored, FALSE },
	{ L"TabVisible", QuadooProperty::Boolean, Layout, FALSE },
	{ L"KeyPreview", QuadooProperty::Boolean, Stored, FALSE },
	{ L"Default", QuadooProperty::Boolean, Stored, FALSE },
	{ L"Cancel", QuadooProperty::Boolean, Stored, FALSE },
	{ L"Icon", QuadooProperty::Icon, Stored, FALSE },
	{ L"ModalResult", QuadooProperty::Long, QuadooForm::PropertyRuntimeOnly, FALSE }
};

struct DescriptorRecord { RSTRING name; QuadooProperty::Type type; DWORD flags; IJSONValue* initial; };
class CDescriptorSnapshot
{
public:
	TArray<DescriptorRecord> items;
	~CDescriptorSnapshot()
	{
		for(sysint n = 0; n < items.Length(); n++) { RStrRelease(items[n].name); SafeRelease(items[n].initial); }
	}
	UINT Find(RSTRING name) const
	{
		UINT n;
		for(n = 0; n < static_cast<UINT>(items.Length()); n++) if(SameString(items[n].name, name)) break;
		return n;
	}
	HRESULT Read(IQuadooPropertyDescriptors* descriptors, BOOL visual = TRUE)
	{
		HRESULT hr;
		UINT count;
		CheckIf(!descriptors, E_INVALIDARG); Check(descriptors->GetCount(&count));
		CheckIf(count > 4096, QuadooForm::EInvalidDefinition);
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooPropertyDescriptor> descriptor;
			TStackRef<IJSONValue> initial;
			CRString name;
			DescriptorRecord record;
			UINT enumCount;
			Check(descriptors->GetAt(n, &descriptor)); CheckIf(!descriptor, QuadooForm::EInvalidDefinition);
			Check(descriptor->GetName(&name));
			CheckIf(!RStrIsWide(static_cast<RSTRING>(name)) || !RStrLen(name) || RStrLen(name) > 256, QuadooForm::EInvalidDefinition);
			CheckIf(Find(name) < static_cast<UINT>(items.Length()), QuadooForm::EInvalidDefinition);
			Check(descriptor->GetType(&record.type)); Check(descriptor->GetFlags(&record.flags));
			CheckIf(!visual && ((record.flags & QuadooForm::PropertyAffectsLayout) || SameString(name,RSTRING_CAST(L"TabOrder"))),QuadooForm::EInvalidDefinition);
			CheckIf(record.type < QuadooProperty::Int || record.type > QuadooProperty::Picture || (record.flags & ~0x3f), QuadooForm::EInvalidDefinition);
			CheckIf((record.flags & QuadooForm::PropertyRuntimeOnly) && (record.flags & (QuadooForm::PropertyDesignOnly | Stored)), QuadooForm::EInvalidDefinition);
			Check(descriptor->GetDefaultValue(&initial));
			if(record.type == QuadooProperty::Picture)
			{
				TStackRef<QFI::CImageResource> picture;
				Check(QFI::CImageResource::Create(initial,&picture));
			}
			const PropertyRule* rule = NULL;
			for(UINT r = 0; r < ARRAYSIZE(rules); r++) if(SameString(name, RSTRING_CAST(rules[r].name))) { rule = &rules[r]; break; }
			if(rule)
			{
				TStackRef<IJSONValue> normalized;
				PropertySpec spec = { rule->name, rule->type, rule->flags, 0, 0 };
				DWORD required = SameString(name,RSTRING_CAST(L"ModalResult")) && (record.flags & Stored) ? Stored : rule->flags;
				if(!visual)
				{
					required &= ~QuadooForm::PropertyAffectsLayout;
				}
				BOOL memoText = SameString(name,RSTRING_CAST(L"Text")) && record.type == QuadooProperty::Memo;
				BOOL nodeItems = SameString(name,RSTRING_CAST(L"Items")) && record.type == QuadooProperty::Collection;
				if(nodeItems)
				{
					spec.type = QuadooProperty::Collection;
				}
				CheckIf((record.type != rule->type && !memoText && !nodeItems) || (record.flags & ~QuadooForm::PropertyHidden) != required, QuadooForm::EInvalidDefinition);
				Check(NormalizeValue(&spec, initial, &normalized));
			}
			else if(initial)
			{
				JSON::Type type = initial->GetType();
				if(record.type == QuadooProperty::String || record.type == QuadooProperty::Event || record.type == QuadooProperty::Memo)
				{ CRString text; Check(initial->GetString(&text)); CheckIf(!RStrIsWide(static_cast<RSTRING>(text)), QuadooForm::EInvalidDefinition); }
				else if(record.type == QuadooProperty::Boolean) { CheckIf(type != JSON::Boolean, QuadooForm::EInvalidDefinition); }
				else if(record.type == QuadooProperty::Color)
				{
					PropertySpec spec = { name.c_wstr(),record.type,record.flags,0,0 };
					TStackRef<IJSONValue> normalized;
					Check(NormalizeValue(&spec,initial,&normalized));
				}
				else if(record.type == QuadooProperty::Int || record.type == QuadooProperty::Enum || record.type == QuadooProperty::Flags)
				{ CheckIf(type != JSON::Integer, QuadooForm::EInvalidDefinition); }
				else if(record.type == QuadooProperty::Long) { CheckIf(type != JSON::LongInteger && type != JSON::Integer, QuadooForm::EInvalidDefinition); }
				else if(record.type == QuadooProperty::Float) { CheckIf(type != JSON::Double && type != JSON::Float && type != JSON::Integer, QuadooForm::EInvalidDefinition); }
				else if(record.type == QuadooProperty::Rect || record.type == QuadooProperty::Point || record.type == QuadooProperty::Size || record.type == QuadooProperty::Font)
				{ CheckIf(type != JSON::Object, QuadooForm::EInvalidDefinition); }
			}
			else { CheckIf(record.type != QuadooProperty::ObjectReference && record.type != QuadooProperty::Icon && record.type != QuadooProperty::Picture && record.type != QuadooProperty::List && record.type != QuadooProperty::Collection, QuadooForm::EInvalidDefinition); }
			Check(descriptor->GetEnumCount(&enumCount)); CheckIf(enumCount > 4096, QuadooForm::EInvalidDefinition);
			if(SameString(name, RSTRING_CAST(L"Align"))) { CheckIf(enumCount != 6, QuadooForm::EInvalidDefinition); }
			if(SameString(name, RSTRING_CAST(L"Anchors"))) { CheckIf(enumCount != 4, QuadooForm::EInvalidDefinition); }
			if(SameString(name, RSTRING_CAST(L"CheckState")))
			{
				INT state; Check(initial->GetInteger(&state));
				CheckIf(enumCount != 3 || state < BST_UNCHECKED || state > BST_INDETERMINATE, QuadooForm::EInvalidDefinition);
			}
			TRStrMap<BOOL> enumNames(RStrCompareRStr);
			for(UINT e = 0; e < enumCount; e++)
			{
				CRString enumName; LONGLONG enumValue;
				Check(descriptor->GetEnumValue(e, &enumName, &enumValue));
				CheckIf(!RStrIsWide(static_cast<RSTRING>(enumName)) || !RStrLen(enumName), QuadooForm::EInvalidDefinition);
				hr = enumNames.Add(enumName, TRUE); CheckIf(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), QuadooForm::EInvalidDefinition); Check(hr);
				if(SameString(name, RSTRING_CAST(L"Align"))) { CheckIf(enumValue != e, QuadooForm::EInvalidDefinition); }
				if(SameString(name, RSTRING_CAST(L"Anchors"))) { CheckIf(enumValue != (static_cast<LONGLONG>(1) << e), QuadooForm::EInvalidDefinition); }
				if(SameString(name, RSTRING_CAST(L"CheckState"))) { CheckIf(enumValue != e, QuadooForm::EInvalidDefinition); }
			}
			record.name = name; record.initial = initial;
			Check(items.Append(record)); name.Detach(); initial.Detach();
		}
		for(UINT r = 0; r < ARRAYSIZE(rules); r++)
			if(rules[r].mandatory && (visual || r == 0)) { CheckIf(Find(RSTRING_CAST(rules[r].name)) == static_cast<UINT>(items.Length()), QuadooForm::EInvalidDefinition); }
		hr = S_OK;
	Cleanup:
		return hr;
	}
};

HRESULT ValidateDescriptors(IQuadooPropertyDescriptors* properties, IQuadooEventDescriptors* events, BOOL visual)
{
	HRESULT hr;
	CDescriptorSnapshot snapshot;
	TRStrMap<BOOL> names(RStrCompareRStr);
	UINT count;
	Check(snapshot.Read(properties,visual));
	if(events)
	{
		Check(events->GetCount(&count)); CheckIf(count > 4096, QuadooForm::EInvalidDefinition);
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IQuadooEventDescriptor> event;
			CRString name;
			DWORD flags; UINT arguments;
			Check(events->GetAt(n, &event)); CheckIf(!event, QuadooForm::EInvalidDefinition);
			Check(event->GetName(&name)); CheckIf(!RStrIsWide(static_cast<RSTRING>(name)) || !RStrLen(name) || RStrLen(name) > 256, QuadooForm::EInvalidDefinition);
			UINT index = snapshot.Find(name);
			CheckIf(index == static_cast<UINT>(snapshot.items.Length()) || snapshot.items[index].type != QuadooProperty::Event, QuadooForm::EInvalidDefinition);
			hr = names.Add(name, TRUE); CheckIf(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), QuadooForm::EInvalidDefinition); Check(hr);
			Check(event->GetFlags(&flags)); CheckIf(flags & ~3, QuadooForm::EInvalidDefinition);
			Check(event->GetArgumentCount(&arguments)); CheckIf(arguments > 256, QuadooForm::EInvalidDefinition);
			TRStrMap<BOOL> argumentNames(RStrCompareRStr);
			for(UINT a = 0; a < arguments; a++)
			{
				CRString argumentName; QuadooForm::EventValueType type; BOOL writable;
				Check(event->GetArgument(a, &argumentName, &type, &writable));
				CheckIf(!RStrIsWide(static_cast<RSTRING>(argumentName)) || !RStrLen(argumentName) || type < QuadooForm::ValueEmpty || type > QuadooForm::ValueObject, QuadooForm::EInvalidDefinition);
				hr = argumentNames.Add(argumentName, TRUE); CheckIf(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), QuadooForm::EInvalidDefinition); Check(hr);
			}
		}
		for(sysint n = 0; n < snapshot.items.Length(); n++)
			if(snapshot.items[n].type == QuadooProperty::Event) { BOOL found; CheckIf(FAILED(names.Find(snapshot.items[n].name, &found)), QuadooForm::EInvalidDefinition); }
	}
	hr = S_OK;
Cleanup:
	return hr;
}

HRESULT ValidateFactory(IQuadooControlFactory* factory)
{
	HRESULT hr;
	TStackRef<IQuadooPropertyDescriptors> properties;
	TStackRef<IQuadooEventDescriptors> events;
	CheckIf(!factory, E_INVALIDARG);
	Check(factory->GetProperties(&properties)); Check(factory->GetEvents(&events));
	CheckIf(!properties || !events, QuadooForm::EInvalidDefinition);
	Check(ValidateDescriptors(properties, events));
Cleanup:
	return hr;
}

HRESULT ValidateComponentFactory(IQuadooComponentFactory* factory)
{
	HRESULT hr;
	TStackRef<IQuadooPropertyDescriptors> properties; TStackRef<IQuadooEventDescriptors> events;
	CheckIf(!factory,E_INVALIDARG);
	Check(factory->GetProperties(&properties));
	Check(factory->GetEvents(&events));
	CheckIf(!properties || !events,QuadooForm::EInvalidDefinition);
	Check(ValidateDescriptors(properties,events,FALSE));
Cleanup:
	return hr;
}
HRESULT ValidateComponentMetadata(IQuadooComponent* component, IQuadooComponentFactory* factory)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> values; TStackRef<IQuadooPropertyDescriptors> descriptors, expected;
	TStackRef<IQuadooControl> visual; TStackRef<IQuadooNonvisualComponent> nonvisual;
	CDescriptorSnapshot actualSnapshot, expectedSnapshot;
	CheckIf(!component,E_INVALIDARG);
	Check(component->QueryInterface(__uuidof(IQuadooNonvisualComponent),reinterpret_cast<PVOID*>(&nonvisual)));
	CheckIf(component->QueryInterface(__uuidof(IQuadooControl),reinterpret_cast<PVOID*>(&visual)) != E_NOINTERFACE,QuadooForm::EInvalidDefinition);
	Check(component->GetProperties(&values));
	CheckIf(!values,QuadooForm::EInvalidDefinition);
	Check(values->GetDescriptors(&descriptors));
	CheckIf(!descriptors,QuadooForm::EInvalidDefinition);
	Check(actualSnapshot.Read(descriptors,FALSE));
	if(factory)
	{
		Check(factory->GetProperties(&expected));
		Check(expectedSnapshot.Read(expected,FALSE));
		CheckIf(actualSnapshot.items.Length() != expectedSnapshot.items.Length(),QuadooForm::EInvalidDefinition);
		for(sysint n = 0; n < actualSnapshot.items.Length(); n++)
		{
			const DescriptorRecord& a = actualSnapshot.items[n]; const DescriptorRecord& e = expectedSnapshot.items[n];
			CheckIf(!SameString(a.name,e.name) || a.type != e.type || a.flags != e.flags || JSONCompare(a.initial,e.initial),QuadooForm::EInvalidDefinition);
		}
	}
	hr = S_OK;
Cleanup:
	return hr;
}

HRESULT ValidateControlMetadata(IQuadooControl* control, IQuadooControlFactory* factory)
{
	HRESULT hr;
	TStackRef<IQuadooProperties> properties;
	TStackRef<IQuadooPropertyDescriptors> descriptors, expected;
	TStackRef<IQuadooWinControl> window;
	TStackRef<IQuadooRadioButton> radio;
	TStackRef<IQuadooComboBox> combo;
	TStackRef<IQuadooListBox> list;
	TStackRef<IQuadooTabControl> tabs;
	TStackRef<IQuadooTabPage> page;
	TStackRef<IQuadooDesignPlaceholder> placeholder;
	CDescriptorSnapshot actualSnapshot, expectedSnapshot;
	CheckIf(!control, E_INVALIDARG);
	Check(control->GetProperties(&properties)); CheckIf(!properties, QuadooForm::EInvalidDefinition);
	Check(properties->GetDescriptors(&descriptors)); CheckIf(!descriptors, QuadooForm::EInvalidDefinition); Check(actualSnapshot.Read(descriptors));
	Check(QueryOptional(control, &window));
	Check(QueryOptional(control,&placeholder));
	if(window && !placeholder) { CheckIf(actualSnapshot.Find(RSTRING_CAST(L"TabOrder")) == static_cast<UINT>(actualSnapshot.items.Length()), QuadooForm::EInvalidDefinition); }
	Check(QueryOptional(control, &radio));
	if(radio) { CheckIf(actualSnapshot.Find(RSTRING_CAST(L"Checked")) == static_cast<UINT>(actualSnapshot.items.Length()), QuadooForm::EInvalidDefinition); }
	Check(QueryOptional(control, &combo)); Check(QueryOptional(control, &list));
	if(combo || list)
	{
		CheckIf(actualSnapshot.Find(RSTRING_CAST(L"Items")) == static_cast<UINT>(actualSnapshot.items.Length()) ||
			actualSnapshot.Find(RSTRING_CAST(L"ItemIndex")) == static_cast<UINT>(actualSnapshot.items.Length()), QuadooForm::EInvalidDefinition);
	}
	if(combo) { CheckIf(actualSnapshot.Find(RSTRING_CAST(L"Text")) == static_cast<UINT>(actualSnapshot.items.Length()), QuadooForm::EInvalidDefinition); }
	Check(QueryOptional(control,&tabs)); Check(QueryOptional(control,&page));
	if(tabs) { CheckIf(actualSnapshot.Find(RSTRING_CAST(L"ActivePageIndex")) == static_cast<UINT>(actualSnapshot.items.Length()),QuadooForm::EInvalidDefinition); }
	if(page) { CheckIf(actualSnapshot.Find(RSTRING_CAST(L"Caption")) == static_cast<UINT>(actualSnapshot.items.Length()) || actualSnapshot.Find(RSTRING_CAST(L"TabVisible")) == static_cast<UINT>(actualSnapshot.items.Length()),QuadooForm::EInvalidDefinition); }
	if(factory)
	{
		Check(factory->GetProperties(&expected)); Check(expectedSnapshot.Read(expected));
		CheckIf(actualSnapshot.items.Length() != expectedSnapshot.items.Length(), QuadooForm::EInvalidDefinition);
		for(sysint n = 0; n < actualSnapshot.items.Length(); n++)
		{
			const DescriptorRecord& a = actualSnapshot.items[n]; const DescriptorRecord& e = expectedSnapshot.items[n];
			CheckIf(!SameString(a.name, e.name) || a.type != e.type || a.flags != e.flags || JSONCompare(a.initial, e.initial), QuadooForm::EInvalidDefinition);
			TStackRef<IQuadooPropertyDescriptor> actualDescriptor, expectedDescriptor;
			UINT actualEnums, expectedEnums;
			Check(descriptors->GetAt(static_cast<UINT>(n), &actualDescriptor)); Check(expected->GetAt(static_cast<UINT>(n), &expectedDescriptor));
			CheckIf(!actualDescriptor || !expectedDescriptor, QuadooForm::EInvalidDefinition);
			Check(actualDescriptor->GetEnumCount(&actualEnums)); Check(expectedDescriptor->GetEnumCount(&expectedEnums));
			CheckIf(actualEnums != expectedEnums || actualEnums > 4096, QuadooForm::EInvalidDefinition);
			for(UINT v = 0; v < actualEnums; v++)
			{
				CRString actualName, expectedName; LONGLONG actualValue, expectedValue;
				Check(actualDescriptor->GetEnumValue(v,&actualName,&actualValue)); Check(expectedDescriptor->GetEnumValue(v,&expectedName,&expectedValue));
				CheckIf(!SameString(actualName,expectedName) || actualValue != expectedValue, QuadooForm::EInvalidDefinition);
			}
		}
	}
	hr = S_OK;
Cleanup:
	return hr;
}
}
