// This extension uses published interfaces only; no QuadooForm runtime bases.
class CScriptMeter : public CMeter, public CQuadooObjectImpl
{
	QuadooVM::QVARIANT m_value;
	INT m_slots[3];
	TStackRef<IQuadooObject> m_payload;
	BOOL m_sawVM;
public:
	IMP_UNKNOWN(CMeter)
	BEGIN_UNK_MAP
		UNK_INTERFACE(IQuadooObject)
		UNK_INTERFACE(IServiceProvider)
		UNK_CHAIN_TO(CMeter)
	END_UNK_MAP
	CScriptMeter(CFactory* factory, IQuadooConstructionContext* context, IQuadooControlContainer* parent) :
		CMeter(factory,context,parent), m_sawVM(FALSE)
	{
		ZeroMemory(&m_value,sizeof(m_value));
		m_value.eType = QuadooVM::I4;
		ZeroMemory(m_slots,sizeof(m_slots));
	}
	STDMETHOD(GetTypeName)(RSTRING* value)
	{
		return RStrCreateW(LSP(L"ExternalScriptMeter"),value);
	}
	STDMETHOD(Invoke)(IQuadooVM* vm, RSTRING name, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = S_OK;
		m_sawVM = vm != NULL;
		CheckIf(!name || lstrcmpW(RStrToWide(name),L"Echo"),DISP_E_MEMBERNOTFOUND);
		CheckIf(parameters->cArgs != 1,DISP_E_BADPARAMCOUNT);
		CheckIf(parameters->pqvArgs[0].eType != QuadooVM::I4,DISP_E_TYPEMISMATCH);
		*result = parameters->pqvArgs[0];
	Cleanup:
		return hr;
	}
	STDMETHOD(GetProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = S_OK;
		PCWSTR text = RStrToWide(name);
		if(!lstrcmpW(text,L"SawVM"))
		{
			result->eType = QuadooVM::Bool;
			result->fVal = m_sawVM != FALSE;
		}
		else if(!lstrcmpW(text,L"NativeValue"))
		{
			m_sawVM = vm != NULL;
			*result = m_value;
		}
		else if(!lstrcmpW(text,L"Payload"))
		{
			result->eType = m_payload ? QuadooVM::Object : QuadooVM::Null;
			if(m_payload)
			{
				result->pObject = m_payload;
				result->pObject->AddRef();
			}
		}
		else
		{
			hr = !lstrcmpW(text,L"Pending") ? E_PENDING : DISP_E_MEMBERNOTFOUND;
		}
		return hr;
	}
	STDMETHOD(SetProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* value)
	{
		HRESULT hr = S_OK;
		m_sawVM = vm != NULL;
		if(!lstrcmpW(RStrToWide(name),L"Payload"))
		{
			CheckIf(value->eType != QuadooVM::Object && value->eType != QuadooVM::Null,DISP_E_TYPEMISMATCH);
			m_payload = value->eType == QuadooVM::Object ? value->pObject : NULL;
		}
		else
		{
			CheckIf(lstrcmpW(RStrToWide(name),L"NativeValue"),DISP_E_MEMBERNOTFOUND);
			CheckIf(value->eType != QuadooVM::I4,DISP_E_TYPEMISMATCH);
			m_value = *value;
		}
	Cleanup:
		return hr;
	}
	STDMETHOD(GetIndexedProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = S_OK;
		m_sawVM = vm != NULL;
		CheckIf(name && lstrcmpW(RStrToWide(name),L"Slots"),DISP_E_MEMBERNOTFOUND);
		CheckIf(index->eType != QuadooVM::I4 || index->lVal < 0 || index->lVal >= ARRAYSIZE(m_slots),DISP_E_BADINDEX);
		result->eType = QuadooVM::I4;
		result->lVal = m_slots[index->lVal];
	Cleanup:
		return hr;
	}
	STDMETHOD(SetIndexedProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* value)
	{
		HRESULT hr = S_OK;
		m_sawVM = vm != NULL;
		CheckIf(name && lstrcmpW(RStrToWide(name),L"Slots"),DISP_E_MEMBERNOTFOUND);
		CheckIf(index->eType != QuadooVM::I4 || index->lVal < 0 || index->lVal >= ARRAYSIZE(m_slots),DISP_E_BADINDEX);
		CheckIf(value->eType != QuadooVM::I4,DISP_E_TYPEMISMATCH);
		m_slots[index->lVal] = value->lVal;
	Cleanup:
		return hr;
	}
	STDMETHOD(GetMemberVariable)(DWORD index, QuadooVM::QVARIANT** value)
	{
		HRESULT hr = S_OK;
		CheckIf(index,DISP_E_BADINDEX);
		*value = &m_value;
	Cleanup:
		return hr;
	}
	STDMETHOD(GetDefaultValue)(QuadooVM::QVARIANT* result)
	{
		*result = m_value;
		return S_OK;
	}
	STDMETHOD(DeleteProperty)(RSTRING name, QuadooVM::QVARIANT* result)
	{
		HRESULT hr = S_FALSE;
		CheckIf(lstrcmpW(RStrToWide(name),L"NativeValue"),DISP_E_MEMBERNOTFOUND);
		if(result)
		{
			*result = m_value;
		}
		m_value.lVal = 0;
	Cleanup:
		return hr;
	}
};
