#pragma once

#include "Library\Window\BaseDialog.h"
#include "Library\Core\MemoryStream.h"
#include "Published\JSON.h"

namespace StudioApplication
{
	enum ModuleArchitecture
	{
		ModulesX86,
		ModulesX64,
		ModuleArchitectureCount
	};

	HRESULT DecodeApplicationIcon(IJSONObject* project, CMemoryStream& bytes);
	ModuleArchitecture CurrentModuleArchitecture();
	HRESULT ReadModuleSettings(IJSONObject* project, ModuleArchitecture architecture, IJSONArray** modules);
	HRESULT WrapModuleSettings(IJSONArray* x86, IJSONArray* x64, IJSONValue** modules);
	HRESULT ResolveModulePath(RSTRING directory, RSTRING path, CRString& absolute, CRString& name);
}

class CApplicationSettingsDlg : public CBaseDialog
{
private:
	TStackRef<IJSONObject> m_project;
	TStackRef<IJSONArray> m_modules[StudioApplication::ModuleArchitectureCount];
	StudioApplication::ModuleArchitecture m_architecture;
	CRString m_directory;
	CRString m_iconData;
	HICON m_icon;
	HRESULT m_error;

public:
	CApplicationSettingsDlg(IJSONObject* project, RSTRING directory);
	~CApplicationSettingsDlg();
	HRESULT LoadIconFile(PCWSTR path);
	HRESULT AddModule(PCWSTR path);
	RSTRING IconData() const;
	IJSONArray* Modules() const;
	HRESULT WrapModules(IJSONValue** modules) const;
	HRESULT Error() const;
	virtual BOOL DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);

private:
	HRESULT Initialize();
	HRESULT ShowModules(StudioApplication::ModuleArchitecture architecture);
	HRESULT InsertModule(RSTRING path, INT index);
	HRESULT BrowseIcon();
	HRESULT BrowseModules();
	HRESULT RemoveModule();
	VOID ClearIcon();
	VOID UpdateButtons();
	VOID ReportError(HRESULT error);
};
