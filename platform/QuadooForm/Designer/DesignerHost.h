#pragma once
#include <windows.h>
#include "Library\Util\RString.h"
interface IJSONObject;

// Native editor hosting contract. This is separate from the runtime DLL ABI.
namespace QFD
{
// Optional native host service. Standalone editing has no source site.
// Prepare applies a reversible source edit before the native mutation; Finish
// commits it or restores it. No application code runs through this contract.
struct ISourceEdit : IUnknown
{
	virtual HRESULT Finish(BOOL commit) = 0;
};
struct ISourceSite
{
	virtual HRESULT PrepareDefinition(IJSONObject* proposed, ISourceEdit** edit) = 0;
	virtual HRESULT OpenHandler(REFGUID component, RSTRING eventName) = 0;
};
struct EditorHostOptions
{
	UINT cbSize;
	HWND paletteParent;
	HWND notifyWindow;
	UINT_PTR session;
};
enum OutlineFlags { OutlineVisual = 1, OutlinePlaceholder = 2 };
struct OutlineEntry
{
	GUID id, parent;
	CRString name, type;
	DWORD flags;
	OutlineEntry() : flags(0) { ZeroMemory(&id,sizeof(id)); ZeroMemory(&parent,sizeof(parent)); }
};
// ChangedMessage posts the session token in wParam and the outline revision in
// lParam. Hosts validate the token before pulling state. No pointers are posted.
}
