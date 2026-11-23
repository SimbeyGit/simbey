#include <windows.h>
#include "QuadooProject.h"
#include "Tabs.h"
#include "QuadooStudio.h"

HRESULT CQuadooProject::GetPackageEditors (TArray<QFD::CDesignerEditor*>& editors)
{
	HRESULT hr = S_OK;
	for(sysint n=0;n<m_mapFiles.Length();n++)
	{
		CProjectFile* file=static_cast<CProjectFile*>(*m_mapFiles.GetValuePtr(n));
		if(file->m_form && file->m_form->editor)
		{
			Check(editors.Append(file->m_form->editor));
		}
	}
Cleanup:
	return hr;
}

HRESULT CQuadooProject::VisitPackageDefinitions(QFP::IDefinitionVisitor* visitor)
{
	HRESULT hr=S_OK;
	PWSTR text=NULL;

	for(sysint n=0;n<m_mapFiles.Length();n++)
	{
		CProjectFile* file=static_cast<CProjectFile*>(*m_mapFiles.GetValuePtr(n));
		if(file->m_form)
		{
			TStackRef<IJSONObject> definition;
			if(file->m_form->editor)
			{
				Check(file->m_form->editor->CommitPropertyEdit());
				Check(file->m_form->editor->Document().Serialize(&definition));
			}
			else
			{
				INT length;

				hr = Text::LoadFromFile(file->m_form->definition.c_wstr(),&text,&length,CP_UTF8);
				if(SUCCEEDED(hr))
					hr = QFD::Parse(text,length,&definition);
				if(FAILED(hr))
				{
					CRString message;
					if(SUCCEEDED(RStrFormatW(&message,L"Cannot verify package usage in %ls (0x%.8x).",file->m_form->definition.c_wstr(),hr)))
						OutputDebugStringW(message.c_wstr());
					Check(visitor->Unavailable(file->m_form->definition,hr));
				}
				SafeDeleteArray(text);
			}
			Check(visitor->Visit(definition,file->m_form->definition));
		}
	}
Cleanup:
	SafeDeleteArray(text);
	return hr;
}

HRESULT CQuadooStudio::VisitDefinitions(QFP::IDefinitionVisitor* visitor)
{
	return m_pProject ? m_pProject->VisitPackageDefinitions(visitor) : S_OK;
}

HRESULT CQuadooStudio::GetEditors(TArray<QFD::CDesignerEditor*>& editors)
{
	return m_pProject ? m_pProject->GetPackageEditors(editors) : S_OK;
}

HRESULT CQuadooStudio::RefreshPackages()
{
	HRESULT hr=S_OK;
	if(m_packages && !m_packagesBusy)
	{
		m_packagesBusy=TRUE;
		hr=m_packages->Refresh(this);
		if(SUCCEEDED(hr))
			hr=m_packages->UpdateUsage(this);
		m_packagesBusy=FALSE;
	}
	return hr;
}
