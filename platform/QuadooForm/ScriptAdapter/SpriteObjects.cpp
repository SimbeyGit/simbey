#include "SpriteObjects.h"
#include "SpritePixels.h"
#include "Library\Core\ILockableStream.h"

namespace AQVM
{
	static volatile LONG s_spriteObjects;

	LONG SpriteLiveObjects()
	{
		return InterlockedCompareExchange(&s_spriteObjects,0,0);
	}

	static QuadooVM::QVARIANT* SpriteArgument(QuadooVM::QVPARAMS* parameters, UINT index)
	{
		return parameters->pqvArgs + parameters->cArgs - index - 1;
	}

	static BOOL SpriteNameEquals(RSTRING value, PCWSTR text, UINT length)
	{
		return value && RStrIsWide(value) && RStrLen(value) == static_cast<INT>(length) &&
			!memcmp(RStrToWide(value),text,length * sizeof(WCHAR));
	}

	static HRESULT SpriteInteger(const QuadooVM::QVARIANT* value, INT* result)
	{
		HRESULT hr;
		LONGLONG number;
		Check(VariantInteger(value,&number));
		CheckIf(number < INT_MIN || number > INT_MAX,DISP_E_OVERFLOW);
		*result = static_cast<INT>(number);
	Cleanup:
		return hr;
	}

	enum SpriteMethod
	{
		SpriteDraw,
		SpriteLoadFile,
		SpriteLoadPng,
		SpriteLoadPixels,
		SpriteClear
	};

	struct SpriteMethodName
	{
		PCWSTR text;
		UINT length;
		SpriteMethod method;
		UINT minimum;
		UINT maximum;
	};
	static const SpriteMethodName c_rgSpriteMethods[] =
	{
		{SLP(L"Draw"),SpriteDraw,2,2},
		{SLP(L"LoadFile"),SpriteLoadFile,1,1},
		{SLP(L"LoadPng"),SpriteLoadPng,1,1},
		{SLP(L"LoadPixels"),SpriteLoadPixels,3,5},
		{SLP(L"Clear"),SpriteClear,0,0}
	};

	class CSpriteObject : public CBaseUnknown, public CQuadooObjectImpl
	{
		TStackRef<IQuadooCanvas> m_canvas;
		TStackRef<CSpriteImage> m_image;
		DWORD m_thread;
		BOOL m_busy;

		HRESULT CheckThread()
		{
			return m_thread == GetCurrentThreadId() ? S_OK : QuadooForm::EWrongThread;
		}

		HRESULT Draw(QuadooVM::QVPARAMS* parameters)
		{
			HRESULT hr;
			INT x;
			INT y;
			HDC dc = NULL;
			HBITMAP bitmap = NULL;
			SIF_SURFACE* surface = NULL;
			DIBSECTION section = {0};
			BOOL changed;
			CheckIf(parameters->cArgs != 2,DISP_E_BADPARAMCOUNT);
			Check(SpriteInteger(SpriteArgument(parameters,0),&x));
			Check(SpriteInteger(SpriteArgument(parameters,1),&y));
			CheckIf(!m_image,QuadooForm::EInvalidState);
			// No script calls, binary-provider callbacks, or message pumping from
			// acquisition through the final CPU write. All outputs stay local.
			Check(m_canvas->GetSurface(&dc,&bitmap,&surface));
			CheckIf(!dc || !bitmap || !surface,E_UNEXPECTED);
			CheckIf(GetObjectW(bitmap,sizeof(section),&section) != sizeof(section) ||
				GetCurrentObject(dc,OBJ_BITMAP) != bitmap || !section.dsBm.bmBits ||
				section.dsBm.bmBits != surface->pbSurface || section.dsBm.bmBitsPixel != 32 ||
				section.dsBm.bmPlanes != 1 || section.dsBm.bmWidth != surface->xSize ||
				section.dsBm.bmHeight != surface->ySize || section.dsBm.bmWidthBytes != surface->lPitch ||
				section.dsBmih.biCompression != BI_RGB,E_UNEXPECTED);
			CheckIf(!GdiFlush(),E_FAIL);
			Check(m_image->Composite(surface,x,y,&changed));
			if(changed)
			{
				Check(m_canvas->Invalidate());
			}
			hr = S_OK;
		Cleanup:
			return hr;
		}

		HRESULT Load(QuadooVM::QVPARAMS* parameters, SpriteMethod method)
		{
			HRESULT hr;
			TStackRef<CSpriteImage> candidate;
			TStackRef<ILockableStream> binary;
			BYTE* bytes = NULL;
			DWORD count = 0;
			BOOL locked = FALSE;
			SIZE size;
			Check(m_canvas->GetPixelSize(&size));
			candidate.Attach(__new CSpriteImage);
			CheckAlloc(candidate);
			if(method == SpriteLoadFile)
			{
				CheckIf(SpriteArgument(parameters,0)->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
				Check(candidate->LoadFile(SpriteArgument(parameters,0)->rstrVal));
			}
			else
			{
				QuadooVM::QVARIANT* input = SpriteArgument(parameters,method == SpriteLoadPixels ? 2 : 0);
				INT width = 0;
				INT height = 0;
				INT stride = 0;
				SpritePixelFormat format = SpriteRGB24;
				if(method == SpriteLoadPixels)
				{
					Check(SpriteInteger(SpriteArgument(parameters,0),&width));
					Check(SpriteInteger(SpriteArgument(parameters,1),&height));
					CheckIf(width <= 0 || height <= 0,E_INVALIDARG);
					if(parameters->cArgs >= 4)
					{
						QuadooVM::QVARIANT* name = SpriteArgument(parameters,3);
						CheckIf(name->eType != QuadooVM::String,DISP_E_TYPEMISMATCH);
						if(SpriteNameEquals(name->rstrVal,SLP(L"RGBA32")))
						{
							format = SpriteRGBA32;
						}
						else if(SpriteNameEquals(name->rstrVal,SLP(L"BGRA32")))
						{
							format = SpriteBGRA32;
						}
						else
						{
							CheckIf(!SpriteNameEquals(name->rstrVal,SLP(L"RGB24")),E_INVALIDARG);
						}
					}
					if(parameters->cArgs == 5)
					{
						Check(SpriteInteger(SpriteArgument(parameters,4),&stride));
						CheckIf(stride <= 0,E_INVALIDARG);
					}
				}
				CheckIf(input->eType != QuadooVM::Object || !input->pObject,DISP_E_TYPEMISMATCH);
				Check(input->pObject->QueryInterface(&binary));
				Check(binary->Lock(&bytes,&count));
				locked = TRUE;
				CheckIf(!bytes || !count,E_INVALIDARG);
				if(method == SpriteLoadPixels)
				{
					Check(candidate->LoadPixels(width,height,bytes,count,format,stride));
				}
				else
				{
					Check(candidate->LoadPng(bytes,count));
				}
				binary->Unlock();
				locked = FALSE;
			}
			// A third-party binary provider can reenter while locking/unlocking.
			// Commit only after it returns and the associated Canvas is still live.
			Check(m_canvas->GetPixelSize(&size));
			m_image = candidate;
		Cleanup:
			if(locked)
			{
				binary->Unlock();
			}
			return hr;
		}

	public:
		IMP_BASE_UNKNOWN
		BEGIN_UNK_MAP
			UNK_INTERFACE(IQuadooObject)
		END_UNK_MAP

		CSpriteObject(IQuadooCanvas* canvas) : m_canvas(canvas), m_thread(GetCurrentThreadId()), m_busy(FALSE)
		{
			InterlockedIncrement(&s_spriteObjects);
		}

		~CSpriteObject()
		{
			InterlockedDecrement(&s_spriteObjects);
		}

		STDMETHOD(Invoke)(IQuadooVM*, RSTRING name, QuadooVM::QVPARAMS* parameters, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			TStackRef<CSpriteObject> keepAlive(this);
			BOOL entered = FALSE;
			CheckIf(!result,E_POINTER);
			result->eType = QuadooVM::Null;
			Check(CheckThread());
			CheckIf(m_busy,QuadooForm::EInvalidState);
			CheckIf(!parameters || (parameters->cArgs && !parameters->pqvArgs),E_POINTER);
			m_busy = TRUE;
			entered = TRUE;
			if(!name)
			{
				// INVOKE_INDIRECT arrives here directly. No member-name lookup.
				Check(Draw(parameters));
			}
			else
			{
				const SpriteMethodName* method = NULL;
				CheckIf(!RStrIsWide(name),E_INVALIDARG);
				for(UINT n = 0; n < ARRAYSIZE(c_rgSpriteMethods); n++)
				{
					if(RStrLen(name) == static_cast<INT>(c_rgSpriteMethods[n].length) &&
						!memcmp(RStrToWide(name),c_rgSpriteMethods[n].text,c_rgSpriteMethods[n].length * sizeof(WCHAR)))
					{
						method = c_rgSpriteMethods + n;
						break;
					}
				}
				CheckIf(!method,DISP_E_MEMBERNOTFOUND);
				CheckIf(parameters->cArgs < method->minimum || parameters->cArgs > method->maximum,DISP_E_BADPARAMCOUNT);
				if(method->method == SpriteDraw)
				{
					Check(Draw(parameters));
				}
				else if(method->method == SpriteClear)
				{
					m_image.Release();
				}
				else
				{
					Check(Load(parameters,method->method));
				}
			}
			hr = S_OK;
		Cleanup:
			if(entered)
			{
				m_busy = FALSE;
			}
			return hr;
		}

		STDMETHOD(GetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT* result)
		{
			HRESULT hr;
			CheckIf(!result,E_POINTER);
			result->eType = QuadooVM::Null;
			Check(CheckThread());
			CheckIf(m_busy,QuadooForm::EInvalidState);
			CheckIf(!name || !RStrIsWide(name),E_INVALIDARG);
			if(SpriteNameEquals(name,SLP(L"Width")) || SpriteNameEquals(name,SLP(L"Height")))
			{
				result->eType = QuadooVM::I4;
				result->lVal = !m_image ? 0 : SpriteNameEquals(name,SLP(L"Width")) ? m_image->width : m_image->height;
			}
			else
			{
				CheckIf(!SpriteNameEquals(name,SLP(L"IsLoaded")),DISP_E_MEMBERNOTFOUND);
				result->eType = QuadooVM::Bool;
				result->fVal = m_image != NULL;
			}
			hr = S_OK;
		Cleanup:
			return hr;
		}

		STDMETHOD(SetProperty)(IQuadooVM*, RSTRING name, QuadooVM::QVARIANT*)
		{
			HRESULT hr;
			Check(CheckThread());
			CheckIf(!name || !RStrIsWide(name),E_INVALIDARG);
			hr = SpriteNameEquals(name,SLP(L"Width")) || SpriteNameEquals(name,SLP(L"Height")) ||
				SpriteNameEquals(name,SLP(L"IsLoaded")) ? E_ACCESSDENIED : DISP_E_MEMBERNOTFOUND;
		Cleanup:
			return hr;
		}
	};

	HRESULT CreateSpriteObject(IQuadooCanvas* canvas, IQuadooObject** object)
	{
		HRESULT hr;
		SIZE size;
		TStackRef<CSpriteObject> sprite;
		CheckIf(!object,E_POINTER);
		*object = NULL;
		CheckIf(!canvas,E_POINTER);
		Check(canvas->GetPixelSize(&size));
		sprite.Attach(__new CSpriteObject(canvas));
		CheckAlloc(sprite);
		*object = sprite.Detach();
	Cleanup:
		return hr;
	}
}
