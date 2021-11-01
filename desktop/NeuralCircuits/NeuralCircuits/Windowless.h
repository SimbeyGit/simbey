#pragma once

interface IBaseContainer;
interface IAccessible;

interface IWindowless : public IUnknown
{
	virtual VOID WINAPI AttachContainer (IBaseContainer* lpContainer) = 0;
	virtual VOID WINAPI Paint (HDC hdc) = 0;
	virtual VOID WINAPI Move (LPRECT lpPosition) = 0;
	virtual VOID WINAPI SizeObject (INT nWidth, INT nHeight) = 0;
	virtual BOOL WINAPI OnMessage (UINT msg, WPARAM wParam, LPARAM lParam, __out LRESULT& lResult) = 0;
	virtual HRESULT WINAPI GetAccObject (IAccessible** lplpAccessible) = 0;
};