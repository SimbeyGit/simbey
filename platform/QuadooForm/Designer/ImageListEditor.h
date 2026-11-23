#pragma once
#include "Designer.h"
#include "..\Imaging\ImageListData.h"

namespace QFD
{
	// The dialog edits this private snapshot. Only Apply publishes a replacement
	// definition through the ordinary document/source transaction.
	class CImageListEditSession : public CBaseUnknown
	{
		CDesignerEditor* m_editor;
		GUID m_id;
		IJSONObject* m_baseline;
	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IUnknown)
		END_UNK_MAP
		QFI::CImageListData* data;
		BOOL committed;
		CImageListEditSession();
		~CImageListEditSession();
		HRESULT Initialize(CDesignerEditor* editor, REFGUID id);
		HRESULT Properties(IJSONObject** properties);
		HRESULT Import(PCWSTR const* paths, UINT count, UINT sizing, const BOOL* split, BOOL key, INT replace = -1);
		HRESULT Apply();
	};
}
