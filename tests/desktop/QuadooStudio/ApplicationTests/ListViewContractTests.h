#include "Published\SimbeyCore.h"

// Test-only proof of the proposed value-holder and live-string facade. It does
// not register a ListView factory or become part of the production adapter.
namespace ListContractProbe
{
	static UINT s_payloads;
	interface __declspec(uuid("D0F5ACB2-4BD9-4817-90B1-C410C89421CE")) IValue : IUnknown
	{
		STDMETHOD(Copy)(QuadooVM::QVARIANT* result) = 0;
	};
	class CValue : public CBaseUnknown, public IValue
	{
		QuadooVM::QVARIANT m_value;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IValue)
		END_UNK_MAP
		CValue(const QuadooVM::QVARIANT* value)
		{
			QVMSetVariant(&m_value,value);
			s_payloads++;
		}
		~CValue()
		{
			QVMClearVariant(&m_value);
			s_payloads--;
		}
		STDMETHOD(Copy)(QuadooVM::QVARIANT* result)
		{
			QVMSetVariant(result,&m_value);
			return S_OK;
		}
	};

	class CItem : public CBaseUnknown, public CQuadooObjectImpl
	{
		TStackRef<IRStringArray> m_strings;
		TStackRef<IUnknown> m_payload;
		BOOL m_alive;
		BOOL m_frozen;
		UINT m_updates;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
		END_UNK_MAP
		CItem() : m_alive(TRUE), m_frozen(FALSE), m_updates(0)
		{
		}
		HRESULT Initialize()
		{
			return ScCreateRStringArray(&m_strings);
		}
		HRESULT Alive()
		{
			return m_alive ? S_OK : QuadooForm::EDestroyed;
		}
		HRESULT Writable()
		{
			return !m_alive ? QuadooForm::EDestroyed : m_frozen ? QuadooForm::EInvalidState : S_OK;
		}
		VOID Freeze()
		{
			m_frozen = TRUE;
		}
		VOID Destroy()
		{
			m_alive = FALSE;
			m_payload.Release();
			m_strings.Release();
		}
		HRESULT Snapshot(IRStringArray** strings)
		{
			HRESULT hr;
			Check(Alive());
			Check(m_strings->Clone(strings));
		Cleanup:
			return hr;
		}
		HRESULT Commit(IRStringArray* strings)
		{
			HRESULT hr;
			INT comparison;
			TStackRef<IRStringArray> candidate;
			Check(Writable());
			Check(strings->Clone(&candidate));
			Check(m_strings->Compare(candidate,&comparison));
			if(comparison)
			{
				m_strings = candidate;
				m_updates++;
			}
		Cleanup:
			return hr;
		}
		HRESULT GetCell(UINT index, RSTRING* text)
		{
			HRESULT hr;
			Check(Alive());
			Check(m_strings->GetRString(index,text));
		Cleanup:
			return hr;
		}
		HRESULT SetCell(UINT index, RSTRING text)
		{
			HRESULT hr;
			TStackRef<IRStringArray> candidate;
			Check(Writable());
			Check(Snapshot(&candidate));
			Check(candidate->Replace(index,text));
			Check(Commit(candidate));
		Cleanup:
			return hr;
		}
		STDMETHOD(GetProperty)(IQuadooVM* vm, RSTRING name, QuadooVM::QVARIANT* result);
		STDMETHOD(SetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* value)
		{
			HRESULT hr;
			Check(Writable());
			CheckIf(!(CRString(name) == RSTRING_CAST(L"Data")),DISP_E_MEMBERNOTFOUND);
			if(value->eType == QuadooVM::Null)
			{
				m_payload.Release();
			}
			else
			{
				CValue* payload = __new CValue(value);
				CheckAlloc(payload);
				m_payload.Attach(static_cast<IValue*>(payload));
			}
			hr = S_OK;
		Cleanup:
			return hr;
		}
		STDMETHOD(GetIndexedProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			CheckIf(!(CRString(name) == RSTRING_CAST(L"SubItems")),DISP_E_MEMBERNOTFOUND);
			CheckIf(index->eType != QuadooVM::I4 || index->lVal < 0,DISP_E_BADINDEX);
			Check(GetCell(index->lVal,&result->rstrVal));
			result->eType = QuadooVM::String;
		Cleanup:
			return hr;
		}
		STDMETHOD(SetIndexedProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* value)
		{
			HRESULT hr;
			CheckIf(!(CRString(name) == RSTRING_CAST(L"SubItems")),DISP_E_MEMBERNOTFOUND);
			CheckIf(index->eType != QuadooVM::I4 || index->lVal < 0,DISP_E_BADINDEX);
			CheckIf(value->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
			Check(SetCell(index->lVal,value->rstrVal));
		Cleanup:
			return hr;
		}
		STDMETHOD(Invoke)(IQuadooVM*, RSTRING name, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			TStackRef<IRStringArray> strings;
			TStackRef<IQuadooArray> array;
			QuadooVM::QVARIANT cell = {QuadooVM::Null};
			Check(Alive());
			if(CRString(name) == RSTRING_CAST(L"Snapshot"))
			{
				CheckIf(parameters->cArgs,DISP_E_BADPARAMCOUNT);
				Check(Snapshot(&strings));
				Check(QVMWrapStringArray(strings,result));
			}
			else if(CRString(name) == RSTRING_CAST(L"Apply"))
			{
				CheckIf(parameters->cArgs != 1 || parameters->pqvArgs->eType != QuadooVM::Object,DISP_E_TYPEMISMATCH);
				Check(parameters->pqvArgs->pObject->QueryInterface(&array));
				Check(ScCreateRStringArray(&strings));
				for(sysint n = 0; n < array->Length(); n++)
				{
					Check(array->GetItem(n,&cell));
					CheckIf(cell.eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
					Check(strings->AddRString(cell.rstrVal));
					QVMClearVariant(&cell);
				}
				Check(Commit(strings));
			}
			else
			{
				Check(DISP_E_MEMBERNOTFOUND);
			}
		Cleanup:
			QVMClearVariant(&cell);
			return hr;
		}
	};

	class CStrings : public CBaseUnknown, public CQuadooObjectImpl
	{
		TStackRef<CItem> m_item;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
		END_UNK_MAP
		CStrings(CItem* item) : m_item(item)
		{
		}
		STDMETHOD(GetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			TStackRef<IRStringArray> strings;
			CheckIf(!(CRString(name) == RSTRING_CAST(L"Count")),DISP_E_MEMBERNOTFOUND);
			Check(m_item->Snapshot(&strings));
			result->eType = QuadooVM::I4;
			result->lVal = static_cast<LONG>(strings->Length());
		Cleanup:
			return hr;
		}
		STDMETHOD(GetIndexedProperty)(IQuadooVM*, RSTRING, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			CheckIf(index->eType != QuadooVM::I4 || index->lVal < 0,DISP_E_BADINDEX);
			Check(m_item->GetCell(index->lVal,&result->rstrVal));
			result->eType = QuadooVM::String;
		Cleanup:
			return hr;
		}
		STDMETHOD(SetIndexedProperty)(IQuadooVM*, RSTRING, QuadooVM::QVARIANT* index, QuadooVM::QVARIANT* value)
		{
			HRESULT hr;
			CheckIf(index->eType != QuadooVM::I4 || index->lVal < 0,DISP_E_BADINDEX);
			CheckIf(value->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
			Check(m_item->SetCell(index->lVal,value->rstrVal));
		Cleanup:
			return hr;
		}
		STDMETHOD(Invoke)(IQuadooVM*, RSTRING name, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			TStackRef<IRStringArray> strings;
			Check(m_item->Writable());
			Check(m_item->Snapshot(&strings));
			result->eType = QuadooVM::Null;
			if(CRString(name) == RSTRING_CAST(L"Add"))
			{
				CheckIf(parameters->cArgs != 1 || parameters->pqvArgs[0].eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				Check(strings->AddRString(parameters->pqvArgs[0].rstrVal));
			}
			else if(CRString(name) == RSTRING_CAST(L"Replace"))
			{
				CheckIf(parameters->cArgs != 2 || parameters->pqvArgs[1].eType != QuadooVM::I4 ||
					parameters->pqvArgs[1].lVal < 0 || parameters->pqvArgs[0].eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				Check(strings->Replace(parameters->pqvArgs[1].lVal,parameters->pqvArgs[0].rstrVal));
			}
			else
			{
				Check(DISP_E_MEMBERNOTFOUND);
			}
			Check(m_item->Commit(strings));
		Cleanup:
			return hr;
		}
	};

	STDMETHODIMP CItem::GetProperty(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
	{
		HRESULT hr;
		Check(Alive());
		result->eType = QuadooVM::Null;
		if(CRString(name) == RSTRING_CAST(L"SubItems"))
		{
			result->pObject = __new CStrings(this);
			CheckAlloc(result->pObject);
			result->eType = QuadooVM::Object;
		}
		else if(CRString(name) == RSTRING_CAST(L"Data"))
		{
			if(m_payload)
			{
				TStackRef<IValue> value;
				Check(m_payload->QueryInterface(&value));
				Check(value->Copy(result));
			}
		}
		else if(CRString(name) == RSTRING_CAST(L"ItemIndex") || CRString(name) == RSTRING_CAST(L"Updates"))
		{
			result->eType = QuadooVM::I4;
			result->lVal = CRString(name) == RSTRING_CAST(L"ItemIndex") ? 7 : m_updates;
		}
		else
		{
			Check(DISP_E_MEMBERNOTFOUND);
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}

	static HRESULT Test(PCWSTR scriptPath)
	{
		HRESULT hr;
		HRESULT registered;
		CMemoryStream code;
		CCompilerStatus compiler;
		TStackRef<IQuadooInstanceLoader> loader;
		TStackRef<IQuadooVM> vm;
		TStackRef<IQuadooObject> exception;
		TStackRef<CItem> item;
		TStackRef<IUnknown> identity;
		TStackRef<IUnknown> otherIdentity;
		TStackRef<IRStringArray> snapshot;
		CRString text;
		QuadooVM::QVARIANT argument = {QuadooVM::Object};
		QuadooVM::QVARIANT result = {QuadooVM::Null};
		QuadooVM::QVARIANT value = {QuadooVM::Null};
		QuadooVM::QVARIANT facade = {QuadooVM::Null};
		QuadooVM::QVARIANT index = {QuadooVM::I4};
		QuadooVM::QVARIANT arraySize = {QuadooVM::I4};
		ULONG ordinal;
		DWORD count;
		item.Attach(__new CItem);
		CheckAlloc(item);
		Check(item->Initialize());
		Check(QuadooParseToStream(scriptPath,0,&code,NULL,&compiler));
		Check(QVMCreateLoader(NULL,&loader));
		Check(loader->AddInstance(RSTRING_CAST(L"ListContract"),NULL,&code,code.DataRemaining(),NULL));
		Check(loader->LoadVM(RSTRING_CAST(L"ListContract"),&registered,&vm));
		Check(vm->RunConstructor(&exception));
		REQUIRE(!exception);
		Check(vm->FindFunction(L"Probe",&ordinal,&count));
		REQUIRE(count == 1);
		argument.pObject = item;
		Check(vm->PushValue(&argument));
		Check(vm->RunFunction(ordinal,&result));
		REQUIRE(result.eType == QuadooVM::I4 && result.lVal == 73);
		REQUIRE(s_payloads == 1);
		Check(item->GetProperty(NULL,RSTRING_CAST(L"Data"),&value));
		REQUIRE(value.eType == QuadooVM::Object);
		Check(value.pObject->GetProperty(NULL,RSTRING_CAST(L"Value"),&result));
		REQUIRE(result.eType == QuadooVM::I4 && result.lVal == 91);
		QVMClearVariant(&value);
		Check(item->Snapshot(&snapshot));
		Check(snapshot->Replace(0,RSTRING_CAST(L"outside")));
		Check(item->GetCell(0,&text));
		REQUIRE(text == RSTRING_CAST(L"independent"));
		Check(item->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&identity)));
		Check(static_cast<IQuadooObject*>(item)->QueryInterface(IID_IUnknown,reinterpret_cast<PVOID*>(&otherIdentity)));
		REQUIRE(static_cast<IUnknown*>(identity) == static_cast<IUnknown*>(otherIdentity));
		arraySize.lVal = 1;
		Check(QVMCreateNewArray(&arraySize,&value));
		Check(item->SetProperty(NULL,RSTRING_CAST(L"Data"),&value));
		QVMClearVariant(&result);
		Check(item->GetProperty(NULL,RSTRING_CAST(L"Data"),&result));
		REQUIRE(result.eType == QuadooVM::Array && result.pArray == value.pArray && s_payloads == 1);
		QVMClearVariant(&value);
		QVMClearVariant(&result);
		value.eType = QuadooVM::I8;
		value.llVal = 0x123456789LL;
		Check(item->SetProperty(NULL,RSTRING_CAST(L"Data"),&value));
		Check(item->GetProperty(NULL,RSTRING_CAST(L"Data"),&result));
		REQUIRE(result.eType == QuadooVM::I8 && result.llVal == value.llVal);
		QVMClearVariant(&result);
		Check(item->GetProperty(NULL,RSTRING_CAST(L"SubItems"),&facade));
		item->Freeze();
		value.eType = QuadooVM::String;
		value.rstrVal = NULL;
		Check(RStrCreateW(LSP(L"rejected"),&value.rstrVal));
		REQUIRE(facade.pObject->SetIndexedProperty(NULL,NULL,&index,&value) == QuadooForm::EInvalidState);
		QVMClearVariant(&value);
		text = NULL;
		Check(item->GetCell(0,&text));
		REQUIRE(text == RSTRING_CAST(L"independent"));
		item->Destroy();
		REQUIRE(s_payloads == 0);
		REQUIRE(facade.pObject->GetProperty(NULL,RSTRING_CAST(L"Count"),&result) == QuadooForm::EDestroyed);
		hr = S_OK;
	Cleanup:
		if(item)
		{
			item->Destroy();
		}
		QVMClearVariant(&facade);
		QVMClearVariant(&value);
		QVMClearVariant(&result);
		if(vm)
		{
			vm->Unload();
		}
		return hr;
	}
}
