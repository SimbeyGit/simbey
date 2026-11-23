#pragma once
#include <windows.h>
#include "Library\Core\CoreDefs.h"
namespace QFI
{
#pragma pack(push,2)
struct IconDirectory { WORD reserved, type, count; };
struct IconEntry { BYTE width, height, colors, reserved; WORD planes, bits; DWORD size, offset; };
#pragma pack(pop)
inline HRESULT ValidateIconDirectory(const BYTE* bytes, UINT size, UINT limit, IconDirectory* directory)
{
	HRESULT hr = S_OK;
	CheckIf(!bytes || !directory || size < sizeof(*directory) || size > limit,E_INVALIDARG);
	CopyMemory(directory,bytes,sizeof(*directory));
	CheckIf(directory->reserved || directory->type != 1 || !directory->count || directory->count > 64,E_INVALIDARG);
	UINT start = sizeof(*directory) + directory->count * sizeof(IconEntry);
	CheckIf(start > size,E_INVALIDARG);
	for(UINT n = 0; n < directory->count; n++)
	{
		IconEntry entry;
		CopyMemory(&entry,bytes + sizeof(*directory) + n * sizeof(entry),sizeof(entry));
		CheckIf(entry.reserved || entry.offset < start || entry.offset > size || !entry.size || entry.size > size - entry.offset,E_INVALIDARG);
		for(UINT p = 0; p < n; p++)
		{
			IconEntry previous;
			CopyMemory(&previous,bytes + sizeof(*directory) + p * sizeof(previous),sizeof(previous));
			CheckIf(entry.offset < previous.offset + previous.size && previous.offset < entry.offset + entry.size,E_INVALIDARG);
		}
	}
Cleanup:
	return hr;
}
}
