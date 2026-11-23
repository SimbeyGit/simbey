#include "Runtime.h"
#include "Library\Core\MemoryStream.h"

namespace QF
{
static HRESULT NativeMemoText(RSTRING text, RSTRING* result)
{
	HRESULT hr; CMemoryStream buffer; UINT length = RStrLen(text); PWSTR target;
	CheckIf(length > (INT_MAX - 1) / 2,E_INVALIDARG);
	Check(buffer.TWriteAdvance(&target,length * 2 + 1)); UINT used = 0;
	for(UINT n = 0; n < length; n++)
	{
		WCHAR ch = RStrToWide(text)[n]; CheckIf(!ch,E_INVALIDARG);
		if(ch == L'\r' || ch == L'\n')
		{
			if(ch == L'\r' && n + 1 < length && RStrToWide(text)[n + 1] == L'\n') n++;
			target[used++] = L'\r'; target[used++] = L'\n';
		}
		else target[used++] = ch;
	}
	Check(RStrCreateW(used,target,result));
Cleanup:
	return hr;
}
VOID CMemo::NativeStyle(PCWSTR* className, DWORD* style, DWORD* extended)
{
	CTextEdit::NativeStyle(className,style,extended); *style &= ~ES_AUTOHSCROLL;
	*style |= ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL;
	if(Boolean(L"ReadOnly")) *style |= ES_READONLY;
	if(Integer(L"ScrollBars") & 2) *style |= WS_VSCROLL;
	if(!Boolean(L"WordWrap")) { *style |= ES_AUTOHSCROLL; if(Integer(L"ScrollBars") & 1) *style |= WS_HSCROLL; }
}
HRESULT CMemo::NativeCreated()
{
	HRESULT hr;
	Check(CWinControl::NativeCreated()); SendMessageW(m_hwnd,EM_SETLIMITTEXT,INT_MAX - 1,0);
	Check(ApplyNative(L"Text"));
Cleanup:
	return hr;
}
HRESULT CMemo::ApplyNative(PCWSTR name)
{
	HRESULT hr = S_OK;
	if(!TStrCmpAssert(name,L"Text"))
	{
		if(m_nativeTextChange) m_nativeTextChange = FALSE;
		else if(m_hwnd)
		{
			CRString logical, native; Check(StoredString(L"Text",&logical)); Check(NativeMemoText(logical,&native));
			Check(WriteNativeText(native));
		}
	}
	else if((!TStrCmpAssert(name,L"WordWrap") || !TStrCmpAssert(name,L"ScrollBars")) && m_hwnd)
	{
		DWORD start, end; SendMessageW(m_hwnd,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end));
		INT line = static_cast<INT>(SendMessageW(m_hwnd,EM_GETFIRSTVISIBLELINE,0,0)); BOOL modified = static_cast<BOOL>(SendMessageW(m_hwnd,EM_GETMODIFY,0,0));
		BOOL previous = m_syncText; m_syncText = TRUE; hr = ReplaceNative(); m_syncText = previous; Check(hr);
		SendMessageW(m_hwnd,EM_SETSEL,start,end); SendMessageW(m_hwnd,EM_LINESCROLL,0,line); SendMessageW(m_hwnd,EM_SETMODIFY,modified,0);
	}
	else if(!TStrCmpAssert(name,L"ReadOnly") && m_hwnd) { CheckIf(!SendMessageW(m_hwnd,EM_SETREADONLY,Boolean(L"ReadOnly"),0),E_FAIL); }
	else { Check(CTextEdit::ApplyNative(name)); }
Cleanup:
	return hr;
}
LRESULT CMemo::Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, BOOL content, BOOL* handled)
{
	LRESULT result = CWinControl::Message(hwnd,msg,wp,lp,content,handled);
	if(*handled) return result;
	if(msg == WM_GETDLGCODE)
	{
		result = CallWindowProcW(m_windowData.original,hwnd,msg,wp,lp); *handled = TRUE;
		if(wp == VK_RETURN) result |= DLGC_WANTMESSAGE;
		if(wp == VK_ESCAPE && !KeyboardComposing(hwnd)) result &= ~DLGC_WANTMESSAGE;
		if(wp == VK_TAB && !(GetKeyState(VK_CONTROL) & 0x8000))
		{
			result &= ~(DLGC_WANTTAB | DLGC_WANTMESSAGE);
			if(Boolean(L"WantTabs")) result |= DLGC_WANTTAB | DLGC_WANTMESSAGE;
		}
	}
	else if(msg == WM_KEYDOWN && wp == 'A' && (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000))
	{ SendMessageW(hwnd,EM_SETSEL,0,-1); *handled = TRUE; }
	else if(msg == WM_CHAR && wp == VK_TAB && Boolean(L"WantTabs"))
	{ if(!Boolean(L"ReadOnly")) SendMessageW(hwnd,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(L"\t")); *handled = TRUE; }
	return result;
}
}
