#include "ListView.h"
#include "NativeImageList.h"
#include "..\Imaging\ImageSurface.h"

namespace QF
{
	class CListDerivedImages : public CBaseUnknown, public IQuadooImageListSnapshot
	{
		TStackRef<CListLink> m_link;
		TStackRef<IQuadooImageListSnapshot> m_source;
	public:
		HIMAGELIST images;
		UINT width;
		UINT height;
		UINT dpi;
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooImageListSnapshot)
		END_UNK_MAP
		CListDerivedImages(CListLink* link, IQuadooImageListSnapshot* source, UINT scale) :
			m_link(link), m_source(source), images(NULL), width(0), height(0), dpi(scale)
		{
		}
		~CListDerivedImages()
		{
			if(images)
			{
				ImageList_Destroy(images);
			}
		}
		STDMETHOD(GetHandle)(HIMAGELIST* value)
		{
			HRESULT hr = S_OK;
			CheckIf(!value,E_POINTER);
			*value = NULL;
			CheckIf(m_link->thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			*value = images;
		Cleanup:
			return hr;
		}
		STDMETHOD(GetSize)(UINT* x, UINT* y, UINT* scale)
		{
			HRESULT hr = S_OK;
			CheckIf(!x || !y || !scale,E_POINTER);
			CheckIf(m_link->thread != GetCurrentThreadId(),QuadooForm::EWrongThread);
			*x = width;
			*y = height;
			*scale = dpi;
		Cleanup:
			return hr;
		}
		HRESULT Build(BOOL composite)
		{
			HRESULT hr;
			HIMAGELIST source = NULL;
			HIMAGELIST checks = NULL;
			QFI::CImageSurface black;
			QFI::CImageSurface white;
			QFI::Raster pixels;
			HDC dc = NULL;
			HDC screen = NULL;
			HBITMAP bitmap = NULL;
			HGDIOBJ previous = NULL;
			INT imageWidth = 1;
			INT imageHeight = 1;
			INT count = 0;
			INT check = max(1,MulDiv(13,dpi,96));
			INT slot = check + max(2,MulDiv(4,dpi,96));
			COLORREF mask = RGB(255,0,255);
			if(composite && m_source)
			{
				Check(m_source->GetHandle(&source));
				CheckIf(!source || !ImageList_GetIconSize(source,&imageWidth,&imageHeight),E_UNEXPECTED);
				count = ImageList_GetImageCount(source);
				CheckIf(count < 0,E_UNEXPECTED);
			}
			width = composite ? slot + imageWidth : slot;
			height = max(check + max(2,MulDiv(2,dpi,96)),composite ? imageHeight : 1);
			CheckIf(static_cast<ULONGLONG>(width) * height * (composite ? (static_cast<ULONGLONG>(count) + 1) * 2 : 2) > 16 * 1024 * 1024,E_OUTOFMEMORY);
			checks = ImageList_Create(slot,height,ILC_COLOR32 | ILC_MASK,2,1);
			CheckIf(!checks,E_OUTOFMEMORY);
			screen = GetDC(NULL);
			CheckIf(!screen,HrEnsureLastError());
			dc = CreateCompatibleDC(screen);
			CheckIf(!dc,E_OUTOFMEMORY);
			bitmap = CreateCompatibleBitmap(screen,slot,height);
			CheckIf(!bitmap,E_OUTOFMEMORY);
			previous = SelectObject(dc,bitmap);
			for(INT state = 0; state < 2; state++)
			{
				RECT area = {0,0,slot,static_cast<INT>(height)};
				RECT box = {(slot - check) / 2,(static_cast<INT>(height) - check) / 2,
					(slot - check) / 2 + check,(static_cast<INT>(height) - check) / 2 + check};
				HBRUSH background = CreateSolidBrush(mask);
				CheckIf(!background,E_OUTOFMEMORY);
				FillRect(dc,&area,background);
				DeleteObject(background);
				CheckIf(!DrawFrameControl(dc,&box,DFC_BUTTON,DFCS_BUTTONCHECK | (state ? DFCS_CHECKED : 0)),E_FAIL);
				CheckIf(ImageList_AddMasked(checks,bitmap,mask) != state,E_OUTOFMEMORY);
			}
			if(!composite)
			{
				images = checks;
				checks = NULL;
			}
			else
			{
				images = ImageList_Create(width,height,ILC_COLOR32 | ILC_MASK,(count + 1) * 2,1);
				CheckIf(!images,E_OUTOFMEMORY);
				Check(black.Create(width,height));
				Check(white.Create(width,height));
				Check(pixels.Allocate(width,height));
				for(INT n = -1; n < count; n++)
				{
					for(INT state = 0; state < 2; state++)
					{
						// Recover premultiplied RGB and coverage from native draws.
						// Merge/GetIcon/ReplaceIcon repeatedly round translucent pixels.
						// Black and white differ by 255 - alpha, also for masked lists.
						HDC targets[] = {black.DC(),white.DC()};
						RECT bounds = {0,0,static_cast<INT>(width),static_cast<INT>(height)};
						for(UINT target = 0; target < ARRAYSIZE(targets); target++)
						{
							CheckIf(!FillRect(targets[target],&bounds,static_cast<HBRUSH>(GetStockObject(target ? WHITE_BRUSH : BLACK_BRUSH))),E_FAIL);
							CheckIf(!ImageList_Draw(checks,state,targets[target],0,0,ILD_TRANSPARENT),E_FAIL);
							if(n >= 0)
							{
								CheckIf(!ImageList_Draw(source,n,targets[target],slot,(static_cast<INT>(height) - imageHeight) / 2,ILD_TRANSPARENT),E_FAIL);
							}
						}
						CheckIf(!GdiFlush(),E_FAIL);
						CopyMemory(pixels.pixels,black.Pixels(),width * height * 4);
						for(UINT pixel = 0; pixel < width * height; pixel++)
						{
							pixels.pixels[pixel * 4 + 3] = static_cast<BYTE>(255 - (white.Pixels()[pixel * 4] - black.Pixels()[pixel * 4]));
						}
						Check(AppendNativeImage(images,pixels,(n + 1) * 2 + state));
					}
				}
			}
			hr = S_OK;
		Cleanup:
			if(previous)
			{
				SelectObject(dc,previous);
			}
			if(bitmap)
			{
				DeleteObject(bitmap);
			}
			if(dc)
			{
				DeleteDC(dc);
			}
			if(screen)
			{
				ReleaseDC(NULL,screen);
			}
			if(checks)
			{
				ImageList_Destroy(checks);
			}
			return hr;
		}
	};
	HRESULT CListView::PrepareVirtualImages(IQuadooImageListSnapshot** source, IQuadooImageListSnapshot** result)
	{
		HRESULT hr = S_OK;
		TStackRef<CListDerivedImages> derived;
		UINT role;
		for(UINT n = 0; n < 3; n++)
		{
			result[n] = NULL;
		}
		CheckIfIgnore(!IsVirtual() || !Boolean(L"CheckBoxes"),S_OK);
		role = Integer(L"ViewStyle") == QuadooForm::ListViewIcon ? 0 :
			Integer(L"ViewStyle") == QuadooForm::ListViewSmallIcon ? 1 : 2;
		derived.Attach(__new CListDerivedImages(m_link,role < 2 ? source[role] : NULL,m_dpi));
		CheckAlloc(derived);
		Check(derived->Build(role < 2));
		result[role] = derived.Detach();
	Cleanup:
		return hr;
	}
	HRESULT CListView::ImageHandles(IQuadooImageListSnapshot** source, IQuadooImageListSnapshot** derived, HIMAGELIST* handles)
	{
		HRESULT hr = S_OK;
		for(UINT n = 0; n < 3; n++)
		{
			handles[n] = NULL;
			IQuadooImageListSnapshot* snapshot = derived[n] ? derived[n] : source[n];
			if(snapshot)
			{
				Check(snapshot->GetHandle(&handles[n]));
				CheckIf(!handles[n],E_UNEXPECTED);
			}
		}
	Cleanup:
		return hr;
	}
	HRESULT CListView::CommitVirtualImages(IQuadooImageListSnapshot** images, HIMAGELIST* prepared)
	{
		HRESULT hr = S_OK;
		HIMAGELIST handles[3];
		IQuadooImageListSnapshot* sources[3];
		if(!prepared)
		{
			for(UINT n = 0; n < 3; n++)
			{
				sources[n] = m_images[n].snapshot;
			}
			Check(ImageHandles(sources,images,handles));
			prepared = handles;
		}
		m_nativeSync++;
		for(UINT n = 0; n < 3; n++)
		{
			if(IsVirtual() || n != QuadooForm::ListStateImages || !Boolean(L"CheckBoxes"))
			{
				ListView_SetImageList(m_hwnd,prepared[n],n == 0 ? LVSIL_NORMAL : n == 1 ? LVSIL_SMALL : LVSIL_STATE);
			}
			m_virtualImages[n] = images[n];
		}
		m_nativeSync--;
		m_virtualNativeStyle = Integer(L"ViewStyle");
	Cleanup:
		return hr;
	}
	HRESULT CListView::RefreshVirtualImages()
	{
		HRESULT hr;
		IQuadooImageListSnapshot* source[3];
		IQuadooImageListSnapshot* images[3] = {NULL,NULL,NULL};
		for(UINT n = 0; n < 3; n++)
		{
			source[n] = m_images[n].snapshot;
		}
		Check(PrepareVirtualImages(source,images));
		Check(CommitVirtualImages(images));
	Cleanup:
		for(UINT n = 0; n < 3; n++)
		{
			SafeRelease(images[n]);
		}
		return hr;
	}
	INT CListView::VirtualImageIndex(INT image, BOOL checked)
	{
		UINT role = Integer(L"ViewStyle") == QuadooForm::ListViewIcon ? 0 : 1;
		HIMAGELIST base = NULL;
		if(m_images[role].snapshot)
		{
			m_images[role].snapshot->GetHandle(&base);
		}
		if(image < 0 || !base || image >= ImageList_GetImageCount(base))
		{
			image = -1;
		}
		return m_virtualImages[role] ? (image + 1) * 2 + (checked ? 1 : 0) : image;
	}
	BOOL CListView::VirtualCheckHit(POINT point, INT* index)
	{
		LVHITTESTINFO hit = {0};
		hit.pt = point;
		*index = ListView_HitTest(m_hwnd,&hit);
		if(*index < 0)
		{
			return FALSE;
		}
		if(hit.flags & LVHT_ONITEMSTATEICON)
		{
			return TRUE;
		}
		if(m_virtualImages[0] || m_virtualImages[1])
		{
			RECT icon;
			if(ListView_GetItemRect(m_hwnd,*index,&icon,LVIR_ICON))
			{
				icon.right = icon.left + max(1,MulDiv(13,m_dpi,96)) + max(2,MulDiv(4,m_dpi,96));
				return PtInRect(&icon,point);
			}
		}
		return FALSE;
	}
}
