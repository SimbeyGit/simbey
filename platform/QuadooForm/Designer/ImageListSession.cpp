#include "ImageListEditor.h"

namespace QFD
{
	CImageListEditSession::CImageListEditSession() : m_editor(NULL), m_id(GUID_NULL), m_baseline(NULL), data(NULL), committed(FALSE)
	{
	}
	CImageListEditSession::~CImageListEditSession()
	{
		SafeRelease(data);
		SafeRelease(m_baseline);
	}
	HRESULT CImageListEditSession::Initialize(CDesignerEditor* editor, REFGUID id)
	{
		HRESULT hr;
		TStackRef<IQuadooComponent> component;
		TStackRef<IJSONValue> images;
		CRString type;
		CheckIf(!editor,E_INVALIDARG);
		Check(editor->Document().FindComponent(id,&component));
		Check(component->GetTypeName(&type));
		CheckIf(!(type == RSTRING_CAST(L"ImageList")),E_INVALIDARG);
		Check(ReadProperty(component,L"Images",&images));
		Check(QFI::CImageListData::Create(images,IntProperty(component,L"Width"),IntProperty(component,L"Height"),IntProperty(component,L"ColorDepth"),BoolProperty(component,L"Masked"),&data));
		m_editor = editor;
		m_id = id;
		Check(Properties(&m_baseline));
	Cleanup:
		return hr;
	}
	HRESULT CImageListEditSession::Properties(IJSONObject** properties)
	{
		HRESULT hr;
		TStackRef<IJSONObject> result;
		TStackRef<IJSONValue> images;
		TStackRef<IJSONValue> mask;
		CheckIf(!properties || !data,E_INVALIDARG);
		*properties = NULL;
		Check(JSONCreateObject(&result));
		Check(PutInt(result,L"Width",data->width));
		Check(PutInt(result,L"Height",data->height));
		Check(PutInt(result,L"ColorDepth",data->depth));
		Check(JSONCreateBool(data->masked != FALSE,&mask));
		Check(result->AddValueW(L"Masked",mask));
		Check(data->Images(&images));
		Check(result->AddValueW(L"Images",images));
		*properties = result.Detach();
	Cleanup:
		return hr;
	}
	HRESULT CImageListEditSession::Import(PCWSTR const* paths, UINT count, UINT sizing, const BOOL* split, BOOL key, INT replace)
	{
		HRESULT hr;
		TStackRef<IJSONValue> snapshot;
		TStackRef<IJSONArray> images;
		TStackRef<QFI::CImageListData> candidate;
		CheckIf(!data || !paths || !count || count > QFI::MaxImageListCount || replace < -1,E_INVALIDARG);
		CheckIf(replace >= 0 && (count != 1 || static_cast<UINT>(replace) >= data->Count()),E_INVALIDARG);
		Check(data->Images(&snapshot));
		Check(snapshot->GetArray(&images));
		if(replace < 0)
		{
			Check(QFI::CImageListData::Create(snapshot,data->width,data->height,data->depth,data->masked,&candidate));
		}
		for(UINT n = 0; n < count; n++)
		{
			TStackRef<IJSONArray> imported;
			Check(QFI::ImportImageListFile(paths[n],data->width,data->height,sizing,split && split[n],key,&imported));
			CheckIf(replace >= 0 && imported->Count() != 1,E_INVALIDARG);
			CheckIf(replace < 0 && candidate->Count() + imported->Count() > QFI::MaxImageListCount,E_INVALIDARG);
			for(sysint k = 0; k < imported->Count(); k++)
			{
				TStackRef<IJSONValue> entry;
				Check(imported->GetValue(k,&entry));
				if(replace >= 0)
				{
					Check(images->Replace(replace,entry));
				}
				else
				{
					// Enforce aggregate limits as each entry is staged, before
					// decoding the remainder of a potentially large batch.
					Check(candidate->Append(entry));
				}
			}
		}
		if(replace >= 0)
		{
			Check(QFI::CImageListData::Create(snapshot,data->width,data->height,data->depth,data->masked,&candidate));
		}
		SafeRelease(data);
		data = candidate.Detach();
	Cleanup:
		return hr;
	}
	HRESULT CImageListEditSession::Apply()
	{
		HRESULT hr;
		TStackRef<IJSONObject> values;
		QFI::Raster preview;
		CheckIf(!m_editor || !data,E_UNEXPECTED);
		Check(Properties(&values));
		CheckIfIgnore(JSONCompareObjects(values,m_baseline) == 0,S_FALSE);
		// Validate all cell conversions before replacing the live document.
		for(UINT n = 0; n < data->Count(); n++)
		{
			Check(data->Render(n,preview));
		}
		Check(m_editor->ApplyComponentProperties(m_id,values));
		ReplaceInterface(m_baseline,static_cast<IJSONObject*>(values));
		committed = TRUE;
	Cleanup:
		return hr;
	}
}
