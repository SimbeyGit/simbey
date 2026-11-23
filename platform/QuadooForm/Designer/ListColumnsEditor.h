#pragma once
#include "Designer.h"

namespace QFD
{
	class CListColumnsEditSession
	{
		CDesignerEditor* m_editor;
		CDesignerDocument m_work;
		GUID m_id;
		HWND m_workspace;
		TStackRef<IQuadooComponent> m_component;
		TStackRef<IJSONObject> m_baseline;
	public:
		TStackRef<IQuadooListView> view;
		TStackRef<IQuadooListColumns> columns;
		CListColumnsEditSession();
		~CListColumnsEditSession();
		HRESULT Initialize(CDesignerEditor* editor, REFGUID id);
		HRESULT Properties(IJSONObject** properties);
		HRESULT Apply();
	};
}
