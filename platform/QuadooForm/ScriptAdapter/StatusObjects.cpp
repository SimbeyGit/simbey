#include "StatusObjects.h"

namespace AQVM
{
	interface __declspec(uuid("AD5E9F7E-F0B7-48BE-B702-F351BBCE3A41")) IStatusPanelObject : IUnknown
	{
		STDMETHOD(GetPanel)(IQuadooStatusPanel** panel) = 0;
	};
	static HRESULT Index(const QuadooVM::QVARIANT* value, INT minimum, INT* result)
	{
		HRESULT hr;
		LONGLONG number;
		Check(VariantInteger(value,&number));
		CheckIf(number < minimum || number > INT_MAX,DISP_E_BADINDEX);
		*result = static_cast<INT>(number);
	Cleanup:
		return hr;
	}
	class CStatusPanelObject : public CBaseUnknown, public CQuadooObjectImpl, public IStatusPanelObject
	{
		TStackRef<IQuadooStatusPanel> m_panel;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
			UNK_INTERFACE(IStatusPanelObject)
		END_UNK_MAP
		CStatusPanelObject(IQuadooStatusPanel* panel) : m_panel(panel)
		{
		}
		STDMETHOD(GetPanel)(IQuadooStatusPanel** panel)
		{
			HRESULT hr = S_OK;
			CheckIf(!panel,E_POINTER);
			*panel = m_panel;
			(*panel)->AddRef();
		Cleanup:
			return hr;
		}
		STDMETHOD(GetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			PCWSTR text = RStrToWide(name);
			CheckIf(!text || !result,E_POINTER);
			result->eType = QuadooVM::Null;
			if(!lstrcmpW(text,L"Text"))
			{
				Check(m_panel->GetText(&result->rstrVal));
				result->eType = QuadooVM::String;
			}
			else
			{
				INT number;
				if(!lstrcmpW(text,L"Width"))
				{
					Check(m_panel->GetWidth(&number));
				}
				else if(!lstrcmpW(text,L"Icon"))
				{
					Check(m_panel->GetIcon(&number));
				}
				else
				{
					UINT index;
					CheckIf(lstrcmpW(text,L"Index"),DISP_E_MEMBERNOTFOUND);
					Check(m_panel->GetIndex(&index));
					number = index;
				}
				result->eType = QuadooVM::I4;
				result->lVal = number;
			}
		Cleanup:
			return hr;
		}
		STDMETHOD(SetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* value)
		{
			HRESULT hr;
			PCWSTR text = RStrToWide(name);
			INT number;
			CheckIf(!text || !value,E_POINTER);
			if(!lstrcmpW(text,L"Text"))
			{
				CheckIf(value->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				Check(m_panel->SetText(value->rstrVal));
			}
			else
			{
				CheckIf(lstrcmpW(text,L"Width") && lstrcmpW(text,L"Icon"),DISP_E_MEMBERNOTFOUND);
				Check(Index(value,!lstrcmpW(text,L"Icon") ? -1 : 0,&number));
				Check(!lstrcmpW(text,L"Width") ? m_panel->SetWidth(number) : m_panel->SetIcon(number));
			}
		Cleanup:
			return hr;
		}
	};
	static HRESULT WrapPanel(IQuadooStatusPanel* panel, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = S_OK;
		TStackRef<CStatusPanelObject> object;
		object.Attach(__new CStatusPanelObject(panel));
		CheckAlloc(object);
		result->eType = QuadooVM::Object;
		result->pObject = object.Detach();
	Cleanup:
		return hr;
	}
	class CStatusPanelsObject : public CBaseUnknown, public CQuadooObjectImpl
	{
		TStackRef<IQuadooStatusPanels> m_panels;
		HRESULT Resolve(QuadooVM::QVARIANT* value, IQuadooStatusPanel** panel)
		{
			HRESULT hr;
			INT index;
			if(value->eType == QuadooVM::Object)
			{
				TStackRef<IStatusPanelObject> object;
				Check(value->pObject->QueryInterface(&object));
				Check(object->GetPanel(panel));
			}
			else
			{
				Check(Index(value,0,&index));
				Check(m_panels->GetAt(index,panel));
			}
		Cleanup:
			return hr;
		}
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
		END_UNK_MAP
		CStatusPanelsObject(IQuadooStatusPanels* panels) : m_panels(panels)
		{
		}
		STDMETHOD(GetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			UINT count;
			CheckIf(!name || !result,E_POINTER);
			result->eType = QuadooVM::Null;
			CheckIf(lstrcmpW(RStrToWide(name),L"Count"),DISP_E_MEMBERNOTFOUND);
			Check(m_panels->GetCount(&count));
			result->eType = QuadooVM::I4;
			result->lVal = count;
		Cleanup:
			return hr;
		}
		STDMETHOD(GetIndexedProperty)(IQuadooVM*, RSTRING, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			INT number;
			TStackRef<IQuadooStatusPanel> panel;
			CheckIf(!index || !result,E_POINTER);
			result->eType = QuadooVM::Null;
			Check(Index(index,0,&number));
			Check(m_panels->GetAt(number,&panel));
			Check(WrapPanel(panel,result));
		Cleanup:
			return hr;
		}
		STDMETHOD(Invoke)(IQuadooVM*, RSTRING name, QuadooVM::QVPARAMS* args, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			PCWSTR method = RStrToWide(name);
			TStackRef<IQuadooStatusPanel> panel;
			INT index;
			CheckIf(!method || !args || !result,E_POINTER);
			result->eType = QuadooVM::Null;
			if(!lstrcmpW(method,L"Add") || !lstrcmpW(method,L"Insert"))
			{
				BOOL add = !lstrcmpW(method,L"Add");
				CheckIf(args->cArgs != (add ? 0 : 1),DISP_E_BADPARAMCOUNT);
				if(add)
				{
					UINT count;
					Check(m_panels->GetCount(&count));
					index = count;
				}
				else
				{
					Check(Index(args->pqvArgs,0,&index));
				}
				Check(m_panels->Insert(index,&panel));
				Check(WrapPanel(panel,result));
			}
			else if(!lstrcmpW(method,L"GetAt") || !lstrcmpW(method,L"GetItem"))
			{
				CheckIf(args->cArgs != 1,DISP_E_BADPARAMCOUNT);
				Check(Index(args->pqvArgs,0,&index));
				Check(m_panels->GetAt(index,&panel));
				Check(WrapPanel(panel,result));
			}
			else if(!lstrcmpW(method,L"Delete") || !lstrcmpW(method,L"Move"))
			{
				BOOL move = !lstrcmpW(method,L"Move");
				CheckIf(args->cArgs != (move ? 2 : 1),DISP_E_BADPARAMCOUNT);
				Check(Resolve(args->pqvArgs + args->cArgs - 1,&panel));
				if(move)
				{
					Check(Index(args->pqvArgs,0,&index));
					Check(m_panels->Move(panel,index));
				}
				else
				{
					Check(m_panels->Delete(panel));
				}
			}
			else
			{
				CheckIf(lstrcmpW(method,L"Clear"),DISP_E_MEMBERNOTFOUND);
				CheckIf(args->cArgs,DISP_E_BADPARAMCOUNT);
				Check(m_panels->Clear());
			}
		Cleanup:
			return hr;
		}
	};
	HRESULT WrapStatusPanels(IQuadooStatusPanels* panels, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = S_OK;
		TStackRef<CStatusPanelsObject> object;
		object.Attach(__new CStatusPanelsObject(panels));
		CheckAlloc(object);
		result->eType = QuadooVM::Object;
		result->pObject = object.Detach();
	Cleanup:
		return hr;
	}
}
