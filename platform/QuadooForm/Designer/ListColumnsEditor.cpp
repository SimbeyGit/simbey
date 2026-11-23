#include "ListColumnsEditor.h"

namespace QFD
{
	CListColumnsEditSession::CListColumnsEditSession() : m_editor(NULL), m_id(GUID_NULL), m_workspace(NULL)
	{
	}
	CListColumnsEditSession::~CListColumnsEditSession()
	{
		m_work.Clear();
		if(m_workspace)
		{
			DestroyWindow(m_workspace);
		}
	}
	HRESULT CListColumnsEditSession::Initialize(CDesignerEditor* editor, REFGUID id)
	{
		HRESULT hr;
		TStackRef<IJSONObject> definition;
		CheckIf(!editor || m_editor,E_INVALIDARG);
		m_workspace = CreateWindowExW(0,L"STATIC",L"",WS_CHILD,0,0,1,1,editor->Window(),NULL,GetModuleHandleW(NULL),NULL);
		CheckIf(!m_workspace,HrEnsureLastError());
		Check(editor->Document().Serialize(&definition));
		Check(m_work.Initialize(editor->Document().Registry()));
		Check(m_work.Load(m_workspace,definition));
		Check(m_work.FindComponent(id,&m_component));
		Check(m_component->QueryInterface(__uuidof(IQuadooListView),reinterpret_cast<PVOID*>(&view)));
		Check(view->GetColumns(&columns));
		m_editor = editor;
		m_id = id;
		Check(Properties(&m_baseline));
	Cleanup:
		return hr;
	}
	HRESULT CListColumnsEditSession::Properties(IJSONObject** properties)
	{
		HRESULT hr;
		TStackRef<IJSONObject> result;
		TStackRef<IJSONValue> value;
		CheckIf(!properties,E_POINTER);
		*properties = NULL;
		CheckIf(!m_component,E_UNEXPECTED);
		Check(JSONCreateObject(&result));
		Check(ReadProperty(m_component,L"Columns",&value));
		Check(result->AddValueW(L"Columns",value));
		value.Release();
		// Logical insertion/deletion/movement also migrates existing row cells.
		// Preserve that association in the same source/document transaction.
		// No row editing UI is provided; virtual Items remains an empty array.
		Check(ReadProperty(m_component,L"Items",&value));
		Check(result->AddValueW(L"Items",value));
		*properties = result.Detach();
	Cleanup:
		return hr;
	}
	HRESULT CListColumnsEditSession::Apply()
	{
		HRESULT hr;
		TStackRef<IJSONObject> properties;
		CheckIf(!m_editor,E_UNEXPECTED);
		Check(Properties(&properties));
		CheckIfIgnore(JSONCompareObjects(properties,m_baseline) == 0,S_FALSE);
		Check(m_editor->ApplyComponentProperties(m_id,properties));
		m_baseline = properties;
	Cleanup:
		return hr;
	}

	class CListColumnsDialog
	{
		CDesignerEditor* m_editor;
		GUID m_id;
		CListColumnsEditSession m_session;
		TStackRef<IQuadooListColumn> m_selected;
		HWND m_window;
		HWND m_list;
		HWND m_preview;
		INT m_index;
		UINT m_dpi;
		BOOL m_loading;
		BOOL m_committed;
		HRESULT m_error;
		static LRESULT CALLBACK PreviewProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR)
		{
			if(message == WM_NOTIFY)
			{
				NMHDR* header = reinterpret_cast<NMHDR*>(lp);
				if(header->hwndFrom == ListView_GetHeader(window) &&
					(header->code == HDN_BEGINTRACKW || header->code == HDN_BEGINTRACKA ||
					header->code == HDN_DIVIDERDBLCLICKW || header->code == HDN_DIVIDERDBLCLICKA || header->code == HDN_BEGINDRAG))
				{
					return TRUE;
				}
			}
			if(message == WM_NCDESTROY)
			{
				RemoveWindowSubclass(window,PreviewProcedure,id);
			}
			return DefSubclassProc(window,message,wp,lp);
		}

		HWND Child(PCWSTR type, PCWSTR text, DWORD style, INT id, INT x, INT y, INT width, INT height)
		{
			RECT rect = {x,y,x + width,y + height};
			MapDialogRect(m_window,&rect);
			HWND child = CreateWindowExW(!lstrcmpW(type,L"EDIT") || !lstrcmpW(type,L"LISTBOX") ? WS_EX_CLIENTEDGE : 0,
				type,text,WS_CHILD | WS_VISIBLE | style,rect.left,rect.top,rect.right - rect.left,rect.bottom - rect.top,
				m_window,reinterpret_cast<HMENU>(id),GetModuleHandleW(NULL),NULL);
			if(child)
			{
				SendMessageW(child,WM_SETFONT,SendMessageW(m_window,WM_GETFONT,0,0),FALSE);
			}
			return child;
		}
		VOID Error(HRESULT error)
		{
			CRString text;
			if(SUCCEEDED(RStrFormatW(&text,L"Column edit failed (0x%.8x). Width must be 0 to 1000000 at 96 DPI; display positions must be valid. The first logical column must be left aligned.",error)))
			{
				MessageBoxW(m_window,text.c_wstr(),L"ListView Columns Editor",MB_OK | MB_ICONERROR);
			}
		}
		HRESULT LoadFields()
		{
			HRESULT hr;
			UINT count;
			Check(m_session.columns->GetCount(&count));
			for(UINT id = 811; id <= 814; id++)
			{
				EnableWindow(GetDlgItem(m_window,id),m_selected != NULL);
			}
			EnableWindow(GetDlgItem(m_window,822),m_selected != NULL);
			EnableWindow(GetDlgItem(m_window,823),m_index > 0);
			EnableWindow(GetDlgItem(m_window,824),m_index >= 0 && static_cast<UINT>(m_index + 1) < count);
			EnableWindow(GetDlgItem(m_window,825),count > 0);
			EnableWindow(GetDlgItem(m_window,826),m_selected != NULL);
			EnableWindow(GetDlgItem(m_window,827),m_selected != NULL);
			if(m_selected)
			{
				CRString caption;
				INT width;
				UINT position;
				QuadooForm::ListColumnAlignment alignment;
				Check(m_selected->GetCaption(&caption));
				Check(m_selected->GetWidth(&width));
				Check(m_selected->GetAlignment(&alignment));
				Check(m_selected->GetDisplayIndex(&position));
				SetDlgItemTextW(m_window,811,caption.c_wstr());
				SetDlgItemInt(m_window,812,width,FALSE);
				SendDlgItemMessageW(m_window,813,CB_SETCURSEL,alignment,0);
				EnableWindow(GetDlgItem(m_window,813),m_index != 0);
				SetDlgItemInt(m_window,814,position,FALSE);
			}
			else
			{
				SetDlgItemTextW(m_window,811,L"");
				SetDlgItemTextW(m_window,812,L"");
				SetDlgItemTextW(m_window,814,L"");
				SendDlgItemMessageW(m_window,813,CB_SETCURSEL,static_cast<WPARAM>(-1),0);
			}
	Cleanup:
		return hr;
		}
		HRESULT SaveFields()
		{
			HRESULT hr;
			BOOL validWidth;
			BOOL validPosition;
			UINT count;
			CRString caption;
			PWSTR write;
			CheckIfIgnore(m_loading || !m_selected,S_OK);
			INT width = GetDlgItemInt(m_window,812,&validWidth,TRUE);
			INT position = GetDlgItemInt(m_window,814,&validPosition,TRUE);
			LRESULT alignment = SendDlgItemMessageW(m_window,813,CB_GETCURSEL,0,0);
			Check(m_session.columns->GetCount(&count));
			CheckIf(!validWidth || width < 0 || width > 1000000 || !validPosition || position < 0 ||
				static_cast<UINT>(position) >= count || alignment < 0 || alignment > 2,E_INVALIDARG);
			INT length = GetWindowTextLengthW(GetDlgItem(m_window,811));
			CheckIf(length > 32767,E_INVALIDARG);
			Check(RStrAllocW(length,&caption,&write));
			GetDlgItemTextW(m_window,811,write,length + 1);
			Check(m_selected->SetCaption(caption));
			Check(m_selected->SetWidth(width));
			Check(m_selected->SetAlignment(static_cast<QuadooForm::ListColumnAlignment>(alignment)));
			Check(m_selected->SetDisplayIndex(position));
	Cleanup:
		return hr;
		}
		HRESULT Rebuild(INT index)
		{
			HRESULT hr;
			UINT count;
			TArray<INT> order;
			m_loading = TRUE;
			SendMessageW(m_list,WM_SETREDRAW,FALSE,0);
			SendMessageW(m_list,LB_RESETCONTENT,0,0);
			m_selected.Release();
			while(Header_GetItemCount(ListView_GetHeader(m_preview)) > 0)
			{
				CheckIf(!ListView_DeleteColumn(m_preview,0),E_FAIL);
			}
			Check(m_session.columns->GetCount(&count));
			CheckIf(count > 4096,E_INVALIDARG);
			for(UINT n = 0; n < count; n++)
			{
				Check(order.Append(0));
			}
			for(UINT n = 0; n < count; n++)
			{
				TStackRef<IQuadooListColumn> column;
				CRString caption;
				CRString label;
				INT width;
				UINT position;
				QuadooForm::ListColumnAlignment alignment;
				Check(m_session.columns->GetAt(n,&column));
				Check(column->GetCaption(&caption));
				Check(column->GetWidth(&width));
				Check(column->GetAlignment(&alignment));
				Check(column->GetDisplayIndex(&position));
				CheckIf(width < 0 || width > 1000000 || alignment < QuadooForm::ListColumnLeft ||
					alignment > QuadooForm::ListColumnRight || !caption || !RStrIsWide(static_cast<RSTRING>(caption)) || RStrLen(caption) > 32767,E_INVALIDARG);
				Check(RStrFormatW(&label,L"%u: %r",n,static_cast<RSTRING>(caption)));
				CheckIf(SendMessageW(m_list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_wstr())) < 0,E_OUTOFMEMORY);
				LVCOLUMNW native = {0};
				static const INT align[] = {LVCFMT_LEFT,LVCFMT_CENTER,LVCFMT_RIGHT};
				native.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
				native.pszText = const_cast<PWSTR>(caption.c_wstr());
				native.cx = MulDiv(width,m_dpi,96);
				native.fmt = align[alignment];
				native.iSubItem = n;
				CheckIf(SendMessageW(m_preview,LVM_INSERTCOLUMNW,n,reinterpret_cast<LPARAM>(&native)) != n,E_OUTOFMEMORY);
				CheckIf(position >= count,E_UNEXPECTED);
				order[position] = n;
			}
			CheckIf(count && !ListView_SetColumnOrderArray(m_preview,count,&order[0]),E_FAIL);
			m_index = count ? max(0,min(index,static_cast<INT>(count) - 1)) : -1;
			if(m_index >= 0)
			{
				Check(m_session.columns->GetAt(m_index,&m_selected));
			}
			SendMessageW(m_list,LB_SETCURSEL,m_index,0);
			Check(LoadFields());
	Cleanup:
			m_loading = FALSE;
			SendMessageW(m_list,WM_SETREDRAW,TRUE,0);
			InvalidateRect(m_list,NULL,TRUE);
			return hr;
		}
		HRESULT Action(UINT command)
		{
			HRESULT hr;
			UINT count;
			INT index = m_index;
			Check(SaveFields());
			Check(m_session.columns->GetCount(&count));
			if(command == IDOK || command == 0x3021)
			{
				Check(m_session.Apply());
				if(hr != S_FALSE)
				{
					m_committed = TRUE;
				}
				if(command == IDOK)
				{
					EndDialog(m_window,IDOK);
					goto Cleanup;
				}
			}
			else if(command == 821)
			{
				CRString caption;
				TStackRef<IQuadooListColumn> added;
				Check(RStrFormatW(&caption,L"Column %u",count + 1));
				Check(m_session.columns->Insert(count,caption,&added));
				index = count;
			}
			else if(command == 825)
			{
				Check(m_session.columns->Clear());
				index = -1;
			}
			else if(command == 822)
			{
				CheckIf(!m_selected,E_INVALIDARG);
				Check(m_session.columns->Delete(m_selected));
			}
			else if(command == 823 || command == 824)
			{
				CheckIf(!m_selected,E_INVALIDARG);
				index += command == 823 ? -1 : 1;
				CheckIf(index < 0 || static_cast<UINT>(index) >= count,E_INVALIDARG);
				Check(m_session.columns->Move(m_selected,index));
			}
			else if(command == 827)
			{
				CheckIf(!m_selected,E_INVALIDARG);
				Check(m_selected->AutoSize(QuadooForm::ListSizeToHeader));
			}
			Check(Rebuild(index));
	Cleanup:
			return hr;
		}
		HRESULT Initialize()
		{
			HRESULT hr;
			SetWindowTextW(m_window,L"ListView Columns Editor");
			HDC dc = GetDC(m_window);
			m_dpi = dc ? GetDeviceCaps(dc,LOGPIXELSX) : 96;
			if(dc)
			{
				ReleaseDC(m_window,dc);
			}
			CheckIf(!Child(L"STATIC",L"Columns (logical order)",0,0,8,8,184,10),HrEnsureLastError());
			m_list = Child(L"LISTBOX",L"",WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,810,8,22,184,148);
			CheckIf(!m_list,HrEnsureLastError());
			CheckIf(!Child(L"STATIC",L"Caption",0,0,202,8,230,10),HrEnsureLastError());
			CheckIf(!Child(L"EDIT",L"",WS_TABSTOP | ES_AUTOHSCROLL,811,202,22,230,14),HrEnsureLastError());
			CheckIf(!Child(L"STATIC",L"Width (96 DPI)",0,0,202,44,80,10),HrEnsureLastError());
			CheckIf(!Child(L"EDIT",L"100",WS_TABSTOP | ES_AUTOHSCROLL,812,202,58,74,14),HrEnsureLastError());
			CheckIf(!Child(L"STATIC",L"Alignment",0,0,286,44,146,10),HrEnsureLastError());
			HWND alignment = Child(L"COMBOBOX",L"",WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,813,286,58,146,80);
			CheckIf(!alignment,HrEnsureLastError());
			static PCWSTR const names[] = {L"Left",L"Center",L"Right"};
			for(UINT n = 0; n < ARRAYSIZE(names); n++)
			{
				CheckIf(SendMessageW(alignment,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(names[n])) < 0,E_OUTOFMEMORY);
			}
			CheckIf(!Child(L"STATIC",L"Display position (zero based)",0,0,202,82,230,10),HrEnsureLastError());
			CheckIf(!Child(L"EDIT",L"0",WS_TABSTOP | ES_AUTOHSCROLL,814,202,96,74,14),HrEnsureLastError());
			CheckIf(!Child(L"BUTTON",L"Apply Column",WS_TABSTOP,826,202,126,108,18),HrEnsureLastError());
			CheckIf(!Child(L"BUTTON",L"Size to Caption",WS_TABSTOP,827,320,126,112,18),HrEnsureLastError());
			CheckIf(!Child(L"STATIC",L"Widths are saved at 96 DPI and scale at runtime.\r\nThe first logical column must be left aligned.",0,0,202,152,230,27),HrEnsureLastError());
			static PCWSTR const actions[] = {L"Add",L"Delete",L"Up",L"Down",L"Clear"};
			for(UINT n = 0; n < ARRAYSIZE(actions); n++)
			{
				CheckIf(!Child(L"BUTTON",actions[n],WS_TABSTOP,821 + n,8 + n * 37,178,34,18),HrEnsureLastError());
			}
			CheckIf(!Child(L"STATIC",L"Report header preview (read only)",0,0,8,204,424,10),HrEnsureLastError());
			m_preview = Child(WC_LISTVIEWW,L"",LVS_REPORT | LVS_NOSORTHEADER,816,8,218,424,32);
			CheckIf(!m_preview,HrEnsureLastError());
			CheckIf(!SetWindowSubclass(m_preview,PreviewProcedure,1,0),HrEnsureLastError());
			ListView_SetUnicodeFormat(m_preview,TRUE);
			CheckIf(!Child(L"BUTTON",L"OK",WS_TABSTOP | BS_DEFPUSHBUTTON,IDOK,242,260,58,18),HrEnsureLastError());
			CheckIf(!Child(L"BUTTON",L"Cancel",WS_TABSTOP,IDCANCEL,308,260,58,18),HrEnsureLastError());
			CheckIf(!Child(L"BUTTON",L"Apply",WS_TABSTOP,0x3021,374,260,58,18),HrEnsureLastError());
			Check(m_session.Initialize(m_editor,m_id));
			Check(Rebuild(0));
	Cleanup:
			return hr;
		}
		static INT_PTR CALLBACK Procedure(HWND window, UINT message, WPARAM wp, LPARAM lp)
		{
			CListColumnsDialog* dialog = reinterpret_cast<CListColumnsDialog*>(GetWindowLongPtrW(window,DWLP_USER));
			if(message == WM_INITDIALOG)
			{
				dialog = reinterpret_cast<CListColumnsDialog*>(lp);
				dialog->m_window = window;
				SetWindowLongPtrW(window,DWLP_USER,lp);
				dialog->m_error = dialog->Initialize();
				if(FAILED(dialog->m_error))
				{
					EndDialog(window,IDCANCEL);
				}
				return TRUE;
			}
			if(!dialog)
			{
				return FALSE;
			}
			if(message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wp) == IDCANCEL))
			{
				EndDialog(window,IDCANCEL);
				return TRUE;
			}
			if(message == WM_COMMAND && !dialog->m_loading)
			{
				HRESULT hr = S_OK;
				UINT command = LOWORD(wp);
				if(command == 810 && HIWORD(wp) == LBN_SELCHANGE)
				{
					INT index = static_cast<INT>(SendMessageW(dialog->m_list,LB_GETCURSEL,0,0));
					hr = dialog->SaveFields();
					if(SUCCEEDED(hr))
					{
						hr = dialog->Rebuild(index);
					}
					else
					{
						SendMessageW(dialog->m_list,LB_SETCURSEL,dialog->m_index,0);
					}
				}
				else if(HIWORD(wp) == BN_CLICKED && (command == IDOK || command == 0x3021 ||
					(command >= 821 && command <= 827)))
				{
					hr = dialog->Action(command);
				}
				if(FAILED(hr))
				{
					dialog->Error(hr);
				}
			}
			return FALSE;
		}
	public:
		CListColumnsDialog(CDesignerEditor* editor, REFGUID id) : m_editor(editor), m_id(id),
			m_window(NULL), m_list(NULL), m_preview(NULL), m_index(-1), m_dpi(96),
			m_loading(FALSE), m_committed(FALSE), m_error(S_OK)
		{
		}
		HRESULT Show(HWND owner)
		{
			HRESULT hr;
			#pragma pack(push,2)
			struct EmptyDialog
			{
				DLGTEMPLATE dialog;
				WORD menu;
				WORD windowClass;
				WORD title;
				WORD pointSize;
				WCHAR font[14];
			};
			#pragma pack(pop)
			EmptyDialog definition = {0};
			definition.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER | DS_SETFONT;
			definition.dialog.cx = 444;
			definition.dialog.cy = 290;
			definition.pointSize = 8;
			CopyMemory(definition.font,L"MS Shell Dlg",sizeof(L"MS Shell Dlg"));
			INT_PTR answer = DialogBoxIndirectParamW(GetModuleHandleW(NULL),&definition.dialog,owner,Procedure,reinterpret_cast<LPARAM>(this));
			hr = answer == -1 ? HrEnsureLastError() : FAILED(m_error) ? m_error : m_committed ? S_OK : S_FALSE;
			return hr;
		}
	};
	HRESULT CDesignerEditor::EditListColumns()
	{
		HRESULT hr;
		Check(FinishInline(TRUE));
		CheckIf(!CanExecute(ListColumnsEditor),E_ACCESSDENIED);
		{
			CListColumnsDialog dialog(this,m_selected);
			Check(dialog.Show(GetAncestor(m_window,GA_ROOT)));
		}
	Cleanup:
		return hr;
	}
}
