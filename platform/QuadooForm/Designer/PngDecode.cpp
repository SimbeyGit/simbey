#include "Designer.h"
#include "..\Imaging\ImageResource.h"
namespace QFD
{
HRESULT DecodeIconPNG(const BYTE* bytes, UINT size, CMemoryStream& pixels, UINT* width, UINT* height)
{
	return QFI::DecodePNG(bytes,size,pixels,width,height);
}
}
