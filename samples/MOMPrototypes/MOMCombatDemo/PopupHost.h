#pragma once

class CSIFPackage;
class CCastSpell;

interface __declspec(uuid("A1408421-26D1-4a3e-A27C-353248AD441D")) IPopupView : IUnknown
{
	virtual VOID Destroy (VOID) = 0;
};

interface __declspec(uuid("48F5ACBB-37B8-444a-AF95-4EECFAA30860")) IPopupHost : IUnknown
{
	virtual HRESULT AddPopup (IPopupView* pView) = 0;
	virtual CSIFPackage* GetPackage (VOID) = 0;
	virtual HRESULT RemovePopup (IPopupView* pView) = 0;
	virtual VOID UpdateMouse (LPARAM lParam) = 0;
	virtual HRESULT AttachSpellCaster (CCastSpell* pSpell) = 0;
};
