#include "Packages.h"
#include "PackagesResource.h"
#include "Library\ChooseFile.h"

namespace QFP
{
class CPackagesDialog
{
	HWND m_window;
	CCatalog* m_catalog;
	IHost* m_host;
	HIMAGELIST m_images;
	INT Selection() { return ListView_GetNextItem(GetDlgItem(m_window,IDC_QF_PACKAGE_LIST),-1,LVNI_SELECTED); }
	VOID Error(HRESULT hr,PCWSTR action)
	{
		CRString message;
		if(SUCCEEDED(RStrFormatW(&message,L"%ls failed (0x%.8x). Check the package format, conflicting type names, file permissions, and whether a designer is using the package.",action,hr))) MessageBoxW(m_window,message.c_wstr(),L"Control Packages",MB_OK | MB_ICONERROR);
	}
	VOID Detail()
	{
		INT selected=Selection(); const Entry* entry=selected<0 ? NULL : m_catalog->At(selected); CRString text,usage;
		BOOL allowed=FALSE;
		if(entry)
		{
			HRESULT hr=m_catalog->CanRemove(selected,m_host,usage); allowed=hr==S_OK;
			if(FAILED(hr) && !RStrLen(usage)) RStrFormatW(&usage,L"Cannot verify project usage (0x%.8x). Close or repair unreadable definitions before removing packages.",hr);
			RStrFormatW(&text,L"%ls\r\n%ls\r\n%ls",entry->definition ? entry->definition->description.c_wstr() : entry->diagnostic.c_wstr(),usage.c_wstr(),entry->path.c_wstr());
		}
		else RStrFormatW(&text,L"Install a JSON control package. Installed metadata is shared by Quadoo Studio and QuadooForm Designer.\r\n%ls",m_catalog->Directory());
		SetDlgItemTextW(m_window,IDC_QF_PACKAGE_DETAIL,text.c_wstr()); EnableWindow(GetDlgItem(m_window,IDC_QF_PACKAGE_REMOVE),allowed);
	}
	HRESULT Populate()
	{
		HRESULT hr=S_OK; HWND list=GetDlgItem(m_window,IDC_QF_PACKAGE_LIST); HIMAGELIST images=NULL; HICON icon=NULL;
		HDC dc=GetDC(m_window); UINT dpi=dc ? GetDeviceCaps(dc,LOGPIXELSX) : 96; if(dc) ReleaseDC(m_window,dc); UINT pixels=MulDiv(20,dpi,96);
		images=ImageList_Create(pixels,pixels,ILC_COLOR32 | ILC_MASK,max(1,m_catalog->Count()),4); CheckIf(!images,E_OUTOFMEMORY);
		for(UINT n=0;n<m_catalog->Count();n++)
		{
			const Entry* entry=m_catalog->At(n); HRESULT loaded=entry->definition ? entry->definition->GetIcon(pixels,pixels,&icon) : S_FALSE;
			if(loaded!=S_OK || !icon) icon=static_cast<HICON>(CopyImage(LoadIcon(NULL,IDI_APPLICATION),IMAGE_ICON,pixels,pixels,0));
			CheckIf(!icon,HrEnsureLastError()); CheckIf(ImageList_AddIcon(images,icon)<0,E_OUTOFMEMORY); DestroyIcon(icon); icon=NULL;
		}
		SendMessageW(list,WM_SETREDRAW,FALSE,0); ListView_DeleteAllItems(list); ListView_SetImageList(list,images,LVSIL_SMALL);
		if(m_images) ImageList_Destroy(m_images); m_images=images; images=NULL;
		for(UINT n=0;n<m_catalog->Count();n++)
		{
			const Entry* entry=m_catalog->At(n); LVITEMW item; ZeroMemory(&item,sizeof(item)); item.mask=LVIF_TEXT | LVIF_IMAGE; item.iItem=n; item.iImage=n; item.pszText=const_cast<PWSTR>(entry->name.c_wstr());
			CheckIf(ListView_InsertItem(list,&item)<0,E_OUTOFMEMORY);
			ListView_SetItemText(list,n,1,const_cast<PWSTR>(entry->type.c_wstr()));
			ListView_SetItemText(list,n,2,const_cast<PWSTR>(entry->definition ? (entry->definition->visual ? (entry->definition->container ? L"Container" : L"Visual") : L"Nonvisual") : L"Unknown"));
			ListView_SetItemText(list,n,3,const_cast<PWSTR>(SUCCEEDED(entry->status) ? L"Installed" : entry->diagnostic.c_wstr()));
		}
		if(m_catalog->Count()) ListView_SetItemState(list,0,LVIS_SELECTED | LVIS_FOCUSED,LVIS_SELECTED | LVIS_FOCUSED);
	Cleanup:
		SendMessageW(list,WM_SETREDRAW,TRUE,0); InvalidateRect(list,NULL,TRUE); if(icon) DestroyIcon(icon); if(images) ImageList_Destroy(images); Detail(); return hr;
	}
	INT_PTR Message(UINT message,WPARAM wp,LPARAM lp)
	{
		switch(message)
		{
		case WM_INITDIALOG:
			{
				HWND list=GetDlgItem(m_window,IDC_QF_PACKAGE_LIST); ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
				static PCWSTR const labels[]={L"Package",L"Control type",L"Kind",L"Status"}; INT widths[]={150,140,90,210};
				HDC dc=GetDC(m_window); INT dpi=dc ? GetDeviceCaps(dc,LOGPIXELSX) : 96; if(dc) ReleaseDC(m_window,dc);
				for(UINT n=0;n<4;n++) { LVCOLUMNW column; ZeroMemory(&column,sizeof(column)); column.mask=LVCF_TEXT | LVCF_WIDTH; column.pszText=const_cast<PWSTR>(labels[n]); column.cx=MulDiv(widths[n],dpi,96); ListView_InsertColumn(list,n,&column); }
				HRESULT hr=Populate(); if(FAILED(hr)) Error(hr,L"Read installed packages"); return TRUE;
			}
		case WM_NOTIFY: if(reinterpret_cast<NMHDR*>(lp)->idFrom==IDC_QF_PACKAGE_LIST && reinterpret_cast<NMHDR*>(lp)->code==LVN_ITEMCHANGED) Detail(); return FALSE;
		case WM_COMMAND:
			if(LOWORD(wp)==IDOK || LOWORD(wp)==IDCANCEL) { EndDialog(m_window,IDOK); return TRUE; }
			if(LOWORD(wp)==IDC_QF_PACKAGE_INSTALL)
			{
				CChooseFile chooser; HRESULT hr=chooser.Initialize();
				if(SUCCEEDED(hr) && chooser.OpenSingleFile(m_window,L"Install control package",L"Control package JSON (*.json)\0*.json\0\0")) { hr=m_catalog->Install(chooser.GetFile(0),m_host); if(SUCCEEDED(hr)) hr=Populate(); }
				if(FAILED(hr)) Error(hr,L"Install package"); return TRUE;
			}
			if(LOWORD(wp)==IDC_QF_PACKAGE_REMOVE)
			{
				INT selected=Selection(); if(selected>=0) { HRESULT hr=m_catalog->Remove(selected,m_host); if(SUCCEEDED(hr)) hr=Populate(); if(FAILED(hr)) Error(hr,L"Remove package"); Detail(); } return TRUE;
			}
			break;
		case WM_DESTROY: ListView_SetImageList(GetDlgItem(m_window,IDC_QF_PACKAGE_LIST),NULL,LVSIL_SMALL); if(m_images) { ImageList_Destroy(m_images); m_images=NULL; } break;
		}
		return FALSE;
	}
	static INT_PTR CALLBACK Procedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
	{
		CPackagesDialog* self=reinterpret_cast<CPackagesDialog*>(GetWindowLongPtrW(window,DWLP_USER));
		if(message==WM_INITDIALOG) { self=reinterpret_cast<CPackagesDialog*>(lp); self->m_window=window; SetWindowLongPtrW(window,DWLP_USER,reinterpret_cast<LONG_PTR>(self)); }
		return self ? self->Message(message,wp,lp) : FALSE;
	}
public:
	CPackagesDialog(CCatalog* catalog,IHost* host) : m_window(NULL),m_catalog(catalog),m_host(host),m_images(NULL) {}
	HRESULT Show(HWND owner,HINSTANCE instance) { INT_PTR result=DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_QF_PACKAGES),owner,Procedure,reinterpret_cast<LPARAM>(this)); return result==-1 ? HrEnsureLastError() : S_OK; }
};
HRESULT ShowPackages(HWND owner,HINSTANCE instance,CCatalog* catalog,IHost* host)
{
	HRESULT hr; CheckIf(!catalog,E_POINTER); Check(catalog->Refresh(host));
	{ CPackagesDialog dialog(catalog,host); Check(dialog.Show(owner,instance)); }
Cleanup: return hr;
}
}
