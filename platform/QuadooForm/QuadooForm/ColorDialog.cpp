#include "Runtime.h"
#include "ComponentRuntime.h"
#include "ColorNames.h"

namespace QF
{
static const PropertySpec colorProperties[] =
{
	{ L"Color",QuadooProperty::Color,QuadooForm::PropertySerializable,0,1 }
};
static const EventSpec colorEvents[] = { { L"OnAccept",0,0,NULL,2 }, { L"OnCancel",0,0,NULL,3 } };
static const TypeSpec colorType = { L"ColorDialog",&NonvisualType,colorProperties,ARRAYSIZE(colorProperties),colorEvents,ARRAYSIZE(colorEvents),IDI_PALETTE_COLORDIALOG };

class CColorDialog : public CNonvisual, public IQuadooColorDialog
{
	BOOL m_executing;
protected:
	virtual HRESULT ValidateProperty(PCWSTR name, IJSONValue* value)
	{
		HRESULT hr = S_OK;
		INT number;
		if(!TStrCmpAssert(name,L"Color"))
		{
			Check(value->GetInteger(&number));
			if((static_cast<UINT>(number) & 0xff000000) == 0x80000000)
			{
				CheckIf(!QuadooColors::SystemColorName(number & 0xffffff)[0],E_INVALIDARG);
			}
		}
	Cleanup:
		return hr;
	}
public:
	IMP_BASE_UNKNOWN
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooColorDialog)
		UNK_CHAIN_TO(CNonvisual)
	END_UNK_MAP
	static const TypeSpec* Type() { return &colorType; }
	CColorDialog(IQuadooConstructionContext* context, IQuadooComponentCollection* owner, IUnknown* factory, UINT* live) : CNonvisual(context,owner,factory,live), m_executing(FALSE) {}
	STDMETHOD(Execute)(HWND owner, BOOL* accepted)
	{
		HRESULT hr;
		TStackRef<CColorDialog> keepAlive(this);
		TStackRef<IQuadooControlContainer> root;
		TStackRef<IJSONValue> value, previous;
		COLORREF color;
		UINT index = 0;
		BOOL entered = FALSE, committed = FALSE, edited = FALSE, changed = FALSE;
		CheckIf(!accepted,E_POINTER);
		*accepted = FALSE;
		Check(CheckAlive());
		CheckIf(m_state != QuadooForm::Ready || m_mode != QuadooForm::Runtime || m_executing,QuadooForm::EInvalidState);
		if(owner)
		{
			CheckIf(!IsWindow(owner) || GetWindowThreadProcessId(owner,NULL) != m_thread,E_INVALIDARG);
		}
		else
		{
			CheckIf(!m_owner,QuadooForm::EDestroyed);
			Check(m_owner->GetRoot(&root));
			Check(root->GetHandle(&owner));
			CheckIf(!owner || !IsWindow(owner),QuadooForm::EInvalidState);
		}
		m_executing = TRUE;
		entered = TRUE;
		hr = ShowQuadooColorPicker(owner,Integer(L"Color"),&color);
		if(hr != S_OK)
		{
			if(hr == S_FALSE && m_state == QuadooForm::Ready)
			{
				HRESULT callback = Dispatch(L"OnCancel");
				if(FAILED(callback)) hr = callback;
			}
			goto Cleanup;
		}
		Check(CheckAlive());
		Check(JSONCreateInteger(static_cast<INT>(color),&value));
		index = Find(L"Color");
		CheckIf(index >= static_cast<UINT>(m_values.Length()),E_UNEXPECTED);
		changed = JSONCompare(m_values[index].value,value) != 0;
		edited = m_values[index].edited;
		previous.Attach(m_values[index].value);
		m_values[index].value = value.Detach();
		m_values[index].edited = edited || changed;
		committed = TRUE;
		*accepted = TRUE;
		Check(Dispatch(L"OnAccept"));
		if(changed && m_state == QuadooForm::Ready)
		{
			Check(Notify(index));
		}
		hr = S_OK;
	Cleanup:
		if(FAILED(hr) && committed)
		{
			SafeRelease(m_values[index].value);
			m_values[index].value = previous.Detach();
			m_values[index].edited = edited;
			*accepted = FALSE;
		}
		if(entered) m_executing = FALSE;
		return hr;
	}
};
HRESULT RegisterColorDialogFactory(IQuadooComponentRegistry* registry)
{
	HRESULT hr;
	TStackRef<TComponentFactory<CColorDialog> > factory;
	factory.Attach(__new TComponentFactory<CColorDialog>);
	CheckAlloc(factory);
	Check(factory->Initialize());
	Check(registry->RegisterComponentFactory(factory));
Cleanup:
	return hr;
}
}
