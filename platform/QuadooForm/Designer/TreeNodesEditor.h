#pragma once
#include "Designer.h"

namespace QFD
{
	class CTreeEditSession
	{
		CDesignerEditor* m_editor;
		CDesignerDocument m_work;
		GUID m_id;
		HWND m_workspace;
		TStackRef<IQuadooComponent> m_component;
		TStackRef<IJSONObject> m_baseline;
	public:
		TStackRef<IQuadooTreeView> tree;
		TStackRef<IQuadooTreeNodes> nodes;
		CTreeEditSession();
		~CTreeEditSession();
		HRESULT Initialize(CDesignerEditor* editor, REFGUID id);
		HRESULT Properties(IJSONObject** properties);
		HRESULT Apply();
	};
}
