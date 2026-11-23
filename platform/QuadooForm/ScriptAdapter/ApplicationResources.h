#pragma once

#include <windows.h>
#include "Library\Core\BaseUnknown.h"
#include "Library\Core\MemoryStream.h"
#include "Library\Util\RString.h"
#include "Published\JSON.h"

namespace AQVM
{
	const UINT CatalogVersion = 2;
	const UINT ByteCodeResource = 101;
	const UINT CatalogResource = 104;
	const UINT TemplateResource = 105;
	const DWORD TemplateSignature = 0x4151564D;
	const UINT MaximumUnits = 4096;
	const DWORD MaximumResourceBytes = 32 * 1024 * 1024;
	const WCHAR CatalogType[] = L"QAPPLICATION";
	const WCHAR FormType[] = L"QFORM";
	const WCHAR FrameType[] = L"QFRAME";
	const WCHAR RootProperty[] = L"__QuadooRoot";
	const WCHAR DestroyBinding[] = L"@AQVM.RootDestroy";

	HRESULT StringField(IJSONObject* object, PCWSTR name, RSTRING* value);
	HRESULT PutString(IJSONObject* object, PCWSTR name, RSTRING value);
	HRESULT PutInt(IJSONObject* object, PCWSTR name, INT value);
	HRESULT ParseObject(PCWSTR text, INT length, IJSONObject** object);
	HRESULT ResourceObject(HINSTANCE module, PCWSTR type, PCWSTR name, IJSONObject** object);
	HRESULT CopyObject(IJSONObject* source, IJSONObject** object);
	HRESULT FindUnit(IJSONObject* catalog, RSTRING className, IJSONObject** unit);
	HRESULT ValidateCatalog(IJSONObject* catalog);
	HRESULT ClassResourceName(RSTRING className, RSTRING* name);
	HRESULT GenerateStartup(RSTRING startupClass, RSTRING* source);
}
