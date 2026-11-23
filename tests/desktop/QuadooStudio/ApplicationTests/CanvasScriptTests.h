static HRESULT CanvasScriptChecks(IQuadooVM* vm)
{
	HRESULT hr;
	QuadooVM::QVARIANT value = {QuadooVM::Null};
	TStackRef<ILockableStream> bytes;
	HDC dc = NULL;
	HBITMAP bitmap = NULL;
	HGDIOBJ old = NULL;
	LONGLONG native;
	PBYTE data = NULL;
	DWORD count;
	BOOL locked = FALSE;
	Check(Call(vm,L"CanvasChecks",&value));
	if(value.eType != QuadooVM::I4 && value.eType != QuadooVM::I8)
	{
		CRString detail;
		CHAR message[2048];
		if(SUCCEEDED(QVMVariantToString(&value,&detail)))
		{
			WideCharToMultiByte(CP_UTF8,0,detail.c_wstr(),-1,message,ARRAYSIZE(message),NULL,NULL);
			Stage(message);
		}
	}
	Check(AQVM::VariantInteger(&value,&native));
	REQUIRE(IsWindow(reinterpret_cast<HWND>(static_cast<UINT_PTR>(native))));
	QVMClearVariant(&value);
	dc = CreateCompatibleDC(NULL);
	CheckIf(!dc,HrEnsureLastError());
	bitmap = CreateBitmap(400,300,1,32,NULL);
	CheckIf(!bitmap,HrEnsureLastError());
	old = SelectObject(dc,bitmap);
	CheckIf(!old || old == HGDI_ERROR,E_FAIL);
	SendMessageW(reinterpret_cast<HWND>(static_cast<UINT_PTR>(native)),WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
	Check(Call(vm,L"CanvasFinish",&value));
	if(value.eType != QuadooVM::Object)
	{
		CRString detail;
		CHAR message[2048];
		if(SUCCEEDED(QVMVariantToString(&value,&detail)))
		{
			WideCharToMultiByte(CP_UTF8,0,detail.c_wstr(),-1,message,ARRAYSIZE(message),NULL,NULL);
			Stage(message);
		}
	}
	REQUIRE(value.eType == QuadooVM::Object);
	Check(value.pObject->QueryInterface(&bytes));
	Check(bytes->Lock(&data,&count));
	locked = TRUE;
	REQUIRE(count > 32 && !memcmp(data,"\x89PNG\r\n\x1a\n",8));
	REQUIRE(!memcmp(data + 12,"IHDR",4) && data[24] == 8 && data[25] == 6);
	hr = S_OK;
Cleanup:
	if(locked)
	{
		bytes->Unlock();
	}
	QVMClearVariant(&value);
	if(old && old != HGDI_ERROR)
	{
		SelectObject(dc,old);
	}
	if(bitmap)
	{
		DeleteObject(bitmap);
	}
	if(dc)
	{
		DeleteDC(dc);
	}
	return hr;
}
