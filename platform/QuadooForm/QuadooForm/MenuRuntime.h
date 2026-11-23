#pragma once
namespace QF
{
	HRESULT MenuProperty(IQuadooComponent* component, PCWSTR name, IJSONValue** value);
	HRESULT MenuInt(IQuadooComponent* component, PCWSTR name, INT* value);
	HRESULT MenuBool(IQuadooComponent* component, PCWSTR name, BOOL* value);
	HRESULT FindComponentID(IQuadooComponentCollection* owner, REFGUID id, IQuadooComponent** component);
	HRESULT MenuChildren(IQuadooComponent* parent, TArray<IQuadooComponent*>& items);
	HRESULT RefreshOwnerMenu(IQuadooComponentCollection* owner);
	HRESULT ParseMenuShortcut(RSTRING text, ACCEL* accelerator);
	HRESULT TranslatePopupShortcuts(IQuadooControl* root, MSG* message, BOOL* handled, UINT depth = 0);
	class CNativeMenu : public CBaseUnknown
	{
		HMENU m_handle;
		HACCEL m_accelerators;
		TArray<IQuadooComponent*> m_items;
		TArray<ACCEL> m_keys;
		HRESULT BuildItems(IQuadooComponent* parent, HMENU menu, BOOL enabled, UINT depth);
	public:
		BEGIN_UNK_MAP
			UNK_INTERFACE_PATH(IUnknown,CBaseUnknown)
		END_UNK_MAP
		CNativeMenu();
		~CNativeMenu();
		HRESULT Build(IQuadooComponent* root, BOOL popup);
		HMENU Handle() const;
		BOOL Translate(HWND window, MSG* message);
		HRESULT Invoke(UINT command);
		BOOL Owns(HMENU menu) const;
		HRESULT InvokePosition(HMENU menu, UINT position);
		HRESULT Opening(HMENU menu);
		HRESULT Synchronize();
	};
	HRESULT FitMenuClient(HWND window, INT width, INT height);
}
