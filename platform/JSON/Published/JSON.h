#pragma once

#include "..\..\..\Shared\Library\Core\IObjectCollection.h"
#include "..\..\..\Shared\Library\Util\RString.h"

#define	JSONAPI	WINAPI

interface IPreProcessorReportError;
interface IJSONValue;

namespace JSON
{
	enum Type
	{
		String,
		Object,
		Boolean,
		Integer,
		LongInteger,
		Double,
		Float,
		Array
	};
};

namespace AddFromObject
{
	enum Options
	{
		Default = 0,
		Optional = 1,
		NotNull = 2,
		NoOverwrite = 4
	};
}

namespace CopyObjects
{
	enum Options
	{
		Default = 0,
		Remove = 1,
		IgnoreNonObjects = 2
	};
}

interface __declspec(uuid("C1CC5B0E-1210-481a-BC08-B56B77B5A421")) IJSONObject : IUnknown
{
	virtual sysint STDMETHODCALLTYPE Count (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetValueName (sysint nValue, __out RSTRING* prstrName) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetValueName (sysint nValue, RSTRING rstrName) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetValueByIndex (sysint nValue, __deref_out_opt IJSONValue** ppValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetValueByIndex (sysint nValue, IJSONValue* pValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddValue (RSTRING rstrName, __in_opt IJSONValue* pValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddValueW (PCWSTR pcwzName, __in_opt IJSONValue* pValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveValue (RSTRING rstrName, __deref_opt_out_opt IJSONValue** ppValue = NULL) = 0;
	virtual HRESULT STDMETHODCALLTYPE RemoveValueW (PCWSTR pcwzName, __deref_opt_out_opt IJSONValue** ppValue = NULL) = 0;
	virtual HRESULT STDMETHODCALLTYPE FindValue (RSTRING rstrName, __deref_out_opt IJSONValue** ppValue) const = 0;
	virtual HRESULT STDMETHODCALLTYPE FindValueW (PCWSTR pcwzName, __deref_out_opt IJSONValue** ppValue) const = 0;
	virtual HRESULT STDMETHODCALLTYPE FindNonNullValue (RSTRING rstrName, __deref_out IJSONValue** ppValue) const = 0;
	virtual HRESULT STDMETHODCALLTYPE FindNonNullValueW (PCWSTR pcwzName, __deref_out IJSONValue** ppValue) const = 0;
	virtual HRESULT STDMETHODCALLTYPE Compact (VOID) = 0;
	virtual bool STDMETHODCALLTYPE HasField (RSTRING rstrName) const = 0;
};

interface __declspec(uuid("ACAA6168-DEB5-4d80-8BE5-A3622F5383C2")) IJSONArray : IUnknown
{
	virtual sysint STDMETHODCALLTYPE Count (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE Add (__in_opt IJSONValue* pvItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE Insert (sysint idxInsert, __in_opt IJSONValue* pvItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE Remove (sysint nItem, __deref_out_opt IJSONValue** ppvItem = NULL) = 0;
	virtual HRESULT STDMETHODCALLTYPE Replace (sysint nItem, __in_opt IJSONValue* pvItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetValue (sysint nItem, __deref_out_opt IJSONValue** ppvItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetObject (sysint nItem, __deref_out IJSONObject** ppObject) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetString (sysint nItem, __deref_out_opt RSTRING* prstrValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE Clear (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE Compact (VOID) = 0;
};

interface __declspec(uuid("817FAB75-C7A0-4894-B221-3132B34AC5AB")) IJSONValue : IUnknown
{
	virtual JSON::Type STDMETHODCALLTYPE GetType (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetString (__out RSTRING* prstrValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetStringAsGuid (__out GUID& guid) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetObject (__deref_out IJSONObject** ppObject) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetArray (__deref_out IJSONArray** ppArray) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetArrayLength (__out sysint* pcArray) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetArrayItem (sysint nItem, __deref_out_opt IJSONValue** ppValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetBoolean (__out bool* pfValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetInteger (__out int* pnValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetLongInteger (__out __int64* pnLongValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDouble (__out double* pdblValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFloat (__out float* pfltValue) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDWord (__out DWORD* pdwValue) = 0;
};

interface __declspec(uuid("0BD194B5-E22A-4f6e-935B-4328C8B7C67F")) IJSONDictionary : IUnknown
{
	virtual sysint STDMETHODCALLTYPE Length (VOID) = 0;
	virtual HRESULT STDMETHODCALLTYPE Get (sysint idx, RSTRING* prstrItem) = 0;
	virtual bool STDMETHODCALLTYPE Has (RSTRING rstrItem) = 0;
	virtual HRESULT STDMETHODCALLTYPE Add (RSTRING rstrItem) = 0;
};

// Core JSON parsing, serialization, and creation functions
HRESULT JSONAPI JSONParse (__in_opt IPreProcessorReportError* pReport, PCWSTR pcwzJSON, INT cchJSON, __deref_out_opt IJSONValue** ppvJSON);
HRESULT JSONAPI JSONParseWithDictionary (__in_opt IJSONDictionary* pJSONDictionary, __in_opt IPreProcessorReportError* pReport, PCWSTR pcwzJSON, INT cchJSON, __deref_out_opt IJSONValue** ppvJSON);
HRESULT JSONAPI JSONCreateDictionary (__deref_out IJSONDictionary** ppJSONDictionary);
HRESULT JSONAPI JSONSerialize (__in_opt IJSONValue* pvJSON, __out ISequentialStream* pstmJSON);
HRESULT JSONAPI JSONSerializeObject (IJSONObject* pObject, __out ISequentialStream* pstmJSON);
HRESULT JSONAPI JSONSerializeArray (IJSONArray* pArray, __out ISequentialStream* pstmJSON);
INT JSONAPI JSONCompareObjects (IJSONObject* pObjectA, IJSONObject* pObjectB);
INT JSONAPI JSONCompareArrays (IJSONArray* pArrayA, IJSONArray* pArrayB);
INT JSONAPI JSONCompare (__in_opt IJSONValue* pvA, __in_opt IJSONValue* pvB);
HRESULT JSONAPI JSONClone (__in_opt IJSONValue* pvJSON, __deref_out IJSONValue** ppvClone, BOOL fDeepClone);
HRESULT JSONAPI JSONCloneObject (IJSONObject* pObject, __deref_out IJSONValue** ppvClone, BOOL fDeepClone);
HRESULT JSONAPI JSONCloneArray (IJSONArray* pArray, __deref_out IJSONValue** ppvClone, BOOL fDeepClone);
HRESULT JSONAPI JSONWrapObject (IJSONObject* pObject, __deref_out IJSONValue** ppvJSON);
HRESULT JSONAPI JSONWrapArray (IJSONArray* pArray, __deref_out IJSONValue** ppvJSON);
HRESULT JSONAPI JSONCreateObject (__deref_out IJSONObject** ppObject);
HRESULT JSONAPI JSONCreateArray (__deref_out IJSONArray** ppArray);
HRESULT JSONAPI JSONCreateString (RSTRING rstrString, __deref_out IJSONValue** ppvString);
HRESULT JSONAPI JSONCreateStringW (PCWSTR pcwzString, INT cchString, __deref_out IJSONValue** ppvString);
HRESULT JSONAPI JSONCreateInteger (int nValue, __deref_out IJSONValue** ppvInteger);
HRESULT JSONAPI JSONCreateLongInteger (__int64 nValue, __deref_out IJSONValue** ppvLongInteger);
HRESULT JSONAPI JSONCreateFloat (float fltValue, __deref_out IJSONValue** ppvFloat);
HRESULT JSONAPI JSONCreateDouble (double dblValue, __deref_out IJSONValue** ppvDouble);
HRESULT JSONAPI JSONFindArrayObjectIndirect (IJSONValue* pvArray, RSTRING rstrField, RSTRING rstrValue, __deref_out IJSONObject** ppObject, __out_opt sysint* pnIndex);
HRESULT JSONAPI JSONFindArrayObject (IJSONArray* pArray, RSTRING rstrField, RSTRING rstrValue, __deref_out IJSONObject** ppObject, __out_opt sysint* pnIndex);
HRESULT JSONAPI JSONFindArrayString (IJSONArray* pArray, RSTRING rstrValue, __out_opt sysint* pidxString);
HRESULT JSONAPI JSONMergeObject (IJSONObject* pTarget, IJSONObject* pSource);
HRESULT JSONAPI JSONAddFromObject (IJSONObject* pTarget, RSTRING rstrTarget, IJSONObject* pSource, RSTRING rstrSource, AddFromObject::Options eOptions);
HRESULT JSONAPI JSONAddStringWToObject (IJSONObject* pTarget, RSTRING rstrField, PCWSTR pcwzText, INT cchText);
HRESULT JSONAPI JSONCopyObjects (IJSONArray* pArray, sysint idxStart, RSTRING rstrField, IJSONValue* pvData, IJSONArray* pFound, CopyObjects::Options eOptions);

// Auxiliary path parsing JSON functions
HRESULT JSONAPI JSONGetObject (IJSONValue* pvRoot, PCWSTR pcwzPath, INT cchPath, BOOL fEnsureExists, __deref_out IJSONObject** ppObject, __in_opt IPreProcessorReportError* pReport = NULL);
HRESULT JSONAPI JSONGetValue (IJSONValue* pvRoot, PCWSTR pcwzPath, INT cchPath, __deref_out_opt IJSONValue** ppvJSON, __in_opt IPreProcessorReportError* pReport = NULL);
HRESULT JSONAPI JSONSetValue (IJSONValue* pvRoot, PCWSTR pcwzPath, INT cchPath, __in_opt IJSONValue* pvJSON, __in_opt IPreProcessorReportError* pReport = NULL);
HRESULT JSONAPI JSONRemoveValue (IJSONValue* pvRoot, PCWSTR pcwzPath, INT cchPath, __deref_opt_out_opt IJSONValue** ppvJSON, __in_opt IPreProcessorReportError* pReport = NULL);
HRESULT JSONAPI JSONGetValueFromObject (IJSONObject* pRoot, PCWSTR pcwzPath, INT cchPath, __deref_out_opt IJSONValue** ppvJSON, __in_opt IPreProcessorReportError* pReport = NULL);

// Formatting for readability
HRESULT JSONAPI ReformatJSON (PCWSTR pcwzJSON, INT cchJSON, __out ISequentialStream* pstmFormatted);

// Legacy adapter for IObjectCollectionEx
HRESULT JSONAPI JSONCreateCollectionWrapper (IJSONArray* pArray, __deref_out IObjectCollectionEx** ppCollection);
