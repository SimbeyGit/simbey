#pragma once
#include "Published\CodeEdit.h"
#include "FormProjectFile.h"

namespace StudioSource
{
// TArray is byte-relocatable storage. Keep objects containing CRString in
// individually constructed slots, with ordinary C++ ownership and destruction.
template<class T> class TSourceObjects : public TArray<T*>
{
public:
	~TSourceObjects() { this->DeleteAll(); }
	HRESULT Append(const T& value)
	{
		HRESULT hr;
		T* copy = __new T(value);
			CheckAlloc(copy);
			Check(TArray<T*>::Append(copy));
		copy = NULL;
	Cleanup:
		__delete copy;
		return hr;
	}
	T& operator[](sysint n) { return *TArray<T*>::operator[](n); }
	const T& operator[](sysint n) const { return *TArray<T*>::operator[](n); }
};
struct Edit
{
	INT offset, length;
	CRString text;
	Edit() : offset(0), length(0) {}
};
struct Result
{
	TSourceObjects<Edit> edits; // Descending offsets in the original source.
	TStackRef<IJSONObject> metadata;
	CRString text, diagnostic;
	INT handlerOffset;
	Result() : handlerOffset(-1) {}
};
HRESULT ValidateIdentifier(RSTRING name);
HRESULT Prepare(RSTRING source, RSTRING className, REFGUID unit, IJSONObject* metadata,
	IJSONObject* definition, RSTRING createHandler, Result& result, BOOL repair = FALSE);
HRESULT ReadDocument(ITextDocument* document, RSTRING* text);
VOID FailAfter(INT checkpoint);
HRESULT Checkpoint();
}
