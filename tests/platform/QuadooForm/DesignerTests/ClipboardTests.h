// Preserve all materialized clipboard formats while exercising the real Win32
// clipboard. OleDuplicateData understands the special bitmap/metafile handles.
class CTestClipboard
{
	struct Item { UINT format; HANDLE data; };
	TArray<Item> m_items;
	BOOL m_saved;
	HWND m_owner;
	static VOID Free(Item& item)
	{
		if(!item.data) return;
		if(item.format == CF_BITMAP || item.format == CF_PALETTE || item.format == CF_DSPBITMAP) DeleteObject(item.data);
		else if(item.format == CF_ENHMETAFILE || item.format == CF_DSPENHMETAFILE) DeleteEnhMetaFile(static_cast<HENHMETAFILE>(item.data));
		else
		{
			if(item.format == CF_METAFILEPICT || item.format == CF_DSPMETAFILEPICT)
			{
				METAFILEPICT* picture = static_cast<METAFILEPICT*>(GlobalLock(item.data));
				if(picture) { DeleteMetaFile(picture->hMF); GlobalUnlock(item.data); }
			}
			GlobalFree(item.data);
		}
	}
public:
	CTestClipboard() : m_saved(FALSE), m_owner(NULL) {}
	~CTestClipboard()
	{
		if(m_saved && QFD::OpenDesignerClipboard(m_owner))
		{
			if(EmptyClipboard()) for(sysint n = 0; n < m_items.Length(); n++)
				if(SetClipboardData(m_items[n].format,m_items[n].data)) m_items[n].data = NULL;
			CloseClipboard();
		}
		for(sysint n = 0; n < m_items.Length(); n++) Free(m_items[n]);
		if(m_owner) DestroyWindow(m_owner);
	}
	HRESULT Save(HWND owner)
	{
		HRESULT hr;
		BOOL opened = FALSE;
		UINT format = 0;
		m_owner = CreateWindowExW(0,L"STATIC",L"Clipboard test owner",0,0,0,0,0,HWND_MESSAGE,NULL,GetModuleHandle(NULL),NULL);
		CheckIf(!m_owner,HrEnsureLastError());
		CheckIf(!QFD::OpenDesignerClipboard(owner),HrEnsureLastError());
		opened = TRUE;
		while((format = EnumClipboardFormats(format)) != 0)
		{
			Item item;
			CheckIf(format == CF_OWNERDISPLAY,E_NOTIMPL);
			HANDLE data = GetClipboardData(format);
			CheckIf(!data,HrEnsureLastError());
			item.format = format; item.data = OleDuplicateData(data,static_cast<CLIPFORMAT>(format),GMEM_MOVEABLE);
			CheckIf(!item.data,E_OUTOFMEMORY);
			hr = m_items.Append(item);
			if(FAILED(hr)) { Free(item); goto Cleanup; }
		}
		m_saved = TRUE; hr = S_OK;
Cleanup:
		if(opened) CloseClipboard();
		return hr;
	}
	HRESULT Put(HWND owner, PCWSTR text)
	{
		HRESULT hr;
		HGLOBAL memory = NULL;
		PWSTR copy;
		BOOL opened = FALSE;
		SIZE_T bytes = (TStrLenAssert(text) + 1) * sizeof(WCHAR);
		memory = GlobalAlloc(GMEM_MOVEABLE,bytes);
		CheckIf(!memory,E_OUTOFMEMORY);
		copy = static_cast<PWSTR>(GlobalLock(memory));
		CheckIf(!copy,E_OUTOFMEMORY);
		CopyMemory(copy,text,bytes); GlobalUnlock(memory);
		CheckIf(!QFD::OpenDesignerClipboard(owner),HrEnsureLastError());
		opened = TRUE;
		CheckIf(!EmptyClipboard(),HrEnsureLastError());
		CheckIf(!SetClipboardData(CF_UNICODETEXT,memory),HrEnsureLastError());
		memory = NULL;
		hr = S_OK;
Cleanup:
		if(opened) CloseClipboard();
		if(memory) GlobalFree(memory);
		return hr;
	}
};
