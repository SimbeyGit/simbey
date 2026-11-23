#include "ImageListEditor.h"
#include "Library\ChooseFile.h"

namespace QFD
{
	class CImageListDialog
	{
		CDesignerEditor* m_editor;
		GUID m_id;
		TStackRef<CImageListEditSession> m_session;
		HWND m_window;
		HWND m_grid;
		INT m_selected;
		INT m_top;
		INT m_drag;
		POINT m_down;
		BOOL m_dragging;
		HRESULT m_error;
		INT CellWidth() const
		{
			RECT cell = {0,0,56,56};
			MapDialogRect(m_window,&cell);
			return cell.right;
		}
		INT CellHeight() const
		{
			RECT cell = {0,0,56,56};
			MapDialogRect(m_window,&cell);
			return cell.bottom;
		}
		INT Columns() const
		{
			RECT client;
			GetClientRect(m_grid,&client);
			return max(1,client.right / CellWidth());
		}
		HWND Child(PCWSTR type, PCWSTR text, DWORD style, INT id, INT x, INT y, INT width, INT height)
		{
			RECT rect = {x,y,x + width,y + height};
			MapDialogRect(m_window,&rect);
			HWND child = CreateWindowExW((!lstrcmpW(type,L"EDIT") ? WS_EX_CLIENTEDGE : 0),type,text,WS_CHILD | WS_VISIBLE | style,
				rect.left,rect.top,rect.right - rect.left,rect.bottom - rect.top,m_window,reinterpret_cast<HMENU>(id),GetModuleHandle(NULL),NULL);
			if(child)
			{
				SendMessageW(child,WM_SETFONT,SendMessageW(m_window,WM_GETFONT,0,0),FALSE);
			}
			return child;
		}
		VOID Error(HRESULT hr)
		{
			CRString text;
			if(SUCCEEDED(RStrFormatW(&text,L"Operation failed (0x%.8x). Use valid BMP, ICO or PNG files within the image limits. No collection changes were committed.",hr)))
			{
				MessageBoxW(m_window,text.c_wstr(),L"Image List Editor",MB_OK | MB_ICONERROR);
			}
		}
		HRESULT Settings()
		{
			HRESULT hr;
			BOOL widthOK;
			BOOL heightOK;
			UINT width = GetDlgItemInt(m_window,720,&widthOK,FALSE);
			UINT height = GetDlgItemInt(m_window,721,&heightOK,FALSE);
			static const UINT depths[] = {4,8,16,24,32};
			INT depth = static_cast<INT>(SendDlgItemMessageW(m_window,722,CB_GETCURSEL,0,0));
			CheckIf(!widthOK || !heightOK || depth < 0 || depth >= ARRAYSIZE(depths),E_INVALIDARG);
			Check(m_session->data->Configure(width,height,depths[depth],IsDlgButtonChecked(m_window,723) == BST_CHECKED));
		Cleanup:
			return hr;
		}
		VOID Scroll(INT rows)
		{
			RECT client;
			GetClientRect(m_grid,&client);
			INT total = (m_session->data->Count() + Columns() - 1) / Columns();
			INT page = max(1,client.bottom / CellHeight());
			m_top = max(0,min(max(0,total - page),rows));
			SCROLLINFO info = { sizeof(info),SIF_RANGE | SIF_PAGE | SIF_POS };
			info.nMax = max(0,total - 1);
			info.nPage = page;
			info.nPos = m_top;
			SetScrollInfo(m_grid,SB_VERT,&info,TRUE);
			InvalidateRect(m_grid,NULL,FALSE);
		}
		VOID Refresh()
		{
			UINT count = m_session->data->Count();
			m_selected = count ? min(max(0,m_selected),static_cast<INT>(count) - 1) : -1;
			EnableWindow(GetDlgItem(m_window,731),m_selected >= 0);
			EnableWindow(GetDlgItem(m_window,732),m_selected >= 0);
			EnableWindow(GetDlgItem(m_window,733),count != 0);
			EnableWindow(GetDlgItem(m_window,734),m_selected > 0);
			EnableWindow(GetDlgItem(m_window,735),m_selected >= 0 && static_cast<UINT>(m_selected + 1) < count);
			CRString text;
			if(m_selected >= 0)
			{
				UINT cx;
				UINT cy;
				UINT sizing;
				if(SUCCEEDED(m_session->data->SourceSize(m_selected,&cx,&cy,&sizing)) &&
					SUCCEEDED(RStrFormatW(&text,L"Index: %u of %u\r\nSource: %u x %u\r\nCell: %u x %u, Color%u\r\nSizing: %ls",m_selected,count,cx,cy,m_session->data->width,m_session->data->height,m_session->data->depth,QFI::ImageListSizingName(sizing))))
				{
					SetDlgItemTextW(m_window,702,text.c_wstr());
				}
			}
			else
			{
				SetDlgItemTextW(m_window,702,L"No image selected.");
			}
			text = NULL;
			if(SUCCEEDED(RStrFormatW(&text,L"%u images. Drag to reorder; indexes follow collection order.",count)))
			{
				SetDlgItemTextW(m_window,703,text.c_wstr());
			}
			Scroll(m_top);
			InvalidateRect(GetDlgItem(m_window,701),NULL,FALSE);
		}
		INT Hit(POINT point)
		{
			RECT client;
			GetClientRect(m_grid,&client);
			if(!PtInRect(&client,point))
			{
				return -1;
			}
			INT index = (point.y / CellHeight() + m_top) * Columns() + point.x / CellWidth();
			return point.x / CellWidth() < Columns() && index < static_cast<INT>(m_session->data->Count()) ? index : -1;
		}
		HRESULT Move(INT target)
		{
			HRESULT hr;
			CheckIf(m_selected < 0 || target < 0,E_INVALIDARG);
			Check(m_session->data->Move(m_selected,target));
			m_selected = target;
			Refresh();
		Cleanup:
			return hr;
		}
		VOID PaintGrid(HDC target)
		{
			RECT client;
			GetClientRect(m_grid,&client);
			HDC dc = CreateCompatibleDC(target);
			HBITMAP bitmap = dc ? CreateCompatibleBitmap(target,max(1,client.right),max(1,client.bottom)) : NULL;
			HGDIOBJ previous = bitmap ? SelectObject(dc,bitmap) : NULL;
			if(!previous || previous == HGDI_ERROR)
			{
				if(bitmap)
				{
					DeleteObject(bitmap);
				}
				if(dc)
				{
					DeleteDC(dc);
				}
				return;
			}
			FillRect(dc,&client,GetSysColorBrush(COLOR_WINDOW));
			SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(m_window,WM_GETFONT,0,0)));
			SetBkMode(dc,TRANSPARENT);
			INT columns = Columns();
			for(UINT n = 0; n < m_session->data->Count(); n++)
			{
				INT x = (n % columns) * CellWidth();
				INT y = (static_cast<INT>(n / columns) - m_top) * CellHeight();
				if(y + CellHeight() <= 0 || y >= client.bottom)
				{
					continue;
				}
				RECT cell = {x + 3,y + 3,x + CellWidth() - 3,y + CellHeight() - 3};
				if(n == m_selected)
				{
					FillRect(dc,&cell,GetSysColorBrush(COLOR_HIGHLIGHT));
				}
				RECT picture = {cell.left + 4,cell.top + 4,cell.right - 4,cell.bottom - 22};
				QFI::Raster pixels;
				HRESULT rendered = m_session->data->Render(n,pixels);
				if(SUCCEEDED(rendered))
				{
					QFI::PaintImageListPixels(dc,&picture,pixels,IsDlgButtonChecked(m_window,725) == BST_CHECKED);
				}
				else
				{
					DrawTextW(dc,L"Preview unavailable",-1,&picture,DT_CENTER | DT_WORDBREAK);
				}
				RECT label = {cell.left,cell.bottom - 20,cell.right,cell.bottom};
				WCHAR text[16];
				wsprintfW(text,L"%u",n);
				SetTextColor(dc,GetSysColor(n == m_selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
				DrawTextW(dc,text,-1,&label,DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			}
			BitBlt(target,0,0,client.right,client.bottom,dc,0,0,SRCCOPY);
			SelectObject(dc,previous);
			DeleteObject(bitmap);
			DeleteDC(dc);
		}
		HRESULT Import(BOOL replace)
		{
			HRESULT hr;
			CChooseFile chooser;
			TArray<PCWSTR> paths;
			TArray<BOOL> split;
			UINT filter;
			UINT oldCount = m_session->data->Count();
			Check(Settings());
			Check(chooser.Initialize());
			hr = chooser.Choose(m_window,TRUE,replace ? L"Replace Image" : L"Add Images",L"",
				L"Images (*.bmp;*.ico;*.png)\0*.bmp;*.ico;*.png\0All files\0*.*\0",NULL,1,
				OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (replace ? 0 : OFN_ALLOWMULTISELECT),&filter);
			CheckIfIgnore(hr == S_FALSE,S_FALSE);
			Check(hr);
			for(INT n = 0; n < chooser.GetFileCount(); n++)
			{
				PCWSTR path = chooser.GetFile(n);
				TStackRef<IJSONValue> value;
				TStackRef<QFI::CImageResource> image;
				BOOL divide = FALSE;
				Check(paths.Append(path));
				Check(QFI::ImportFile(path,&value));
				Check(QFI::CImageResource::Create(value,&image));
				INT length = TStrLenAssert(path);
				if(!replace && length >= 4 && !lstrcmpiW(path + length - 4,L".bmp") &&
					!(image->Width() % m_session->data->width) && !(image->Height() % m_session->data->height) &&
					(image->Width() != m_session->data->width || image->Height() != m_session->data->height))
				{
					CRString question;
					Check(RStrFormatW(&question,L"%ls\r\n\r\nThis bitmap fits %u cells. Split it into individual images in row-major order?",path,
						(image->Width() / m_session->data->width) * (image->Height() / m_session->data->height)));
					INT answer = MessageBoxW(m_window,question.c_wstr(),L"Bitmap Strip",MB_YESNOCANCEL | MB_ICONQUESTION);
					CheckIfIgnore(answer == IDCANCEL,S_FALSE);
					divide = answer == IDYES;
				}
				Check(split.Append(divide));
			}
			INT sizing = static_cast<INT>(SendDlgItemMessageW(m_window,724,CB_GETCURSEL,0,0));
			CheckIf(!paths.Length() || paths.Length() > QFI::MaxImageListCount,E_INVALIDARG);
			Check(m_session->Import(&paths[0],static_cast<UINT>(paths.Length()),sizing,&split[0],IsDlgButtonChecked(m_window,726) == BST_CHECKED,replace ? m_selected : -1));
			m_selected = replace ? m_selected : oldCount;
			Refresh();
		Cleanup:
			return hr;
		}
		HRESULT Initialize()
		{
			HRESULT hr;
			m_session.Attach(__new CImageListEditSession);
			CheckAlloc(m_session);
			Check(m_session->Initialize(m_editor,m_id));
			SetWindowTextW(m_window,L"Image List Editor");
			Child(L"STATIC",L"Images (zero-based indexes)",0,0,12,8,310,12);
			m_grid = Child(L"STATIC",L"",WS_TABSTOP | WS_VSCROLL | SS_NOTIFY,700,12,22,312,235);
			CheckIf(!m_grid,HrEnsureLastError());
			CheckIf(!SetWindowSubclass(m_grid,GridProc,1,reinterpret_cast<DWORD_PTR>(this)),HrEnsureLastError());
			Child(L"STATIC",L"Selected image",0,0,336,8,160,12);
			Child(L"STATIC",L"",SS_OWNERDRAW,701,336,22,160,130);
			Child(L"STATIC",L"",0,702,336,160,160,65);
			Child(L"BUTTON",L"Checkerboard",BS_AUTOCHECKBOX | WS_TABSTOP,725,336,237,160,14);
			CheckDlgButton(m_window,725,BST_CHECKED);
			Child(L"STATIC",L"Width",0,0,12,267,32,12);
			Child(L"EDIT",L"",ES_NUMBER | WS_TABSTOP,720,47,264,32,16);
			Child(L"STATIC",L"Height",0,0,86,267,32,12);
			Child(L"EDIT",L"",ES_NUMBER | WS_TABSTOP,721,121,264,32,16);
			Child(L"STATIC",L"Depth",0,0,166,267,30,12);
			HWND depth = Child(L"COMBOBOX",L"",CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,722,198,264,80,100);
			Child(L"BUTTON",L"Masked",BS_AUTOCHECKBOX | WS_TABSTOP,723,284,265,50,14);
			Child(L"STATIC",L"Import",0,0,336,267,32,12);
			HWND sizing = Child(L"COMBOBOX",L"",CBS_DROPDOWNLIST | WS_TABSTOP,724,372,264,124,100);
			Child(L"BUTTON",L"Top-left color as mask",BS_AUTOCHECKBOX | WS_TABSTOP,726,336,287,160,14);
			static PCWSTR const labels[] = {L"Add...",L"Replace...",L"Delete",L"Clear All",L"Move Left",L"Move Right"};
			for(UINT n = 0; n < ARRAYSIZE(labels); n++)
			{
				Child(L"BUTTON",labels[n],WS_TABSTOP,730 + n,12 + n * 53,295,49,18);
			}
			Child(L"STATIC",L"",0,703,12,320,310,22);
			Child(L"BUTTON",L"OK",BS_DEFPUSHBUTTON | WS_TABSTOP,IDOK,336,320,50,20);
			Child(L"BUTTON",L"Cancel",WS_TABSTOP,IDCANCEL,390,320,50,20);
			Child(L"BUTTON",L"Apply",WS_TABSTOP,0x3021,444,320,50,20);
			static PCWSTR const depths[] = {L"Color4",L"Color8",L"Color16",L"Color24",L"Color32"};
			static const UINT numbers[] = {4,8,16,24,32};
			for(UINT n = 0; n < ARRAYSIZE(depths); n++)
			{
				SendMessageW(depth,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(depths[n]));
				if(numbers[n] == m_session->data->depth)
				{
					SendMessageW(depth,CB_SETCURSEL,n,0);
				}
			}
			for(UINT n = 0; n <= QFI::ImageContain; n++)
			{
				SendMessageW(sizing,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(QFI::ImageListSizingName(n)));
			}
			SendMessageW(sizing,CB_SETCURSEL,QFI::ImageContain,0);
			SetDlgItemInt(m_window,720,m_session->data->width,FALSE);
			SetDlgItemInt(m_window,721,m_session->data->height,FALSE);
			CheckDlgButton(m_window,723,m_session->data->masked ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(m_window,726,m_session->data->ColorKeyEnabled() ? BST_CHECKED : BST_UNCHECKED);
			Refresh();
		Cleanup:
			return hr;
		}
		static LRESULT CALLBACK GridProc(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
		{
			CImageListDialog* dialog = reinterpret_cast<CImageListDialog*>(data);
			POINT point = {GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
			switch(msg)
			{
			case WM_ERASEBKGND:
				return 1;
			case WM_PAINT:
				{
					PAINTSTRUCT paint;
					HDC dc = BeginPaint(window,&paint);
					dialog->PaintGrid(dc);
					EndPaint(window,&paint);
				}
				return 0;
			case WM_PRINTCLIENT:
				dialog->PaintGrid(reinterpret_cast<HDC>(wp));
				return 0;
			case WM_SIZE:
				dialog->Scroll(dialog->m_top);
				break;
			case WM_VSCROLL:
				{
					SCROLLINFO info = {sizeof(info),SIF_ALL};
					GetScrollInfo(window,SB_VERT,&info);
					INT next = dialog->m_top;
					switch(LOWORD(wp))
					{
					case SB_LINEUP:
						next--;
						break;
					case SB_LINEDOWN:
						next++;
						break;
					case SB_PAGEUP:
						next -= info.nPage;
						break;
					case SB_PAGEDOWN:
						next += info.nPage;
						break;
					case SB_THUMBTRACK:
						next = info.nTrackPos;
						break;
					case SB_TOP:
						next = 0;
						break;
					case SB_BOTTOM:
						next = info.nMax;
						break;
					}
					dialog->Scroll(next);
				}
				return 0;
			case WM_MOUSEWHEEL:
				dialog->Scroll(dialog->m_top - GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * 3);
				return 0;
			case WM_LBUTTONDOWN:
				SetFocus(window);
				dialog->m_selected = dialog->Hit(point);
				dialog->m_drag = dialog->m_selected;
				dialog->m_down = point;
				dialog->m_dragging = FALSE;
				if(dialog->m_drag >= 0)
				{
					SetCapture(window);
				}
				dialog->Refresh();
				return 0;
			case WM_MOUSEMOVE:
				if(GetCapture() == window && (abs(point.x - dialog->m_down.x) > GetSystemMetrics(SM_CXDRAG) || abs(point.y - dialog->m_down.y) > GetSystemMetrics(SM_CYDRAG)))
				{
					dialog->m_dragging = TRUE;
					SetCursor(LoadCursor(NULL,IDC_SIZEALL));
					RECT client;
					GetClientRect(window,&client);
					if(point.y < 0 || point.y >= client.bottom)
					{
						dialog->Scroll(dialog->m_top + (point.y < 0 ? -1 : 1));
					}
				}
				return 0;
			case WM_LBUTTONUP:
				if(GetCapture() == window)
				{
					INT target = dialog->Hit(point);
					if(dialog->m_dragging && target >= 0)
					{
						dialog->Move(target);
					}
					ReleaseCapture();
				}
				return 0;
			case WM_CAPTURECHANGED:
				dialog->m_dragging = FALSE;
				dialog->m_drag = -1;
				break;
			case WM_NCDESTROY:
				RemoveWindowSubclass(window,GridProc,id);
				break;
			}
			return DefSubclassProc(window,msg,wp,lp);
		}
		static INT_PTR CALLBACK Procedure(HWND window, UINT msg, WPARAM wp, LPARAM lp)
		{
			CImageListDialog* dialog = reinterpret_cast<CImageListDialog*>(GetWindowLongPtrW(window,DWLP_USER));
			if(msg == WM_INITDIALOG)
			{
				dialog = reinterpret_cast<CImageListDialog*>(lp);
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
			if(msg == WM_DRAWITEM && wp == 701)
			{
				DRAWITEMSTRUCT* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
				FillRect(draw->hDC,&draw->rcItem,GetSysColorBrush(COLOR_WINDOW));
				QFI::Raster pixels;
				if(dialog->m_selected >= 0 && SUCCEEDED(dialog->m_session->data->Render(dialog->m_selected,pixels)))
				{
					QFI::PaintImageListPixels(draw->hDC,&draw->rcItem,pixels,IsDlgButtonChecked(window,725) == BST_CHECKED);
				}
				return TRUE;
			}
			if(msg == WM_CLOSE)
			{
				EndDialog(window,IDCANCEL);
				return TRUE;
			}
			if(msg == WM_COMMAND)
			{
				HRESULT hr = S_OK;
				UINT command = LOWORD(wp);
				if(command == IDCANCEL)
				{
					EndDialog(window,IDCANCEL);
					return TRUE;
				}
				if(command == IDOK || command == 0x3021)
				{
					hr = dialog->Settings();
					if(SUCCEEDED(hr))
					{
						hr = dialog->m_session->Apply();
					}
					if(SUCCEEDED(hr) && command == IDOK)
					{
						EndDialog(window,IDOK);
					}
				}
				else if(command == 730 || command == 731)
				{
					hr = dialog->Import(command == 731);
				}
				else if(command == 732 && dialog->m_selected >= 0)
				{
					hr = dialog->m_session->data->Remove(dialog->m_selected);
				}
				else if(command == 733 && MessageBoxW(window,L"Remove every image from this collection?",L"Clear All",MB_YESNO | MB_ICONQUESTION) == IDYES)
				{
					dialog->m_session->data->Clear();
				}
				else if(command == 734 || command == 735)
				{
					hr = dialog->Move(dialog->m_selected + (command == 734 ? -1 : 1));
				}
				else if(command == 726)
				{
					hr = dialog->m_session->data->SetColorKey(IsDlgButtonChecked(window,726) == BST_CHECKED);
					if(FAILED(hr))
					{
						CheckDlgButton(window,726,IsDlgButtonChecked(window,726) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED);
					}
				}
				else if(command == 725 || command == 723 || (command == 722 && HIWORD(wp) == CBN_SELCHANGE) ||
					((command == 720 || command == 721) && HIWORD(wp) == EN_KILLFOCUS))
				{
					hr = dialog->Settings();
				}
				else
				{
					return FALSE;
				}
				if(FAILED(hr))
				{
					dialog->Error(hr);
				}
				dialog->Refresh();
				return TRUE;
			}
			return FALSE;
		}
	public:
		CImageListDialog(CDesignerEditor* editor, REFGUID id) : m_editor(editor), m_id(id), m_window(NULL), m_grid(NULL), m_selected(0), m_top(0), m_drag(-1), m_dragging(FALSE), m_error(S_OK)
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
			definition.dialog.cx = 510;
			definition.dialog.cy = 350;
			definition.pointSize = 8;
			CopyMemory(definition.font,L"MS Shell Dlg",sizeof(L"MS Shell Dlg"));
			INT_PTR answer = DialogBoxIndirectParamW(GetModuleHandle(NULL),&definition.dialog,owner,Procedure,reinterpret_cast<LPARAM>(this));
			hr = answer == -1 ? HrEnsureLastError() : FAILED(m_error) ? m_error : m_session && m_session->committed ? S_OK : S_FALSE;
			return hr;
		}
	};
	HRESULT CDesignerEditor::EditImageList()
	{
		HRESULT hr;
		Check(FinishInline(TRUE));
		{
			CImageListDialog dialog(this,m_selected);
			Check(dialog.Show(GetAncestor(m_window,GA_ROOT)));
		}
	Cleanup:
		return hr;
	}
	HRESULT CDesignerEditor::ApplyComponentProperties(REFGUID id, IJSONObject* properties)
	{
		HRESULT hr;
		m_changeDepth++;
		hr = m_document.ApplyComponentProperties(id,properties);
		m_changeDepth--;
		Check(hr);
		m_selection.Clear();
		m_selected = id;
		Check(Refresh(TRUE));
	Cleanup:
		return hr;
	}
}
