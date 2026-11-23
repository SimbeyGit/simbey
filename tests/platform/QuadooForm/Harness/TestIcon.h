#pragma once
static HRESULT TestIconValue(IJSONValue** value)
{
	BYTE file[38 + 2 * sizeof(BITMAPINFOHEADER) + 16 * 16 * 4 + 16 * 4 + 32 * 32 * 4 + 32 * 4];
	ZeroMemory(file,sizeof(file)); WORD header[] = { 0,1,2 }; DWORD offset = 38;
	CopyMemory(file,header,sizeof(header));
	for(UINT image = 0; image < 2; image++)
	{
		UINT width = image ? 32 : 16; BYTE* entry = file + 6 + image * 16;
		entry[0] = entry[1] = static_cast<BYTE>(width); entry[4] = 1; entry[6] = 32;
		DWORD size = sizeof(BITMAPINFOHEADER) + width * width * 4 + width * 4;
		CopyMemory(entry + 8,&size,4); CopyMemory(entry + 12,&offset,4);
		BITMAPINFOHEADER bitmap; ZeroMemory(&bitmap,sizeof(bitmap)); bitmap.biSize = sizeof(bitmap); bitmap.biWidth = width; bitmap.biHeight = width * 2; bitmap.biPlanes = 1; bitmap.biBitCount = 32;
		CopyMemory(file + offset,&bitmap,sizeof(bitmap));
		for(UINT n = offset + sizeof(bitmap); n < offset + size - width * 4; n += 4) { file[n] = 0x90; file[n + 1] = 0x60; file[n + 2] = 0x20; file[n + 3] = 0xff; }
		offset += size;
	}
	return QuadooCreateIconValue(file,sizeof(file),value);
}
