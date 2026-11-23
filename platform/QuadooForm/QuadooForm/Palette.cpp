#include "Runtime.h"

namespace QF
{
HRESULT LoadPaletteResource(UINT resource, UINT cx, UINT cy, HICON* icon)
{
	HRESULT hr = S_OK;
	CheckIf(!icon,E_POINTER);
	*icon = NULL;
	CheckIf(!cx || !cy || cx > 256 || cy > 256,E_INVALIDARG);
	CheckIf(!resource,HRESULT_FROM_WIN32(ERROR_RESOURCE_NAME_NOT_FOUND));
	*icon = static_cast<HICON>(LoadImageW(g_hInstance,MAKEINTRESOURCEW(resource),IMAGE_ICON,cx,cy,0));
	CheckIf(!*icon,HrEnsureLastError());
Cleanup:
	return hr;
}
STDMETHODIMP CFactory::GetPaletteIcon(UINT cx, UINT cy, HICON* icon)
{
	HRESULT hr;
	CheckIf(!icon,E_POINTER);
	*icon = NULL;
	CheckIf(m_thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
	Check(LoadPaletteResource(m_type->paletteIcon,cx,cy,icon));
Cleanup:
	return hr;
}
}
