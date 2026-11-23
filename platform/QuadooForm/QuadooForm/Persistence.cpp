#include "Runtime.h"

namespace QF
{
class CSerializer : public CBaseUnknown, public IQuadooFormSerializer
{
	DWORD m_thread;
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooFormSerializer)
	END_UNK_MAP
	CSerializer() : m_thread(GetCurrentThreadId()) {}
	STDMETHOD(Serialize)(IQuadooComponent* component, IJSONObject** definition)
	{
		HRESULT hr;
		TStackRef<IQuadooControl> control;
		TStackRef<IQuadooProperties> properties;
		TStackRef<IQuadooPropertyDescriptors> descriptors;
		TStackRef<IDefinitionSource> source;
		TStackRef<IQuadooDefinitionSource> publicSource;
		TStackRef<IQuadooDesignPlaceholder> placeholder;
		TStackRef<IJSONObject> root, values, events;
		TStackRef<IJSONArray> children;
		TStackRef<IJSONValue> childList;
		CRString type, name, idText;
		GUID id;
		UINT count;
		CheckIf(!definition, E_POINTER); *definition = NULL; CheckIf(!component, E_INVALIDARG);
		CheckIf(m_thread != GetCurrentThreadId(), QuadooForm::EWrongThread);
		Check(QueryOptional(component,&control));
		Check(QueryOptional(component,&placeholder));
		if(control) { Check(ValidateControlMetadata(control)); }
		else { Check(ValidateComponentMetadata(component)); }
		Check(QueryOptional(component,&publicSource));
		if(publicSource) { Check(publicSource->GetDefinitionTemplate(&root)); }
		else
		{
			Check(QueryOptional(component,&source));
			if(source) { Check(source->GetDefinitionTemplate(&root)); }
			else { Check(JSONCreateObject(&root)); }
		}
		CheckIf(!root,E_UNEXPECTED);
		if(root->HasField(RSTRING_CAST(L"properties")))
		{ TStackRef<IJSONValue> original; Check(root->FindNonNullValueW(L"properties", &original)); Check(original->GetObject(&values)); }
		else { Check(JSONCreateObject(&values)); }
		if(root->HasField(RSTRING_CAST(L"events")))
		{ TStackRef<IJSONValue> original; Check(root->FindNonNullValueW(L"events", &original)); Check(original->GetObject(&events)); }
		else { Check(JSONCreateObject(&events)); }
		values->RemoveValueW(L"Name");
		Check(JSONCreateArray(&children));
		Check(component->GetID(&id)); Check(component->GetName(&name)); Check(component->GetTypeName(&type)); Check(GuidString(id, &idText));
		if(!placeholder) { Check(PutInt(root,L"schemaVersion",1)); }
		Check(PutString(root, L"type", type)); Check(PutString(root, L"name", name)); Check(PutString(root, L"id", idText));
		Check(component->GetProperties(&properties)); CheckIf(!properties,E_UNEXPECTED);
		Check(properties->GetDescriptors(&descriptors)); CheckIf(!descriptors,E_UNEXPECTED);
		Check(descriptors->GetCount(&count)); CheckIf(count > 4096,QuadooForm::EInvalidDefinition);
		for(UINT n = 0; !placeholder && n < count; n++)
		{
			TStackRef<IQuadooPropertyDescriptor> descriptor; TStackRef<IJSONValue> value; CRString propertyName;
			DWORD flags; QuadooProperty::Type propertyType;
			Check(descriptors->GetAt(n, &descriptor)); CheckIf(!descriptor,E_UNEXPECTED); Check(descriptor->GetFlags(&flags));
			Check(descriptor->GetName(&propertyName)); Check(descriptor->GetType(&propertyType));
			if(!(flags & QuadooForm::PropertySerializable) || (flags & QuadooForm::PropertyRuntimeOnly))
			{ values->RemoveValue(propertyName); events->RemoveValue(propertyName); continue; }
			if(SameString(propertyName, RSTRING_CAST(L"Name"))) continue;
			Check(properties->GetValueJSON(n, &value));
			IJSONObject* target = propertyType == QuadooProperty::Event ? static_cast<IJSONObject*>(events) : static_cast<IJSONObject*>(values);
			TStackRef<IJSONValue> clone; Check(JSONClone(value, &clone, TRUE));
			Check(target->AddValue(propertyName, clone));
		}
		{
			TStackRef<IQuadooControlContainer> container;
			if(SUCCEEDED(component->QueryInterface(__uuidof(IQuadooControlContainer), reinterpret_cast<PVOID*>(&container))))
			{
				Check(container->GetChildCount(&count));
				for(UINT n = 0; n < count; n++)
				{
					TStackRef<IQuadooControl> child; TStackRef<IQuadooFrame> frame; TStackRef<IJSONObject> childObject; TStackRef<IJSONValue> childValue;
					Check(container->GetChild(n, &child));
					if(SUCCEEDED(child->QueryInterface(__uuidof(IQuadooFrame), reinterpret_cast<PVOID*>(&frame)))) continue;
					Check(Serialize(child, &childObject)); Check(JSONWrapObject(childObject, &childValue)); Check(children->Add(childValue));
				}
			}
		}
		if(!placeholder) { Check(PutObject(root,L"properties",values)); Check(PutObject(root,L"events",events)); }
		if(control && (!placeholder || root->HasField(RSTRING_CAST(L"children")) || children->Count()))
		{ Check(JSONWrapArray(children,&childList)); Check(root->AddValueW(L"children",childList)); }
		{
			TStackRef<IQuadooComponentHost> host;
			Check(QueryOptional(component,&host));
			if(host)
			{
				TStackRef<IQuadooComponentCollection> collection; TStackRef<IJSONArray> components;
				Check(host->GetComponents(&collection)); Check(collection->GetCount(&count)); Check(JSONCreateArray(&components));
				for(UINT n = 0; n < count; n++)
				{ TStackRef<IQuadooComponent> item; TStackRef<IJSONObject> object; TStackRef<IJSONValue> value; Check(collection->GetAt(n,&item)); Check(Serialize(item,&object)); Check(JSONWrapObject(object,&value)); Check(components->Add(value)); }
				if(count || root->HasField(RSTRING_CAST(L"components"))) { childList.Release(); Check(JSONWrapArray(components,&childList)); Check(root->AddValueW(L"components",childList)); }
			}
		}
		*definition = root.Detach();
Cleanup:
		return hr;
	}
};
}
HRESULT WINAPI QuadooGetFormSerializer(IQuadooFormSerializer** serializer)
{
	HRESULT hr = S_OK;
	CheckIf(!serializer, E_POINTER); *serializer = __new QF::CSerializer; CheckAlloc(*serializer);
Cleanup:
	return hr;
}
